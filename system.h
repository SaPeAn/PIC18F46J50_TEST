#ifndef SYSTEM_H
#define	SYSTEM_H

#include <xc.h>

#define      F_OSC                    48000000L
#define      U1_BAUDRATE              230400L

#define      LOPRIO_INTMAX            10 
#define      HIPRIO_INTMAX            10
#define      MS_CLBK_MAX              10


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

void sys_init(void);

uint32_t get_ms(void);
void delay_ms(uint32_t del);

uint8_t sys_regiter_ms_clbk(void (*clbk)(void));
uint8_t sys_regiter_IRQ_clbk(void (*cbk)(void), uint8_t IPrio);

void UART1_Init(void);
void UART1_PutChar(char byte);
int16_t UART1_PutStr(char* byte, uint16_t N);
char UART1_GetChar(void);

#endif	/* XC_HEADER_TEMPLATE_H */

