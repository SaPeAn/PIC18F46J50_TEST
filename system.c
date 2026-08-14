#include "system.h"

typedef void (*IRQ_cbk_t)(void);
typedef void (*ms_cllbk_t)(void);

IRQ_cbk_t IRQ_clbk_lp[LOPRIO_INTMAX];
uint8_t IRQ_lpmax = 0;

IRQ_cbk_t IRQ_clbk_hp[HIPRIO_INTMAX];
uint8_t IRQ_hpmax = 0;

ms_cllbk_t ms_clbk[MS_CLBK_MAX];
uint8_t ms_clbk_max = 0;

uint8_t TMR0L_tmp;
uint8_t TMR0H_tmp;
volatile uint32_t timestamp = 0;

uint8_t sys_regiter_IRQ_clbk(void (*cbk)(void), uint8_t IPrio)
{
  if(IPrio == 0) //low priority interrupt
  {
    if(IRQ_lpmax >= LOPRIO_INTMAX) return 1; 
    IRQ_clbk_lp[IRQ_lpmax] = cbk;
    IRQ_lpmax++;
    return 0;
  }
  if(IPrio == 1) //high priority interrupt
  {
    if(IRQ_hpmax >= HIPRIO_INTMAX) return 2;
    IRQ_clbk_hp[IRQ_hpmax] = cbk;
    IRQ_hpmax++;  
    return 0;  
  }
  return 3;
}

uint8_t sys_regiter_ms_clbk(void (*clbk)(void))
{
  if(ms_clbk_max >= MS_CLBK_MAX) return 1;
  ms_clbk[ms_clbk_max] = clbk;
  ms_clbk_max++;
  return 0;
}

 void __interrupt(high_priority)  HighInterrupts_handler(void)
{
  // Timer0 interrupt
  if (TMR0IE && TMR0IF)
  {
    TMR0IF = 0;
    TMR0H = TMR0H_tmp;
    TMR0L = TMR0L_tmp;
    timestamp++;
    for(uint8_t i = 0; i < ms_clbk_max; i++) ms_clbk[i]();
  }
  // other hiprio registered interrupts handlers
  for(int i = 0; i < IRQ_hpmax; i++)
  {
    IRQ_clbk_hp[i]();
  }
}
 
void __interrupt(low_priority)  Interrupts_handler(void)
{
  // other loprio registered interrupts handlers
  for(int i = 0; i < IRQ_lpmax; i++)
  {
    IRQ_clbk_lp[i]();
  }
}

void sys_mstimer_init(void)
{
  T0CONbits.TMR0ON = 0;
  T0CONbits.T08BIT = 0;   // Timer0 is configured as a 16-bit timer/counter
  T0CONbits.T0PS = 3;     // 0 -- 1:2 Prescale value
                          // 1 -- 1:4 Prescale value
                          // 2 -- 1:8 Prescale value
                          // 3 -- 1:16 Prescale value
                          // 4 -- 1:32 Prescale value
  
  T0CONbits.T0CS = 0;     // Internal clock (FOSC/4)
  T0CONbits.PSA = 0;      // Timer0 prescaler is assigned. Timer0 clock input comes from prescaler output.
  
  // 48 MHz / 4 = 12 MHz / 4 = 3 MHz (6000 cycles for 1 ms) 
  // 65536 - 6000 = 59536
  uint32_t ms_cycles;
  uint32_t T0_prescaler = 2;
  for(int i = 0; i < T0CONbits.T0PS; i++) T0_prescaler *= 2;
  ms_cycles = 65536 - ((F_OSC / 4000) / T0_prescaler);
  TMR0L_tmp = (uint8_t)(ms_cycles % 256);
  TMR0H_tmp = (uint8_t)(ms_cycles / 256);
  
  TMR0H = TMR0H_tmp;
  TMR0L = TMR0L_tmp;
  
  INTCONbits.GIE = 1;
  INTCONbits.PEIE = 1;
  INTCONbits.T0IE = 1;
  TMR0IF = 0;
  
  T0CONbits.TMR0ON = 1;
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