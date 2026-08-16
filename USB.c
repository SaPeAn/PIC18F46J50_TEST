#include "USB.h"
#include "ringbuf.h"

/* ------------------------------------------------------------------------ *
 *  Константы протокола USB
 * ------------------------------------------------------------------------ */

#define EP0_SIZE                8
#define EP1_SIZE                8

// BDnSTAT: биты, когда буфером владеет SIE (UOWN = 1)
#define _UOWN                   0x80
#define _DTS                    0x40
#define _DTSEN                  0x08
#define _BSTALL                 0x04

// BDnSTAT: PID токена, когда буфер вернулся процессору (UOWN = 0)
#define BD_PID(stat)            (((stat) >> 2) & 0x0F)
#define PID_OUT                 0x01
#define PID_IN                  0x09
#define PID_SETUP               0x0D

// Стандартные запросы
#define REQ_GET_STATUS          0x00
#define REQ_CLEAR_FEATURE       0x01
#define REQ_SET_FEATURE         0x03
#define REQ_SET_ADDRESS         0x05
#define REQ_GET_DESCRIPTOR      0x06
#define REQ_GET_CONFIGURATION   0x08
#define REQ_SET_CONFIGURATION   0x09
#define REQ_GET_INTERFACE       0x0A
#define REQ_SET_INTERFACE       0x0B

// Запросы класса HID
#define HID_GET_REPORT          0x01
#define HID_GET_IDLE            0x02
#define HID_GET_PROTOCOL        0x03
#define HID_SET_REPORT          0x09
#define HID_SET_IDLE            0x0A
#define HID_SET_PROTOCOL        0x0B

// Типы дескрипторов
#define DSC_DEVICE              0x01
#define DSC_CONFIG              0x02
#define DSC_STRING              0x03
#define DSC_HID                 0x21
#define DSC_REPORT              0x22

#define REQ_TYPE(bm)            ((bm) & 0x60)   // 0x00 - standard, 0x20 - class
#define REQ_DIR_IN(bm)          ((bm) & 0x80)

// Состояния устройства
#define ST_DETACHED             0
#define ST_DEFAULT              1
#define ST_ADDRESSED            2
#define ST_CONFIGURED           3

/*
 * Стадии управляющей передачи на EP0.
 *
 * Инвариант: BD EP0 OUT взводится ровно один раз за передачу — в конце
 * ep0_setup() (для стадий IN_DATA и STATUS_IN) либо самой стадией OUT_DATA,
 * и далее перевзводится только после того, как очередной пакет OUT реально
 * принят. Повторное взведение буфера, которым уже владеет SIE, ломает
 * управляющий канал, поэтому статусные стадии разведены явно.
 */
#define CTRL_IDLE               0
#define CTRL_IN_DATA            1   // отдаём данные хосту
#define CTRL_OUT_DATA           2   // принимаем данные от хоста
#define CTRL_STATUS_IN          3   // наш пустой IN-пакет взведён как статус
#define CTRL_STATUS_OUT         4   // данные отданы, ждём статусный OUT от хоста

// Индексы в таблице дескрипторов буферов (режим без ping-pong, PPB = 00)
#define BD_EP0_OUT              0
#define BD_EP0_IN               1
#define BD_EP1_OUT              2
#define BD_EP1_IN               3

// Флаг "символ требует Shift" в таблице ASCII -> HID Usage ID
#define KBD_SHIFT               0x80

/* ------------------------------------------------------------------------ *
 *  Память USB (банки 4..7, 0x400...0x7FF)
 * ------------------------------------------------------------------------ */

typedef struct {
  uint8_t STAT;
  uint8_t CNT;
  uint8_t ADRL;
  uint8_t ADRH;
} BD_t;

// Если используемая версия XC8 не понимает __at(), замените на "@ 0x0400" и т.д.
volatile BD_t   BDT[4]                 __at(0x0400);
volatile uint8_t ep0_out_buf[EP0_SIZE] __at(0x0410);
volatile uint8_t ep0_in_buf[EP0_SIZE]  __at(0x0418);
volatile uint8_t ep1_in_buf[EP1_SIZE]  __at(0x0420);

/* ------------------------------------------------------------------------ *
 *  Дескрипторы
 * ------------------------------------------------------------------------ */

