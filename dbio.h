#ifndef DBIO_H
#define	DBIO_H

#include "system.h"

#define     TxIntEn()            U1_TxIntEn()
#define     RxIntEn()            U1_RxIntEn()
#define     TxIntDis()           U1_TxIntDis()
#define     RxIntDis()           U1_RxIntDis()
#define     CheckTxPermission()  U1_CheckTxPermission()
#define     TxSendByte(byte)     U1_SendByte(byte)
#define     RxGetByte()          U1_GetByte()

void dbio_init(void);
int16_t dbio_getstring(uint8_t* ch, uint16_t Nmax, uint16_t timeout);
int16_t dbio_putstring(uint8_t* str, uint16_t Nmax);
        
#endif	/* DEBUGIO_UART1_H */

