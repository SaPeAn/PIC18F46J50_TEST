#ifndef SYSTEM_H
#define	SYSTEM_H

#include <xc.h>

#define      F_OSC                    48000000L
#define      U1_BAUDRATE              230400L
#define      U1_TxIntEn()             PIE1bits.TX1IE = 1
#define      U1_RxIntEn()             PIE1bits.RC1IE = 1
#define      U1_TxIntDis()            PIE1bits.TX1IE = 0
#define      U1_RxIntDis()            PIE1bits.RC1IE = 0
#define      U1_CheckTxPermission()   TX1IF
#define      U1_SendByte(byte)        TXREG = byte
#define      U1_GetByte()             RCREG

#ifndef	InterruptEn
#define	InterruptEn()	INTCONbits.GIE = 1 	// Interrupts of Hi/Lo Priority or Peripheral interrupts 
#endif

#ifndef	InterruptDis
#define	InterruptDis()	INTCONbits.GIE = 0	// Interrupts of Hi/Lo Priority or Peripheral interrupts 
#endif

void Sys_init(void);

void Sys_msTimestamp_init(void (*)(void));

uint32_t get_ms(void);
void delay_ms(uint32_t del);


void UART1_Init(void (*rx_cbck)(void), void (*tx_cbck)(void));
void UART1_PutChar(char byte);
int16_t UART1_PutStr(char* byte, uint16_t N);
char UART1_GetChar(void);
void ADC_set_cbk(void (*adc_cbk)(void));

#endif	/* XC_HEADER_TEMPLATE_H */