/*
 * ВНИМАНИЕ: VID/PID ниже — демонстрационные идентификаторы Microchip из MLA
 * (для отладочных плат). Для любого серийного изделия их обязательно нужно
 * заменить на собственные, полученные в USB-IF.
 */
#define USB_VID                 0x04D8
#define USB_PID                 0x003F

static const uint8_t dev_desc[18] = {
  18,                     // bLength
  DSC_DEVICE,             // bDescriptorType
  0x00, 0x02,             // bcdUSB = 2.00
  0x00,                   // bDeviceClass  (определяется интерфейсом)
  0x00,                   // bDeviceSubClass
  0x00,                   // bDeviceProtocol
  EP0_SIZE,               // bMaxPacketSize0
  (uint8_t)(USB_VID & 0xFF), (uint8_t)(USB_VID >> 8),
  (uint8_t)(USB_PID & 0xFF), (uint8_t)(USB_PID >> 8),
  0x00, 0x01,             // bcdDevice = 1.00
  0x01,                   // iManufacturer
  0x02,                   // iProduct
  0x00,                   // iSerialNumber (нет)
  0x01                    // bNumConfigurations
};

#define CFG_TOTAL_LEN   34
#define REPORT_DESC_LEN 63

static const uint8_t cfg_desc[CFG_TOTAL_LEN] = {
  /* --- Configuration ------------------------------------------------- */
  9, DSC_CONFIG,
  CFG_TOTAL_LEN, 0x00,    // wTotalLength
  0x01,                   // bNumInterfaces
  0x01,                   // bConfigurationValue
  0x00,                   // iConfiguration
  0x80,                   // bmAttributes: питание от шины, без remote wakeup
  50,                     // bMaxPower = 100 мА

  /* --- Interface ----------------------------------------------------- */
  9, 0x04,
  0x00,                   // bInterfaceNumber
  0x00,                   // bAlternateSetting
  0x01,                   // bNumEndpoints
  0x03,                   // bInterfaceClass    = HID
  0x01,                   // bInterfaceSubClass = Boot Interface
  0x01,                   // bInterfaceProtocol = Keyboard
  0x00,                   // iInterface

  /* --- HID ----------------------------------------------------------- */
  9, DSC_HID,
  0x11, 0x01,             // bcdHID = 1.11
  0x00,                   // bCountryCode
  0x01,                   // bNumDescriptors
  DSC_REPORT,             // bDescriptorType
  REPORT_DESC_LEN, 0x00,  // wDescriptorLength

  /* --- Endpoint 1 IN ------------------------------------------------- */
  7, 0x05,
  0x81,                   // bEndpointAddress = EP1 IN
  0x03,                   // bmAttributes = Interrupt
  EP1_SIZE, 0x00,         // wMaxPacketSize
  10                      // bInterval = 10 мс (~50 симв/с с учётом
                          //              нажатия + отпускания)
};

// Смещение HID-дескриптора внутри cfg_desc (для GET_DESCRIPTOR type 0x21)
#define HID_DESC_OFFSET  18

/* Стандартный boot-отчёт клавиатуры: 8 байт IN, 1 байт OUT (светодиоды). */
static const uint8_t report_desc[REPORT_DESC_LEN] = {
  0x05, 0x01,             // Usage Page (Generic Desktop)
  0x09, 0x06,             // Usage (Keyboard)
  0xA1, 0x01,             // Collection (Application)
  0x05, 0x07,             //   Usage Page (Keyboard/Keypad)
  0x19, 0xE0,             //   Usage Minimum (LeftControl)
  0x29, 0xE7,             //   Usage Maximum (Right GUI)
  0x15, 0x00,             //   Logical Minimum (0)
  0x25, 0x01,             //   Logical Maximum (1)
  0x75, 0x01,             //   Report Size (1)
  0x95, 0x08,             //   Report Count (8)
  0x81, 0x02,             //   Input (Data,Var,Abs)   -- байт модификаторов
  0x95, 0x01,             //   Report Count (1)
  0x75, 0x08,             //   Report Size (8)
  0x81, 0x01,             //   Input (Cnst)           -- зарезервированный байт
  0x95, 0x05,             //   Report Count (5)
  0x75, 0x01,             //   Report Size (1)
  0x05, 0x08,             //   Usage Page (LEDs)
  0x19, 0x01,             //   Usage Minimum (Num Lock)
  0x29, 0x05,             //   Usage Maximum (Kana)
  0x91, 0x02,             //   Output (Data,Var,Abs)  -- светодиоды
  0x95, 0x01,             //   Report Count (1)
  0x75, 0x03,             //   Report Size (3)
  0x91, 0x01,             //   Output (Cnst)          -- выравнивание
  0x95, 0x06,             //   Report Count (6)
  0x75, 0x08,             //   Report Size (8)
  0x15, 0x00,             //   Logical Minimum (0)
  0x25, 0x65,             //   Logical Maximum (101)
  0x05, 0x07,             //   Usage Page (Keyboard/Keypad)
  0x19, 0x00,             //   Usage Minimum (0)
  0x29, 0x65,             //   Usage Maximum (101)
  0x81, 0x00,             //   Input (Data,Ary)       -- 6 кодов клавиш
  0xC0                    // End Collection
};

