#include "dbio.h"
#include "ringbuf.h"
#include "system.h"
#include <string.h>

#define UARTBUFLENGTH   200


RINGBUF_t RXringbuf;
uint8_t RXbuf[UARTBUFLENGTH] = {0};

RINGBUF_t TXringbuf;
uint8_t TXbuf[UARTBUFLENGTH] = {0};

void TXbyte_cbk(void)
{
  uint16_t buf_len = 0;
  RingBuf_Available(&buf_len, &TXringbuf);
  if(buf_len) RingBuf_DataRead((uint8_t*)&TXREG, 1, &TXringbuf);
  else U1_TxIntDis();
}

void RXbyte_cbk(void)
{
  RingBuf_DataPut((uint8_t*)&RCREG, 1, &RXringbuf);
}

void dbio_init(void)
{
  UART1_Init(RXbyte_cbk, TXbyte_cbk);
  RingBuf_Init(RXbuf, UARTBUFLENGTH, 1, &RXringbuf);
  RingBuf_Init(TXbuf, UARTBUFLENGTH, 1, &TXringbuf);
}

int16_t dbio_getstring(uint8_t* str, uint16_t Nmax, uint16_t timeout)
{
  if((Nmax + 1) > UARTBUFLENGTH) return -1;
  static uint32_t timetmp = 0;
  static uint16_t buf_len = 0;
  static uint16_t buf_len_prev = 0;
  
  RingBuf_Available(&buf_len, &RXringbuf);
  
  if(buf_len)
  {
    if(buf_len_prev != buf_len)
    {
      if(buf_len >= Nmax)
      {
        RingBuf_DataRead(str, Nmax, &RXringbuf);
        //RingBuf_Clear(&RXringbuf);
        buf_len_prev = 0;
        str[Nmax] = '\0';
        return Nmax;
      }
      buf_len_prev = buf_len;
      timetmp = gettimestamp();
      return 0;
    }
    if(((gettimestamp() - timetmp) > timeout))
    {
      buf_len = (buf_len > Nmax) ? Nmax : buf_len;
      RingBuf_DataRead(str, buf_len, &RXringbuf);
      str[buf_len] = '\0';
      return buf_len;
    }
  }
  return -2;
}

uint16_t dbio_strnlen(const char* str, uint16_t N)
{
  for(int i = 0; i < N; i++)
  {
    if(str[i]) continue;
    return ++i;
  }
  return N;
}

int16_t dbio_putstring(uint8_t* str, uint16_t Nmax)
{
  static uint16_t buf_len = 0;  
  uint16_t str_len;
  RingBuf_Available(&buf_len, &TXringbuf);
  if((buf_len + Nmax + 1) > UARTBUFLENGTH) return -1;  
 
  str_len = dbio_strnlen(str, Nmax);
  RingBuf_DataPut(str, str_len, &TXringbuf);
  RingBuf_DataRead(&TXREG, 1, &TXringbuf);
  U1_TxIntEn();
  return str_len;
}


