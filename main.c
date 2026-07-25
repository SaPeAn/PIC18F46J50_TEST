#include "config.h"
#include <string.h>
#include "system.h"
#include <stdio.h>
#include "ringbuf.h"
#include "dbio.h"

void blinky(void)
{
  static uint32_t time;
  if(gettimestamp() - time > 10)
  {
    LATDbits.LATD4 = 0;
    if(gettimestamp() - time > 1000) 
    {
      LATDbits.LATD4 = 1;
      time = gettimestamp();
    }
  }
}

void ms_call_func(void)
{
  blinky();
}

void main(void) 
{
  Sys_init();
  SysMillisecTimestamp_init(ms_call_func);
  dbio_init();
  TRISDbits.TRISD4 = 0;
  
  char strtx[100];
  char strrx[100];
  
  sprintf(strtx, "Hello!!!\n\rTime: %ld\n\rEnter string: \n\r", gettimestamp());
  //dbio_putstring(strtx, 50);
  UART1_PutStr(strtx, 50);
  delay_ms(15);
  while(1)
  {
    if(dbio_getstring(strrx, 50, 5) > 0)
    {
      sprintf(strtx, "Time: %ld\n\rEntered string: %s\n\r", gettimestamp(), strrx);
      //dbio_putstring(strtx, 50);
  UART1_PutStr(strtx, 50);
      delay_ms(15);
      sprintf(strtx, "\n\rHello!!!\n\rTime: %ld\n\rEnter string: \n\r", gettimestamp());
      //dbio_putstring(strtx, 50);
  UART1_PutStr(strtx, 50);
      delay_ms(15);
    }
  }
  
  return;
}
