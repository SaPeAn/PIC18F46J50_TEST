#ifndef RTC_H
#define	RTC_H

#include <xc.h> 

typedef struct{
    uint8_t YEAR;
    uint8_t MONTH;
    uint8_t DAY;
    uint8_t WEEKDAY;
    uint8_t HOURS;
    uint8_t MINUTES;
    uint8_t SECONDS;
    uint8_t unknown;
} RTCC_VAL;

void write_RTCC(RTCC_VAL * const me);
void read_RTCC(RTCC_VAL * const me);

#endif	/* XC_HEADER_TEMPLATE_H */

