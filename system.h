#ifndef SYSTEM_H
#define	SYSTEM_H

#include <xc.h>

#define      F_OSC                 48000000L
#define      U1_BAUDRATE           230400L
#define      U1_TxIntEn()          PIE1bits.TX1IE = 1
#define      U1_RxIntEn()          PIE1bits.RC1IE = 1
#define      U1_TxIntDis()         PIE1bits.TX1IE = 0
#define      U1_RxIntDis()         PIE1bits.RC1IE = 0


void Sys_init(void);

void SysMillisecTimestamp_init(void (*)(void));

uint32_t gettimestamp(void);
void delay_ms(uint32_t del);


void UART1_Init(void (*rx_cbck)(void), void (*tx_cbck)(void));
void UART1_PutChar(char byte);
int16_t UART1_PutStr(char* byte, uint16_t N);
char UART1_GetChar(void);

#endif	/* XC_HEADER_TEMPLATE_H */

