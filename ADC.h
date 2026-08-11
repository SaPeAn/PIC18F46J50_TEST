#ifndef ADC_H
#define	ADC_H

#include <xc.h>

//     |  Cannel | Enable |Chan input |
#define   AN0_Ch      1     //AN0(RA0)
#define   AN1_Ch      1     //AN1(RA1)
#define   AN2_Ch      1     //AN2(RA2)
#define   AN3_Ch      0     //AN3(RA3)
#define   AN4_Ch      0     //AN4(RA5)
#define   AN5_Ch      0     //AN5(RE0)
#define   AN6_Ch      0     //AN6(RE1)
#define   AN7_Ch      0     //AN7(RE2)
#define   AN8_Ch      0     //AN8(RB2)
#define   AN9_Ch      0     //AN9(RB3)
#define   AN10_Ch     0     //AN10(RB1)
#define   AN11_Ch     0     //AN11(RC2)
#define   AN12_Ch     0     //AN12(RB0)
//        AN13_Ch     -     // Unimplemented
#define   AN14_Vddcr  0     // Vddcore
#define   AN15_Vbg    0     // Reference Voltage ~1.2 V (Band Gap)

#define   CHAN_MAX    (AN0_Ch+AN1_Ch+AN2_Ch+AN3_Ch+AN4_Ch+AN5_Ch+AN6_Ch+AN7_Ch+AN8_Ch+AN9_Ch+AN10_Ch+AN11_Ch+AN12_Ch+AN14_Vddcr+AN15_Vbg) 


int16_t ADC_getChan(uint8_t chan);

void ADC_start_IT(void);

void ADC_stop_IT(void);

void ADC_init(void);

#endif	/* ADC_H */

