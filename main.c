#include "config.h"
#include <string.h>
#include "system.h"
#include <stdio.h>
#include "dbio.h"
#include "RTC.h"
#include "ADC.h"
#include "USB.h"

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
  dbio_init();
  
  TRISDbits.TRISD4 = 0;
  sys_register_ms_clbk(blinky);
  
  RTC_init();
  // Регистры RTCC хранят BCD, поэтому десятичные значения переводим явно:
  // без DECtoBCD() всё, что больше 9, попало бы в часы как мусор.
  DateTime.DAY = DECtoBCD(1);
  DateTime.YEAR = DECtoBCD(0);
  DateTime.MONTH = DECtoBCD(1);
  DateTime.WEEKDAY = DECtoBCD(0);
  DateTime.HOURS = DECtoBCD(0);
  DateTime.MINUTES = DECtoBCD(0);
  DateTime.SECONDS = DECtoBCD(0);
  write_RTCC(&DateTime);
  
  ADC_init();
  ADC_start_IT();

  USB_init();

  char strrx[100];
  uint8_t usb_was_ready = 0;
  
  dbio_putfstring("Hello!!!\n\rTime: %ld\n\rEnter string: \n\r", get_ms());
  delay_ms(15);
  while(1)
  {
    read_RTCC(&DateTime);
    
    dbio_putfstring("ADC: %d %d %d\n\r", ADC_getChan(0), ADC_getChan(1), ADC_getChan(2));
    
    // Сообщаем в отладочный порт о смене состояния USB-клавиатуры
    if(USB_is_configured() != usb_was_ready)
    {
      usb_was_ready = USB_is_configured();
      dbio_putfstring(usb_was_ready ? "USB keyboard: ready\n\r"
                                   : "USB keyboard: not ready\n\r");
    }

    if(dbio_getstring(strrx, 50, 5) > 0)
    {
      dbio_putfstring("Time: %.2d:%2.2d:%2.2d\n\rEntered string: %s\n\r", BCDtoDEC(DateTime.HOURS), BCDtoDEC(DateTime.MINUTES), BCDtoDEC(DateTime.SECONDS), strrx);

      // ...и "печатаем" её же на ПК как обычная USB-клавиатура
      if(USB_is_configured())
      {
        USB_kbd_putstring(strrx, 50);
        USB_kbd_putkey(0x28, 0);   // Enter
      }
    }
    delay_ms(500);
  }
}
