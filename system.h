#ifndef SYSTEM_H
#define	SYSTEM_H

#include <xc.h>

#define      F_OSC                    48000000L
#define      F_CYC                    (F_OSC / 4)
#define      U1_BAUDRATE              230400L

#define      delay_cyc_ms(ms)         {volatile uint32_t cycles = (F_CYC * ms) / 1000; while(cycles--);}

#define      MS_CLBK_MAX              10


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

/*
 * Nestable critical section. InterruptEn() sets GIE unconditionally, so a
 * plain InterruptDis()/InterruptEn() pair re-enables interrupts even when the
 * caller had them disabled. These macros save and restore the previous state.
 *
 * Usage:
 *   CRIT_DECL();
 *   CRIT_ENTER();
 *   ... shared data ...
 *   CRIT_EXIT();
 *
 * CRIT_DECL() declares a variable in the caller's scope, so it must not be
 * wrapped in do{}while(0) like the other two.
 */
#define      CRIT_DECL()              uint8_t gie_state
#define      CRIT_ENTER()             do { gie_state = (uint8_t)INTCONbits.GIE; InterruptDis(); } while(0)
#define      CRIT_EXIT()              do { if(gie_state) InterruptEn(); } while(0)

/*
 * ќбработчики прерываний модулей вызываютс€ напр€мую из Interrupts_handler()
 * в system.c Ч реестра указателей больше нет. „тобы подключить новый источник,
 * объ€вите его обработчик в заголовке модул€ и допишите строку в system.c.
 * ѕериодические задачи (раз в 1 мс) по-прежнему регистрируютс€ динамически
 * через sys_register_ms_clbk().
 */

void sys_init(void);

uint32_t get_ms(void);
void delay_ms(uint32_t del);

uint8_t sys_register_ms_clbk(void (*clbk)(void));

void UART1_Init(void);
//void UART1_PutChar(char byte);
//int16_t UART1_PutStr(char* byte, uint16_t N);
//char UART1_GetChar(void);

#endif	/* SYSTEM_H */