static const uint8_t str0_desc[4] = { 4, DSC_STRING, 0x09, 0x04 };  // en-US

static const uint8_t str1_desc[24] = {                              // "PIC18F46J50"
  24, DSC_STRING,
  'P',0, 'I',0, 'C',0, '1',0, '8',0, 'F',0, '4',0, '6',0,
  'J',0, '5',0, '0',0
};

static const uint8_t str2_desc[34] = {                              // "PIC HID Keyboard"
  34, DSC_STRING,
  'P',0, 'I',0, 'C',0, ' ',0, 'H',0, 'I',0, 'D',0, ' ',0,
  'K',0, 'e',0, 'y',0, 'b',0, 'o',0, 'a',0, 'r',0, 'd',0
};

/* ------------------------------------------------------------------------ *
 *  ASCII -> HID Usage ID (раскладка US). Старший бит = требуется Shift.
 *  Значение 0 означает "символ не печатается" и молча пропускается.
 * ------------------------------------------------------------------------ */
static const uint8_t ascii2usage[128] = {
/* 0x00 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x08 */ 0x2A, 0x2B, 0x28, 0x00, 0x00, 0x28, 0x00, 0x00,  // BS TAB LF - - CR
/* 0x10 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x18 */ 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x00,  // ESC
/* 0x20 */ 0x2C, 0x9E, 0xB4, 0xA0, 0xA1, 0xA2, 0xA4, 0x34,  //   ! " # $ % & '
/* 0x28 */ 0xA6, 0xA7, 0xA5, 0xAE, 0x36, 0x2D, 0x37, 0x38,  // ( ) * + , - . /
/* 0x30 */ 0x27, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,  // 0 1 2 3 4 5 6 7
/* 0x38 */ 0x25, 0x26, 0xB3, 0x33, 0xB6, 0x2E, 0xB7, 0xB8,  // 8 9 : ; < = > ?
/* 0x40 */ 0x9F, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A,  // @ A B C D E F G
/* 0x48 */ 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92,  // H I J K L M N O
/* 0x50 */ 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A,  // P Q R S T U V W
/* 0x58 */ 0x9B, 0x9C, 0x9D, 0x2F, 0x31, 0x30, 0xA3, 0xAD,  // X Y Z [ \ ] ^ _
/* 0x60 */ 0x35, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,  // ` a b c d e f g
/* 0x68 */ 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,  // h i j k l m n o
/* 0x70 */ 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,  // p q r s t u v w
/* 0x78 */ 0x1B, 0x1C, 0x1D, 0xAF, 0xB1, 0xB0, 0xB5, 0x4C   // x y z { | } ~ DEL
};

/* ------------------------------------------------------------------------ *
 *  Состояние модуля
 * ------------------------------------------------------------------------ */

static volatile uint8_t usb_state    = ST_DETACHED;
static volatile uint8_t kbd_leds     = 0;

// Управляющая передача на EP0
static uint8_t  ctrl_stage  = CTRL_IDLE;
static const uint8_t* ctrl_rom = 0;   // источник данных во флеш-памяти
static uint8_t  ctrl_ram[8];          // источник данных в ОЗУ
static uint8_t  ctrl_in_rom = 0;      // 1 - читаем из ctrl_rom, 0 - из ctrl_ram
static uint16_t ctrl_pos    = 0;
static uint16_t ctrl_len    = 0;
static uint8_t  ctrl_zlp    = 0;      // нужен завершающий пустой пакет
static uint8_t  ctrl_dts    = 1;
static uint8_t  pending_addr = 0;     // адрес, применяемый после статусной стадии

