#include "RTC.h"
#include "system.h"

#define lock_RTCC()           RTCWREN = 0
#define on_RTCC()             RTCEN = 1
#define off_RTCC()            RTCEN = 0

void unlock_RTCC(void)
{
  __asm("MOVLB 0x0F");          //sequence followed for unlocking RTCC
  __asm("MOVLW 0x55");
  __asm("MOVWF EECON2");
  __asm("MOVLW 0xAA");
  __asm("MOVWF EECON2");
  __asm("BSF RTCCFG, 5, 1");
}

void RTC_init(void)
{
  TRISCbits.TRISC0 = 1;
  TRISCbits.TRISC1 = 1;
  T1CONbits.T1OSCEN = 1;
  delay_ms(50);
}

/*
 * Register window layout (datasheet Table 17-3): the RTCPTR value decrements
 * only when RTCVALH is accessed, so RTCVALL of each pair must be touched
 * first. Pairs are --/YEAR, MONTH/DAY, WEEKDAY/HOURS, MINUTES/SECONDS.
 * Values are BCD - use DECtoBCD() when filling the structure.
 */
void write_RTCC(RTCC_VAL * const me)
{
  CRIT_DECL();
  CRIT_ENTER();
  unlock_RTCC();
  off_RTCC();

  RTCOE = 0;

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
  CRIT_EXIT();
}

/*Read RTCC from RTCPTR*/
void read_RTCC(RTCC_VAL * const me)
{
  CRIT_DECL();
  // Interrupts off first: RTCSYNC only tells us the rollover window is closed
  // right now, and an interrupt between the check and the reads could land us
  // in the middle of one. RTCWREN is not needed for reading.
  CRIT_ENTER();
  while(RTCSYNC);     //check if safe to access registers wait 'till zero

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

  CRIT_EXIT();
}