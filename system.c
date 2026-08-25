#include "system.h"

/*
 * Обработчики прерываний модулей. Список источников намеренно статический:
 * перебор таблицы указателей на XC8 с -O0 обходился в ~45 тактов на каждый
 * "чужой" источник, статическая цепочка ниже — в 5 тактов (замер по
 * дизассемблеру, см. isr_dispatch_notes.md).
 *
 * Как добавить источник: объявить его обработчик в заголовке своего модуля,
 * подключить заголовок здесь и дописать одну строку в соответствующий
 * обработчик. Проверка пары "разрешение + флаг" остаётся за диспетчером,
 * внутри обработчика её дублировать не нужно.
 */
#include "dbio.h"
#include "ADC.h"
#include "USB.h"

typedef void (*ms_clbk_t)(void);

volatile uint32_t timestamp = 0;

volatile ms_clbk_t ms_clbk[MS_CLBK_MAX];
volatile uint8_t ms_clbk_max = 0;

uint8_t sys_register_ms_clbk(void (*clbk)(void))
{
  if(ms_clbk_max >= MS_CLBK_MAX) return 1;
  ms_clbk[ms_clbk_max] = clbk;
  ms_clbk_max++;
  return 0;
}

 void __interrupt(high_priority)  HighInterrupts_handler(void)
{
  // Тик 1 мс от Timer2. Перезагружать нечего: TMR2 сбрасывается аппаратно по
  // совпадению с PR2, поэтому период не зависит ни от задержки входа в
  // прерывание, ни от времени работы обработчика и ms-колбэков.
  if (PIE1bits.TMR2IE && PIR1bits.TMR2IF)
  {
    PIR1bits.TMR2IF = 0;
    timestamp++;
    for(uint8_t i = 0; i < ms_clbk_max; i++) ms_clbk[i]();
  }
  // Высокоприоритетных источников, кроме Timer2, сейчас нет.
  // Новый добавляется строкой вида:
  //   if(PIE1bits.XXIE && PIR1bits.XXIF) module_isr();
}

void __interrupt(low_priority)  Interrupts_handler(void)
{
  // Порядок — по убыванию частоты источника: UART на 230400 бод даёт
  // прерывание раз в ~43 мкс, USB SOF — раз в 1 мс, ADC — раз в 1 мс.
  if(PIE1bits.RC1IE && PIR1bits.RC1IF) dbio_rx_isr();
  if(PIE1bits.TX1IE && PIR1bits.TX1IF) dbio_tx_isr();
  if(PIE1bits.ADIE  && PIR1bits.ADIF)  ADC_isr();
  if(PIE2bits.USBIE && PIR2bits.USBIF) USB_isr();
}

/*
 * Тик 1 мс на Timer2.
 *
 * TMR2 сравнивается с PR2 и сбрасывается аппаратно на следующем цикле, поэтому
 * период равен ровно
 *     (PR2 + 1) * предделитель * постделитель = 50 * 16 * 15 = 12000
 * командных циклов = 1.000 мс при F_CYC = 12 МГц, и не зависит ни от задержки
 * входа в прерывание, ни от длительности обработчика.
 *
 * Прежний вариант на Timer0 с перезагрузкой присваиванием давал 12019.6 такта
 * (+1637 ppm): запись в TMR0 отбрасывает такты, прошедшие с момента
 * переполнения, и вдобавок обнуляет счёт предделителя (даташит, раздел 12.3).
 * Замеры на плате — timer_1ms_measurements.md.
 *
 * Timer2 после этого занят и не может использоваться как база для CCP/PWM.
 */
void sys_mstimer_init(void)
{
  T2CONbits.TMR2ON = 0;
  PR2 = 49;                    // (49 + 1) отсчёт до совпадения
  T2CONbits.T2CKPS = 0b11;     // предделитель:  1x = 1:16
  T2CONbits.T2OUTPS = 0b1110;  // постделитель: 1110 = 1:15
  TMR2 = 0;

  PIR1bits.TMR2IF = 0;
  IPR1bits.TMR2IP = 1;         // высокий приоритет
  PIE1bits.TMR2IE = 1;

  INTCONbits.GIE = 1;
  INTCONbits.PEIE = 1;

  T2CONbits.TMR2ON = 1;
}

void sys_init(void)
{
  OSCTUNEbits.PLLEN = 1; // PLL enable  
  RCONbits.IPEN = 1; // interrupt priority EN/DIS
  delay_cyc_ms(10);
  ANCON0 = 0xFF;   // all ports are digitall
  ANCON1 = 0x1F;   // all ports are digitall
  sys_mstimer_init();
}

void delay_ms(uint32_t del)
{
  volatile uint32_t temp_time = get_ms();
  while((get_ms() - temp_time) < del);
}

uint32_t get_ms(void)
{
  uint32_t retval_1 = timestamp;
  uint32_t retval_2 = timestamp;
  while(retval_1 != retval_2) {
    retval_1 = retval_2;
    retval_2 = timestamp;
  }
  return retval_2;
}

void UART1_Init(void)
{
  RCSTA1bits.SPEN = 0;   // Serial port is disabled
  TXSTA1bits.TXEN = 0;
  
  TRISCbits.TRISC7 = 1;
  TRISCbits.TRISC6 = 0;
    
  RCSTA1bits.RX9 = 0;    // Selects 8-bit reception
  RCSTA1bits.SREN = 0;   // Disables single receive
  
  TXSTA1bits.CSRC = 1;
  TXSTA1bits.TX9 = 0;
  TXSTA1bits.SYNC = 0; 
  TXSTA1bits.BRGH = 1;
  
  BAUDCON1bits.ABDEN = 0;
  BAUDCON1bits.RXDTP = 0;
  BAUDCON1bits.TXCKP = 0;
  BAUDCON1bits.BRG16 = 1;
  
  int32_t freq = F_OSC;
  uint16_t SPBRG_VAL = (uint16_t)((freq/(4 * U1_BAUDRATE)) + (((freq % (4 * U1_BAUDRATE)) >= (2 * U1_BAUDRATE)) ? 0 : -1));
  
  SPBRGH1 = (uint8_t)(SPBRG_VAL >> 8);
  SPBRG1 = (uint8_t)(SPBRG_VAL);
  
  TXSTA1bits.TXEN = 1;   // Transmit is enabled
  RCSTA1bits.CREN = 1;   // Enables receiver
  
  // Interrupt enable
  PIE1bits.TX1IE = 0;
  PIE1bits.RC1IE = 1;
  // Interrupt priority
  IPR1bits.RCIP = 0;
  IPR1bits.TXIP = 0;
  
  RCSTA1bits.SPEN = 1;   // Serial port is enabled
  
}

//void UART1_PutChar(char byte)
//{
//  while(!TXSTA1bits.TRMT);
//  TXREG1 = byte;
//}
//
//int16_t UART1_PutStr(char* byte, uint16_t N)
//{
//  for(int i = 0; i < N; i++)
//  {
//    UART1_PutChar(byte[i]);
//    if(byte[i] == 0) return i;
//  }
//  return -1;
//}
//
//char UART1_GetChar(void)
//{
//  uint8_t retval;
//  while(PIR1bits.RC1IF == 0)
//  {
//    if(RCSTAbits.OERR) 
//    {
//      RCSTAbits.CREN = 0;
//      RCSTAbits.CREN = 1;
//    }
//  }
//  PIR1bits.RC1IF = 0;
//  retval = RCREG;
//  return retval;
//}