// Очередь "нажатий": по 2 байта на элемент — {usage, modifiers}
static RINGBUF_t kbd_rb;
static uint8_t   kbd_rb_mem[USB_KBD_BUFLEN * 2];
static uint8_t   kbd_release = 0;     // следующий отчёт — "все клавиши отпущены"
static uint8_t   kbd_dts     = 0;

/* ------------------------------------------------------------------------ *
 *  Низкоуровневые помощники
 * ------------------------------------------------------------------------ */

static void bd_set_addr(uint8_t idx, volatile uint8_t* buf)
{
  uint16_t a = (uint16_t)buf;
  BDT[idx].ADRL = (uint8_t)(a & 0xFF);
  BDT[idx].ADRH = (uint8_t)(a >> 8);
}

/* Подставить EP0 OUT под приём следующего пакета (SETUP либо статус). */
static void ep0_arm_out(void)
{
  BDT[BD_EP0_OUT].CNT  = EP0_SIZE;
  bd_set_addr(BD_EP0_OUT, ep0_out_buf);
  BDT[BD_EP0_OUT].STAT = _UOWN;      // без DTSEN: принимаем любой toggle
}

/* Отправить очередную порцию данных управляющей передачи. */
static void ep0_send_chunk(void)
{
  uint8_t n = (ctrl_len > EP0_SIZE) ? EP0_SIZE : (uint8_t)ctrl_len;

  for(uint8_t i = 0; i < n; i++)
    ep0_in_buf[i] = ctrl_in_rom ? ctrl_rom[ctrl_pos + i] : ctrl_ram[ctrl_pos + i];

  ctrl_pos += n;
  ctrl_len -= n;

  BDT[BD_EP0_IN].CNT  = n;
  bd_set_addr(BD_EP0_IN, ep0_in_buf);
  BDT[BD_EP0_IN].STAT = ctrl_dts ? (_UOWN | _DTS | _DTSEN) : (_UOWN | _DTSEN);
  ctrl_dts ^= 1;
}

/* Начать стадию данных IN (источник — флеш). */
static void ctrl_send_rom(const uint8_t* p, uint16_t len, uint16_t wLength)
{
  if(len > wLength) len = wLength;
  ctrl_rom    = p;
  ctrl_in_rom = 1;
  ctrl_pos    = 0;
  ctrl_len    = len;
  // Если отдаём меньше, чем просил хост, и длина кратна размеру пакета,
  // передачу нужно завершить пустым пакетом.
  ctrl_zlp    = (len < wLength) && (len != 0) && ((len % EP0_SIZE) == 0);
  ctrl_stage  = CTRL_IN_DATA;
  ep0_send_chunk();
}

/* Начать стадию данных IN (источник — ctrl_ram). */
static void ctrl_send_ram(uint16_t len, uint16_t wLength)
{
  if(len > wLength) len = wLength;
  ctrl_in_rom = 0;
  ctrl_pos    = 0;
  ctrl_len    = len;
  ctrl_zlp    = 0;                   // ответы из ОЗУ всегда короче EP0_SIZE
  ctrl_stage  = CTRL_IN_DATA;
  ep0_send_chunk();
}

/* Запрос без стадии данных: отправляем статусный пустой пакет. */
static void ctrl_ack(void)
{
  ctrl_stage = CTRL_STATUS_IN;
  ctrl_len   = 0;
  BDT[BD_EP0_IN].CNT  = 0;
  bd_set_addr(BD_EP0_IN, ep0_in_buf);
  BDT[BD_EP0_IN].STAT = _UOWN | _DTS | _DTSEN;   // статус всегда DATA1
}

/* Нераспознанный запрос — STALL на обоих направлениях EP0. */
static void ctrl_stall(void)
{
  ctrl_stage = CTRL_IDLE;
  BDT[BD_EP0_OUT].CNT  = EP0_SIZE;
  bd_set_addr(BD_EP0_OUT, ep0_out_buf);
  BDT[BD_EP0_OUT].STAT = _UOWN | _BSTALL;
  BDT[BD_EP0_IN].CNT   = 0;
  BDT[BD_EP0_IN].STAT  = _UOWN | _BSTALL;
}

