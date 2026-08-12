#include "config.h"
#include <string.h>
#include "system.h"
#include <stdio.h>
#include "dbio.h"
#include "RTC.h"
#include "ADC.h"

void blinky(void)
{
  static uint32_t time;
  if(get_ms() - time > 10)
  {
    LATDbits.LATD4 = 0;
    if(get_ms() - time > 1000) 
    {
      LATDbits.LATD4 = 1;
      time = get_ms();
    }
  }
}

RTCC_VAL DateTime;

void main(void) 
{
  sys_init();
  sys_regiter_ms_clbk(blinky);
  dbio_init();
  RTC_init();
  DateTime.DAY = 1;
  DateTime.YEAR = 0;
  DateTime.MONTH = 1;
  DateTime.HOURS = 0;
  DateTime.MINUTES = 0;
  DateTime.SECONDS = 0;
  write_RTCC(&DateTime);
  ADC_init();
  ADC_start_IT();
  TRISDbits.TRISD4 = 0;
  
  char strtx[100];
  char strrx[100];
  
  sprintf(strtx, "Hello!!!\n\rTime: %ld\n\rEnter string: \n\r", get_ms());
  dbio_putstring(strtx, 50);
  delay_ms(15);
  while(1)
  {
    read_RTCC(&DateTime);
    
    sprintf(strtx, "ADC: %d %d %d\n\r", ADC_getChan(0), ADC_getChan(1), ADC_getChan(2));
    dbio_putstring(strtx, 100);
    
    if(dbio_getstring(strrx, 50, 5) > 0)
    {
      sprintf(strtx, "Time: %.2d:%2.2d:%2.2d\n\rEntered string: %s\n\r", BCDtoDEC(DateTime.HOURS), BCDtoDEC(DateTime.MINUTES), BCDtoDEC(DateTime.SECONDS), strrx);
      dbio_putstring(strtx, 100);
    }
    
    delay_ms(500);
  }
  
  return;
}
