#ifndef DBIO_H
#define	DBIO_H

#include <xc.h>


void dbio_init(void);
int16_t dbio_getstring(uint8_t* ch, uint16_t Nmax, uint16_t timeout);
int16_t dbio_putstring(uint8_t* str, uint16_t Nmax);
        
#endif	/* DEBUGIO_UART1_H */