/* ------------------------------------------------------------------------ *
 *  Разбор SETUP-пакета
 * ------------------------------------------------------------------------ */

static void ep0_setup(void)
{
  uint8_t bmRequestType = ep0_out_buf[0];
  uint8_t bRequest      = ep0_out_buf[1];
  uint8_t wValueL       = ep0_out_buf[2];
  uint8_t wValueH       = ep0_out_buf[3];
  uint16_t wLength      = ((uint16_t)ep0_out_buf[7] << 8) | ep0_out_buf[6];

  // SETUP отменяет всё, что было в работе на EP0
  BDT[BD_EP0_OUT].STAT = 0;
  BDT[BD_EP0_IN].STAT  = 0;
  ctrl_stage  = CTRL_IDLE;
  ctrl_len    = 0;
  ctrl_pos    = 0;
  ctrl_zlp    = 0;
  ctrl_dts    = 1;             // первый пакет после SETUP — всегда DATA1
  UCONbits.PKTDIS = 0;         // SIE приостановил обработку — возобновляем

  if(REQ_TYPE(bmRequestType) == 0x00)          /* --- стандартные --- */
  {
    switch(bRequest)
    {
      case REQ_GET_DESCRIPTOR:
        switch(wValueH)
        {
          case DSC_DEVICE:
            ctrl_send_rom(dev_desc, sizeof(dev_desc), wLength);
            return;
          case DSC_CONFIG:
            ctrl_send_rom(cfg_desc, sizeof(cfg_desc), wLength);
            return;
          case DSC_HID:
            ctrl_send_rom(&cfg_desc[HID_DESC_OFFSET], 9, wLength);
            return;
          case DSC_REPORT:
            ctrl_send_rom(report_desc, sizeof(report_desc), wLength);
            return;
          case DSC_STRING:
            if(wValueL == 0) { ctrl_send_rom(str0_desc, sizeof(str0_desc), wLength); return; }
            if(wValueL == 1) { ctrl_send_rom(str1_desc, sizeof(str1_desc), wLength); return; }
            if(wValueL == 2) { ctrl_send_rom(str2_desc, sizeof(str2_desc), wLength); return; }
            break;
          default:
            break;
        }
        break;

      case REQ_SET_ADDRESS:
        // Адрес вступает в силу только после статусной стадии
        pending_addr = wValueL;
        ctrl_ack();
        return;

      case REQ_SET_CONFIGURATION:
        if(wValueL == 0)
        {
          usb_state = ST_ADDRESSED;
          UEP1 = 0x00;
        }
        else
        {
          // EP1: EPHSHK = 1, EPCONDIS = 1 (только data), EPINEN = 1
          UEP1 = 0x1A;
          BDT[BD_EP1_IN].STAT = 0;
          BDT[BD_EP1_IN].CNT  = 0;
          bd_set_addr(BD_EP1_IN, ep1_in_buf);
          kbd_dts     = 0;
          kbd_release = 0;
          usb_state   = ST_CONFIGURED;
        }
        ctrl_ack();
        return;

      case REQ_GET_CONFIGURATION:
        ctrl_ram[0] = (usb_state == ST_CONFIGURED) ? 1 : 0;
        ctrl_send_ram(1, wLength);
        return;

      case REQ_GET_STATUS:
        ctrl_ram[0] = 0x00;      // питание от шины, remote wakeup выключен
        ctrl_ram[1] = 0x00;
        ctrl_send_ram(2, wLength);
        return;

      case REQ_GET_INTERFACE:
        ctrl_ram[0] = 0x00;
        ctrl_send_ram(1, wLength);
        return;

      case REQ_SET_INTERFACE:
      case REQ_CLEAR_FEATURE:
      case REQ_SET_FEATURE:
        ctrl_ack();
        return;

      default:
        break;
    }
  }
  else if(REQ_TYPE(bmRequestType) == 0x20)     /* --- класса HID --- */
  {
    switch(bRequest)
    {
      case HID_SET_IDLE:
      case HID_SET_PROTOCOL:
        ctrl_ack();
        return;

      case HID_GET_IDLE:
        ctrl_ram[0] = 0x00;
        ctrl_send_ram(1, wLength);
        return;

      case HID_GET_PROTOCOL:
        ctrl_ram[0] = 0x01;      // report protocol
        ctrl_send_ram(1, wLength);
        return;

      case HID_GET_REPORT:
        for(uint8_t i = 0; i < 8; i++) ctrl_ram[i] = 0;
        ctrl_send_ram(8, wLength);
        return;

      case HID_SET_REPORT:
        // Хост шлёт состояние светодиодов. Принимаем стадию данных OUT.
        if(wLength)
        {
          ctrl_stage = CTRL_OUT_DATA;
          BDT[BD_EP0_OUT].CNT  = EP0_SIZE;
          bd_set_addr(BD_EP0_OUT, ep0_out_buf);
          BDT[BD_EP0_OUT].STAT = _UOWN | _DTS | _DTSEN;
        }
        else ctrl_ack();
        return;

      default:
        break;
    }
  }

  ctrl_stall();
}

