#include "RTC.h"

#define lock_RTCC()      RTCWREN = 0
#define on_RTCC()        RTCEN = 1
#define off_RTCC()       RTCEN = 0


void unlock_RTCC(void)
{   
  di();
  __asm("MOVLB 0x0F");          //sequence followed for unlocking RTCC
  __asm("MOVLW 0x55");
  __asm("MOVWF EECON2");
  __asm("MOVLW 0xAA");
  __asm("MOVWF EECON2");
  __asm("BSF RTCCFG, 5, 1");
  ei();
}

void write_RTCC(RTCC_VAL * const me){
    
    unlock_RTCC();
    off_RTCC();
    
    RTCOE = 1;
    
    RTCCFGbits.RTCPTR1 = 1;     //Point to Year
    RTCCFGbits.RTCPTR0 = 1;
    
    RTCVALL = me->YEAR;
    RTCVALH = me->unknown;
    
    RTCVALL = me->DAY;
    RTCVALH = me->MONTH;
    
    RTCVALL = me->HOURS;
    RTCVALH = me->WEEKDAY; 
    
    RTCVALL = me->SECONDS;
    RTCVALH = me->MINUTES;
    
    on_RTCC();
    lock_RTCC();
}

/*Read RTCC from RTCPTR*/
void read_RTCC(RTCC_VAL * const me){
    while(RTCSYNC);     //check if safe to access registers wait 'till zero
    unlock_RTCC();
    
    RTCCFGbits.RTCPTR1 = 1;     //Point to Year
    RTCCFGbits.RTCPTR0 = 1;
    
    me->YEAR = RTCVALL;
    me->unknown = RTCVALH;
    
    me->DAY = RTCVALL;
    me->MONTH = RTCVALH;
    
    me->HOURS = RTCVALL;
    me->WEEKDAY = RTCVALH;
    
    me->SECONDS = RTCVALL;
    me->MINUTES = RTCVALH;
    
    lock_RTCC();
}

uint8_t BCDtoDEC (uint8_t BCDByte){
    return(((BCDByte & 0xF0) >> 4) * 10) + (BCDByte &0x0F);
}

uint8_t DECtoBCD (uint8_t DECByte){
    return(((DECByte / 10) << 4) | (DECByte %10));
}