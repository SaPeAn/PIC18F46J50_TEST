/**
 * @file dbio.c
 * @brief Отладочный ввод-вывод через UART1.
 *
 * Правки по результатам обзора (нумерация — по dbio_review.md):
 *   2.1 читается Nmax-1 байт, последний принятый байт больше не затирается '\0';
 *   2.2 длина клипуется до Nmax-1 — '\0' не выходит за границу буфера;
 *   2.3 пустая строка не приводит к отправке неинициализированного байта;
 *   2.4 при OERR/FERR приёмный FIFO вычитывается до сброса CREN, битые байты
 *       в кольцо не попадают;
 *   2.5 отправку ведёт только dbio_tx_isr — гонка за TXREG устранена;
 *   2.6 свободное место проверяется по фактической длине строки;
 *   2.7 переполнение приёмного кольца фиксируется счётчиком;
 *   2.8 отсекается Nmax < 2;
 *   3.1 критические секции сохраняют и восстанавливают GIE;
 *   3.2-3.6 static/volatile/const, проверка кодов возврата;
 *   3.7-3.8 коды возврата вынесены в enum, убраны лишние include и касты.
 */

#include "dbio.h"
#include "ringbuf.h"

#define BUFLENGTH    256
/* Полезная ёмкость кольца на единицу меньше физической: в ringbuf состояние
   head == tail трактуется как "пусто" (ringbuf.c:136). */
#define BUFCAPACITY  (BUFLENGTH - 1)

static RINGBUF_t RXringbuf;
static uint8_t RXbuf[BUFLENGTH] = {0};

static RINGBUF_t TXringbuf;
static uint8_t TXbuf[BUFLENGTH] = {0};

static volatile uint16_t hw_overrun_errors = 0;
static volatile uint16_t hw_framing_errors = 0;
static volatile uint16_t rx_dropped_bytes = 0;

/* Вложенные критические секции CRIT_DECL/CRIT_ENTER/CRIT_EXIT объявлены в
   system.h — прежний безусловный InterruptEn() включал прерывания даже там,
   где вызывающий код их выключил. */

static uint16_t dbio_strnlen(const char* str, uint16_t N)
{
  for(uint16_t i = 0; i < N; i++)
  {
    if(str[i]) continue;
    return i;
  }
  return N;
}

/* Вызывается диспетчером system.c, когда взведены TX1IE и TX1IF —
   проверять эту пару повторно не нужно. */
void dbio_tx_isr(void)
{
  uint16_t buf_len = 0;
  uint8_t byte;

  RingBuf_Available(&buf_len, &TXringbuf);
  if(buf_len && (RingBuf_DataRead(&byte, 1, &TXringbuf) == RINGBUF_OK))
  {
    SendByte(byte);
  }
  else TxIntDis();
}

/* Вызывается диспетчером system.c при взведённых RC1IE и RC1IF. */
void dbio_rx_isr(void)
{
  uint8_t byte;
  uint16_t buf_len = 0;

  /* Порядок операций важен: сброс CREN очищает приёмный FIFO, поэтому
     сначала вычитываем аппаратный буфер (он двухуровневый) и только затем
     перезапускаем приёмник. Раньше CREN сбрасывался первым, и последующее
     чтение пустого RCREG клало в кольцо мусорный байт. */
  if(RCSTA1bits.OERR)
  {
    (void)GetByte();
    (void)GetByte();
    RCSTA1bits.CREN = 0;
    RCSTA1bits.CREN = 1;
    hw_overrun_errors++;
    return;
  }

  /* FERR относится к байту, лежащему в вершине FIFO: чтение RCREG
     одновременно отбрасывает битый байт и обновляет флаг. */
  if(RCSTA1bits.FERR)
  {
    (void)GetByte();
    hw_framing_errors++;
    return;
  }

  byte = GetByte();

  /* RingBuf_DataPut при заполненном кольце сдвигает tail, молча затирая
     самые старые данные (ringbuf.c:90). Для строкового протокола выгоднее
     сохранить уже принятое начало строки, поэтому новый байт отбрасываем
     явно и считаем потери. */
  RingBuf_Available(&buf_len, &RXringbuf);
  if(buf_len >= BUFCAPACITY)
  {
    rx_dropped_bytes++;
    return;
  }
  RingBuf_DataPut(&byte, 1, &RXringbuf);
}

uint8_t dbio_init(void)
{
  if(RingBuf_Init(RXbuf, BUFLENGTH, 1, &RXringbuf) != RINGBUF_OK) return DBIO_INIT_BUF_ERR;
  if(RingBuf_Init(TXbuf, BUFLENGTH, 1, &TXringbuf) != RINGBUF_OK) return DBIO_INIT_BUF_ERR;
  /* dbio_rx_isr() / dbio_tx_isr() вызываются диспетчером system.c напрямую,
     регистрировать их не нужно. */
  UART1_Init();   /* включает RC1IE — отдельный RxIntEn() не нужен */
  return DBIO_INIT_OK;
}