/* ------------------------------------------------------------------------ *
 *  Обработка завершённых транзакций
 * ------------------------------------------------------------------------ */

static void ep0_out_done(void)
{
  if(BD_PID(BDT[BD_EP0_OUT].STAT) == PID_SETUP)
  {
    ep0_setup();
    // OUT_DATA взвела приёмный буфер сама, а после STALL (CTRL_IDLE) он
    // намеренно оставлен с BSTALL — в этих двух случаях не трогаем.
    if(ctrl_stage == CTRL_IN_DATA || ctrl_stage == CTRL_STATUS_IN) ep0_arm_out();
    return;
  }

  if(ctrl_stage == CTRL_OUT_DATA)
  {
    // Единственная поддерживаемая передача OUT — SET_REPORT (светодиоды)
    if(BDT[BD_EP0_OUT].CNT >= 1) kbd_leds = ep0_out_buf[0];
    ctrl_ack();                  // подтверждаем статусным пустым пакетом
  }
  else
  {
    // Статусная стадия OUT после передачи данных хосту
    ctrl_stage = CTRL_IDLE;
  }
  ep0_arm_out();
}

static void ep0_in_done(void)
{
  if(ctrl_stage == CTRL_IN_DATA)
  {
    if(ctrl_len > 0)
    {
      ep0_send_chunk();
      return;
    }
    if(ctrl_zlp)
    {
      ctrl_zlp = 0;
      ep0_send_chunk();          // отправит пакет нулевой длины
      return;
    }
    // Данные отданы полностью, дальше хост пришлёт статусный OUT.
    // Приёмный буфер уже взведён в ep0_out_done() — повторно не трогаем.
    ctrl_stage = CTRL_STATUS_OUT;
    return;
  }

  if(ctrl_stage == CTRL_STATUS_IN)
  {
    // Адрес назначается только теперь, после подтверждения SET_ADDRESS
    if(pending_addr)
    {
      UADDR = pending_addr;
      usb_state = ST_ADDRESSED;
      pending_addr = 0;
    }
    ctrl_stage = CTRL_IDLE;      // EP0 OUT уже ждёт следующий SETUP
  }
}

/* ------------------------------------------------------------------------ *
 *  Формирование отчётов клавиатуры (EP1 IN)
 * ------------------------------------------------------------------------ */

static void kbd_arm_report(void)
{
  BDT[BD_EP1_IN].CNT = EP1_SIZE;
  bd_set_addr(BD_EP1_IN, ep1_in_buf);
  BDT[BD_EP1_IN].STAT = kbd_dts ? (_UOWN | _DTS | _DTSEN) : (_UOWN | _DTSEN);
  kbd_dts ^= 1;
}

/*
 * Каждый символ печатается двумя отчётами: "клавиша нажата" и "клавиша
 * отпущена". Без второго отчёта хост воспримет ввод как удержание клавиши,
 * а два одинаковых символа подряд сольются в один.
 *
 * Вызывается только из USB-прерывания, поэтому дополнительная синхронизация
 * доступа к kbd_rb здесь не нужна.
 */
