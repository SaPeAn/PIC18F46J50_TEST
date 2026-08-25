/**
 * @file dbio_fixed.c
 * @brief Исправленная версия dbio.c (см. dbio_review.md).
 *
 * ВНИМАНИЕ: одновременно с dbio.c не собирается — имена функций совпадают.
 *
 * Внесённые правки (нумерация по dbio_review.md):
 *   2.1 читается Nmax-1 байт, последний принятый байт больше не затирается '\0';
 *   2.2 длина клипуется до Nmax-1 — '\0' не выходит за границу буфера;
 *   2.3 пустая строка не приводит к отправке неинициализированного байта;
 *   2.4 при OERR/FERR приёмный FIFO вычитывается до сброса CREN, битые байты
 *       в кольцо не попадают;
 *   2.5 отправку ведёт только TXbyte_cbk — гонка за TXREG устранена;
 *   2.6 свободное место проверяется по фактической длине строки;
 *   2.7 переполнение приёмного кольца фиксируется счётчиком;
 *   2.8 отсекается Nmax < 2;
 *   3.1 критические секции сохраняют и восстанавливают GIE;
 *   3.2-3.6 static/volatile/const, проверка кодов возврата;
 *   3.7-3.8 коды возврата вынесены в enum, убраны лишние include и касты.
 */

#include "dbio_fixed.h"
#include "ringbuf.h"

#define BUFLENGTH    200
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

/* Вложенные критические секции.
   Прежний InterruptEn() безусловно ставил GIE = 1 и включал прерывания даже
   там, где они были выключены вызывающим кодом. */
#define CRIT_DECL()   uint8_t gie_state
#define CRIT_ENTER()  do { gie_state = (uint8_t)INTCONbits.GIE; InterruptDis(); } while(0)
#define CRIT_EXIT()   do { if(gie_state) InterruptEn(); } while(0)

static uint16_t dbio_strnlen(const char* str, uint16_t N)
{
  for(uint16_t i = 0; i < N; i++)
  {
    if(str[i]) continue;
    return i;
  }
  return N;
}

static void TXbyte_cbk(void)
{
  if (TX1IE && TX1IF)
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
}

static void RXbyte_cbk(void)
{
  if (RC1IE && RC1IF)
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
}

uint8_t dbio_init(void)
{
  if(RingBuf_Init(RXbuf, BUFLENGTH, 1, &RXringbuf) != RINGBUF_OK) return DBIO_INIT_BUF_ERR;
  if(RingBuf_Init(TXbuf, BUFLENGTH, 1, &TXringbuf) != RINGBUF_OK) return DBIO_INIT_BUF_ERR;
  if(sys_regiter_IRQ_clbk(RXbyte_cbk, 0)) return DBIO_INIT_CBK_ERR;
  if(sys_regiter_IRQ_clbk(TXbyte_cbk, 0)) return DBIO_INIT_CBK_ERR;
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

int16_t dbio_putstring(const char* str, uint16_t Nmax)
{
  uint16_t buf_len = 0;
  uint16_t str_len;
  CRIT_DECL();

  if(str == NULL) return DBIO_PARAM_ERR;

  /* Длина считается ДО проверки места: прежняя версия сравнивала с Nmax,
     то есть с размером буфера вызывающего, и молча отбрасывала короткие
     строки при наличии свободного места. */
  str_len = dbio_strnlen(str, Nmax);
  if(str_len == 0) return DBIO_PENDING;
  if(str_len > BUFCAPACITY) return DBIO_PARAM_ERR;

  CRIT_ENTER();
  RingBuf_Available(&buf_len, &TXringbuf);
  if((uint16_t)(buf_len + str_len) > BUFCAPACITY)
  {
    CRIT_EXIT();
    return DBIO_NO_SPACE;
  }
  RingBuf_DataPut(str, str_len, &TXringbuf);
  CRIT_EXIT();

  /* Отправку целиком ведёт TXbyte_cbk. TX1IF взведён всегда, когда TXREG
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
