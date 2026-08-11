#include "system.h"


uint8_t TMR0L_tmp;
uint8_t TMR0H_tmp;
volatile uint32_t timestamp = 0;

void (*ms_callback)(void);
void (*U1RX_callback)(void);
void (*U1TX_callback)(void);
void (*U1TX_callback)(void);
void (*ADC_callback)(void);



 void __interrupt(high_priority)  HighInterrupts_handler(void)
{
  // Timer0 interrupt
  if (TMR0IE && TMR0IF)
  {
    TMR0L += TMR0L_tmp;
    TMR0H = TMR0H_tmp;
    timestamp++;
    if(ms_callback != NULL) ms_callback();
    TMR0IF = 0;
  }
}
 
void __interrupt(low_priority)  Interrupts_handler(void)
{
  // UART1 RX interrupt
  if (RC1IE && RC1IF)
  {
    RC1IF = 0;
    if(U1RX_callback != NULL) U1RX_callback();
  }
  // UART1 TX interrupt
  if (TX1IE && TX1IF)
  {
    uint8_t dummy;
    if(U1TX_callback != NULL) U1TX_callback();
  }
  // ADC complete interrupt
  if (ADIF && ADIE)
  {
    ADIF = 0;
    if(ADC_callback != NULL) ADC_callback();
  }
}

void ADC_set_cbk(void (*adc_cbk)(void))
{
  ADC_callback = adc_cbk;
}
  
void Sys_init(void)
{
  OSCTUNEbits.PLLEN = 1; // PLL enable  
  RCONbits.IPEN = 1; // interrupt priority EN/DIS
  
  ANCON0 = 0xFF;   // all ports are digitall
  ANCON1 = 0x1F;   // all ports are digitall
}

void Sys_msTimestamp_init(void (*callback_func)(void))
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
  ms_cycles = 65537 - ((F_OSC / 4000) / T0_prescaler);
  TMR0L_tmp = (uint8_t)(ms_cycles % 256);
  TMR0H_tmp = (uint8_t)(ms_cycles / 256);
  
  TMR0L = TMR0L_tmp;
  TMR0H = TMR0H_tmp;
  
  ms_callback = callback_func;
  
  INTCONbits.GIE = 1;
  INTCONbits.PEIE = 1;
  INTCONbits.T0IE = 1;
  TMR0IF = 0;
  
  T0CONbits.TMR0ON = 1;
}

void delay_ms(uint32_t del)
{
  volatile uint32_t temp_time = timestamp;
  while((timestamp - temp_time) < del);
}

uint32_t get_ms(void)
{
  return timestamp;
}

void UART1_Init(void (*rx_cbck)(void), void (*tx_cbck)(void))
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

  uint16_t SPBRG_VAL = (uint16_t)((F_OSC/(4 * U1_BAUDRATE)) + (((F_OSC % (4 * U1_BAUDRATE)) >= (2 * U1_BAUDRATE)) ? 0 : -1));
  
  SPBRGH1 = (uint8_t)(SPBRG_VAL >> 8);
  SPBRG1 = (uint8_t)(SPBRG_VAL);
  
  TXSTA1bits.TXEN = 1;   // Transmit is enabled
  RCSTA1bits.CREN = 1;   // Enables receiver
  
  U1RX_callback = rx_cbck;
  U1TX_callback = tx_cbck;
  // Interrupt enable
  PIE1bits.TX1IE = 0;
  PIE1bits.RC1IE = 1;
  // Interrupt priority
  IPR1bits.RCIP = 0;
  IPR1bits.TXIP = 0;
  // Clear interrrupt flags
  PIR1bits.RC1IF = 0;
  PIR1bits.TX1IF = 0;
  
  RCSTA1bits.SPEN = 1;   // Serial port is enabled
  
}

void UART1_PutChar(char byte)
{
  while(!TXSTA1bits.TRMT);
  TXREG1 = byte;
}

int16_t UART1_PutStr(char* byte, uint16_t N)
{
  for(int i = 0; i < N; i++)
  {
    UART1_PutChar(byte[i]);
    if(byte[i] == 0) return i;
  }
  return -1;
}

char UART1_GetChar(void)
{
  uint8_t retval;
  while(PIR1bits.RC1IF == 0)
  {
    if(RCSTAbits.OERR) 
    {
      RCSTAbits.CREN = 0;
      RCSTAbits.CREN = 1;
    }
  }
  PIR1bits.RC1IF = 0;
  retval = RCREG;
  return retval;
}