static void kbd_service(void)
{
  uint16_t avail = 0;
  uint16_t key   = 0;

  if(usb_state != ST_CONFIGURED) return;
  if(BDT[BD_EP1_IN].STAT & _UOWN) return;   // предыдущий отчёт ещё не забран

  for(uint8_t i = 0; i < EP1_SIZE; i++) ep1_in_buf[i] = 0;

  if(kbd_release)
  {
    kbd_release = 0;                        // отчёт "всё отпущено" — уже нули
    kbd_arm_report();
    return;
  }

  RingBuf_Available(&avail, &kbd_rb);
  if(avail == 0) return;
  if(RingBuf_DataRead(&key, 1, &kbd_rb) != RINGBUF_OK) return;

  ep1_in_buf[0] = (uint8_t)(key >> 8);      // модификаторы
  ep1_in_buf[2] = (uint8_t)(key & 0xFF);    // код клавиши
  kbd_release = 1;
  kbd_arm_report();
}

/* ------------------------------------------------------------------------ *
 *  Обработчик прерывания USB (регистрируется как low-priority колбэк)
 * ------------------------------------------------------------------------ */

static void usb_reset(void)
{
  // Сбрасываем очередь транзакций USTAT
  while(UIRbits.TRNIF) UIRbits.TRNIF = 0;

  UEP1 = 0x00;
  UADDR = 0x00;
  UIR   = 0x00;
  UEIR  = 0x00;

  UCONbits.PPBRST = 1;
  UCONbits.PPBRST = 0;
  UCONbits.PKTDIS = 0;

  // EP0: EPHSHK = 1, EPOUTEN = 1, EPINEN = 1, управляющие передачи разрешены
  UEP0 = 0x16;

  BDT[BD_EP0_IN].STAT  = 0;
  BDT[BD_EP1_IN].STAT  = 0;
  ep0_arm_out();

  ctrl_stage   = CTRL_IDLE;
  pending_addr = 0;
  kbd_release  = 0;
  kbd_dts      = 0;
  kbd_leds     = 0;
  RingBuf_Clear(&kbd_rb);       // не выплёвывать в новый сеанс старую очередь

  usb_state = ST_DEFAULT;
}

/* Вызывается диспетчером system.c при взведённых USBIE и USBIF. */
void USB_isr(void)
{
  PIR2bits.USBIF = 0;

  // Выход из suspend по активности на шине
  if(UIRbits.ACTVIF && UIEbits.ACTVIE)
  {
    UCONbits.SUSPND = 0;
    // ACTVIF снимается только после того, как активность реально началась
    while(UIRbits.ACTVIF) UIRbits.ACTVIF = 0;
    UIEbits.ACTVIE = 0;
  }

  if(UCONbits.SUSPND) return;

  if(UIRbits.URSTIF)
  {
    usb_reset();
    UIRbits.URSTIF = 0;
  }

  if(UIRbits.IDLEIF)
  {
    UIRbits.IDLEIF = 0;
    UIEbits.ACTVIE = 1;
    UCONbits.SUSPND = 1;        // хост перевёл шину в suspend
    return;
  }

  if(UIRbits.UERRIF)
  {
    UEIR = 0x00;                // ошибки шины: чистим и продолжаем
    UIRbits.UERRIF = 0;
  }

  if(UIRbits.STALLIF)
  {
    if(UEP0bits.EPSTALL)
    {
      // Хост принял STALL — возвращаем EP0 в рабочее состояние
      if(ctrl_stage == CTRL_IDLE)
      {
        BDT[BD_EP0_IN].STAT = 0;
        ep0_arm_out();
      }
      UEP0bits.EPSTALL = 0;
    }
    UIRbits.STALLIF = 0;
  }

  if(UIRbits.SOFIF)
  {
    UIRbits.SOFIF = 0;
    kbd_service();              // раз в 1 мс подталкиваем очередь символов
  }

  // Очередь завершённых транзакций (USTAT — FIFO на 4 записи)
  while(UIRbits.TRNIF)
  {
    uint8_t stat = USTAT;
    UIRbits.TRNIF = 0;          // сброс флага продвигает FIFO

    uint8_t ep  = (stat >> 3) & 0x0F;
    uint8_t dir = (stat >> 2) & 0x01;   // 1 = IN

    if(ep == 0)
    {
      if(dir) ep0_in_done();
      else    ep0_out_done();
    }
    else if(ep == 1 && dir)
    {
      kbd_service();            // отчёт ушёл — готовим следующий
    }
  }
}