int16_t dbio_getstring(char* str, uint16_t Nmax, uint16_t timeout)
{
  static uint32_t timetmp = 0;
  static uint16_t buf_len_prev = 0;
  uint16_t buf_len = 0;
  RINGBUF_STATUS st;
  CRIT_DECL();

  /* Nmax включает место под '\0', поэтому буфер меньше 2 байт бессмыслен. */
  if((str == NULL) || (Nmax < 2) || (Nmax > BUFLENGTH)) return DBIO_PARAM_ERR;

  CRIT_ENTER();
  RingBuf_Available(&buf_len, &RXringbuf);
  CRIT_EXIT();

  if(buf_len == 0) return DBIO_NO_DATA;

  if(buf_len_prev != buf_len)
  {
    if(buf_len > (uint16_t)(Nmax - 1))
    {
      /* Читаем ровно Nmax-1 байт: место под терминатор уже учтено.
         Раньше вычитывалось Nmax байт и последний из них терялся. */
      CRIT_ENTER();
      st = RingBuf_DataRead(str, (uint16_t)(Nmax - 1), &RXringbuf);
      CRIT_EXIT();
      if(st != RINGBUF_OK) return DBIO_NO_DATA;
      str[Nmax - 1] = '\0';
      buf_len_prev = 0;
      return (int16_t)(Nmax - 1);
    }
    /* Данные ещё поступают — перезапускаем отсчёт межсимвольной паузы. */
    buf_len_prev = buf_len;
    timetmp = get_ms();
    return DBIO_PENDING;
  }

  if((get_ms() - timetmp) > timeout)
  {
    if(buf_len > (uint16_t)(Nmax - 1)) buf_len = (uint16_t)(Nmax - 1);
    CRIT_ENTER();
    st = RingBuf_DataRead(str, buf_len, &RXringbuf);
    CRIT_EXIT();
    if(st != RINGBUF_OK) return DBIO_NO_DATA;
    str[buf_len] = '\0';
    buf_len_prev = 0;
    return (int16_t)buf_len;
  }

  /* Пауза ещё не истекла. Прежняя версия возвращала здесь -2 ("нет данных"),
     хотя данные в буфере есть; теперь -2 означает только пустой буфер. */
  return DBIO_PENDING;
}

int16_t dbio_putfstring(const char* fstr, ...)
{
  va_list args;
  uint16_t buf_len = 0;
  int str_len;
  char buf[(BUFLENGTH >> 1)];
  CRIT_DECL();

  if(fstr == NULL) return DBIO_PARAM_ERR;

  va_start(args, fstr);
  str_len = vsnprintf(buf, sizeof(buf), fstr, args);
  va_end(args);
  /* Длина считается ДО проверки места: прежняя версия сравнивала с Nmax,
     то есть с размером буфера вызывающего, и молча отбрасывала короткие
     строки при наличии свободного места. */
  if(str_len == 0) return DBIO_PENDING;
  if(str_len < 0) return DBIO_PARAM_ERR;
  if((size_t)str_len > sizeof(buf) - 1) str_len = sizeof(buf) - 1;

  CRIT_ENTER();
  RingBuf_Available(&buf_len, &TXringbuf);
  if((buf_len + (uint16_t)str_len) > BUFCAPACITY)
  {
    CRIT_EXIT();
    return DBIO_NO_SPACE;
  }
  RingBuf_DataPut(buf, (uint16_t)str_len, &TXringbuf);
  CRIT_EXIT();

  /* Отправку целиком ведёт dbio_tx_isr. TX1IF взведён всегда, когда TXREG
     свободен, поэтому включение TX1IE немедленно вызовет обработчик.
     Ручная отправка первого байта из основного кода создавала гонку за TXREG
     с обработчиком и могла отправить неинициализированный байт. */
  TxIntEn();

  return (int16_t)str_len;
}

uint16_t dbio_get_hw_overruns(void)
{
  uint16_t val;
  CRIT_DECL();
  CRIT_ENTER();          /* 16-битное чтение на PIC18 не атомарно */
  val = hw_overrun_errors;
  CRIT_EXIT();
  return val;
}

uint16_t dbio_get_hw_framing_errors(void)
{
  uint16_t val;
  CRIT_DECL();
  CRIT_ENTER();
  val = hw_framing_errors;
  CRIT_EXIT();
  return val;
}

uint16_t dbio_get_rx_dropped(void)
{
  uint16_t val;
  CRIT_DECL();
  CRIT_ENTER();
  val = rx_dropped_bytes;
  CRIT_EXIT();
  return val;
}
