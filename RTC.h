#ifndef RTC_H
#define	RTC_H

#include <xc.h> 

#define BCDtoDEC(BCDByte)     (((BCDByte & 0xF0) >> 4) * 10) + (BCDByte & 0x0F)
#define DECtoBCD(DECByte)     (((DECByte / 10) << 4) | (DECByte % 10))

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
void RTC_init(void);

#endif	/* XC_HEADER_TEMPLATE_H */