/* ------------------------------------------------------------------------ *
 *  Публичный интерфейс
 * ------------------------------------------------------------------------ */

void USB_init(void)
{
  RingBuf_Init(kbd_rb_mem, USB_KBD_BUFLEN, 2, &kbd_rb);

  usb_state = ST_DETACHED;

  UCON = 0x00;
  UIE  = 0x00;
  UEIE = 0x00;

  // UCFG: UPUEN = 1 (встроенная подтяжка), UTRDIS = 0 (встроенный
  // трансивер), FSEN = 1 (Full Speed), PPB = 00 (без ping-pong)
  UCFG = 0x14;

  UEP0 = 0x00;
  UEP1 = 0x00;

  for(uint8_t i = 0; i < 4; i++)
  {
    BDT[i].STAT = 0;
    BDT[i].CNT  = 0;
  }

  UCONbits.USBEN = 1;                 // подключаем модуль к шине

  // Ждём окончания состояния Single-Ended Zero, но не дольше 100 мс:
  // при невоткнутом кабеле SE0 держится постоянно, и безусловный цикл
  // повесил бы прошивку на старте. Подключение всё равно будет поймано
  // по прерыванию URSTIF в момент, когда кабель воткнут.
  uint32_t t0 = get_ms();
  while(UCONbits.SE0 && ((get_ms() - t0) < 100));

  UIR = 0x00;
  UIEbits.URSTIE  = 1;
  UIEbits.TRNIE   = 1;
  UIEbits.IDLEIE  = 1;
  UIEbits.STALLIE = 1;
  UIEbits.UERRIE  = 1;
  UIEbits.SOFIE   = 1;

  IPR2bits.USBIP = 0;                 // низкий приоритет — как у ADC и UART
  PIR2bits.USBIF = 0;
  // USB_isr() вызывается диспетчером system.c напрямую, регистрация не нужна
  PIE2bits.USBIE = 1;

  usb_state = ST_DEFAULT;
}

void USB_deinit(void)
{
  PIE2bits.USBIE = 0;
  UIE  = 0x00;
  UCON = 0x00;
  usb_state = ST_DETACHED;
}

uint8_t USB_is_configured(void)
{
  return (usb_state == ST_CONFIGURED) && !UCONbits.SUSPND;
}

uint8_t USB_kbd_getleds(void)
{
  return kbd_leds;
}

int16_t USB_kbd_putkey(uint8_t usage, uint8_t modifiers)
{
  uint16_t key   = ((uint16_t)modifiers << 8) | usage;
  uint16_t avail = 0;
  uint8_t  ie;

  if(usage == 0) return 0;
  if(usb_state != ST_CONFIGURED) return -1;

  // USB-прерывание низкоприоритетное: достаточно запретить именно его,
  // не трогая GIE и не мешая остальной системе.
  ie = PIE2bits.USBIE;
  PIE2bits.USBIE = 0;
  RingBuf_Available(&avail, &kbd_rb);
  if(avail >= (USB_KBD_BUFLEN - 1))
  {
    PIE2bits.USBIE = ie;
    return 0;                          // очередь переполнена, символ не потерян молча
  }
  RingBuf_DataPut(&key, 1, &kbd_rb);
  PIE2bits.USBIE = ie;

  return 1;
}

int16_t USB_kbd_putchar(char c)
{
  uint8_t code;

  if((uint8_t)c > 127) return 0;       // вне US-ASCII — печатать нечем
  code = ascii2usage[(uint8_t)c];
  if(code == 0) return 0;

  return USB_kbd_putkey(code & 0x7F,
                        (code & KBD_SHIFT) ? USB_KBD_MOD_LSHIFT : 0);
}

int16_t USB_kbd_putstring(const char* str, uint16_t Nmax)
{
  int16_t n = 0;

  if(usb_state != ST_CONFIGURED) return -1;

  for(uint16_t i = 0; i < Nmax; i++)
  {
    if(str[i] == '\0') break;
    if(USB_kbd_putchar(str[i]) <= 0) break;   // очередь кончилась
    n++;
  }
  return n;
}
