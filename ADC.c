#include "ADC.h"


void ADC_init(void)
{
  
#if AN0_Ch      //AN0(RA0)
  ChanNum[ChanMax] = 0;
  ANSELAbits.ANSELA0 = 1;
  TRISAbits.TRISA0 = 1;
  ADTRIG0Lbits.TRGSRC0 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN1_Ch      //AN1(RA1)
  ChanNum[ChanMax] = 1;
  ANSELAbits.ANSELA1 = 1;
  TRISAbits.TRISA1 = 1;
  ADTRIG0Lbits.TRGSRC1 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN2_Ch      //AN2(RA2)
  ChanNum[ChanMax] = 2;
  ANSELAbits.ANSELA2 = 1;
  TRISAbits.TRISA2 = 1;
  ADTRIG0Hbits.TRGSRC2 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN3_Ch      //AN3(RA3)
  ChanNum[ChanMax] = 3;
  ANSELAbits.ANSELA3 = 1;
  TRISAbits.TRISA3 = 1;
  ADTRIG0Hbits.TRGSRC3 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN4_Ch      //AN4(RA4)
  ChanNum[ChanMax] = 4;
  ANSELAbits.ANSELA4 = 1;
  TRISAbits.TRISA4 = 1;
  ADTRIG1Lbits.TRGSRC4 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN5_Ch      //AN5(RB0)
  ChanNum[ChanMax] = 5;
  ANSELBbits.ANSELB0 = 1;
  TRISBbits.TRISB0 = 1;
  ADTRIG1Lbits.TRGSRC5 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN6_Ch      //AN6(RB1)
  ChanNum[ChanMax] = 6;
  ANSELBbits.ANSELB1 = 1;
  TRISBbits.TRISB1 = 1;
  ADTRIG1Hbits.TRGSRC6 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN7_Ch      //AN7(RB2)
  ChanNum[ChanMax] = 7;
  ANSELBbits.ANSELB2 = 1;
  TRISBbits.TRISB2 = 1;
  ADTRIG1Hbits.TRGSRC7 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN8_Ch      //AN8(RB3)
  ChanNum[ChanMax] = 8;
  ANSELBbits.ANSELB3 = 1;
  TRISBbits.TRISB3 = 1;
  ADTRIG2Lbits.TRGSRC8 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN9_Ch      //AN9(RB7)
  ChanNum[ChanMax] = 9;
  ANSELBbits.ANSELB7 = 1;
  TRISBbits.TRISB7 = 1;
  ADTRIG2Lbits.TRGSRC9 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN10_Ch      //AN10(RB8)
  ChanNum[ChanMax] = 10;
  ANSELBbits.ANSELB8 = 1;
  TRISBbits.TRISB8 = 1;
  ADTRIG2Hbits.TRGSRC10 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN11_Ch      //AN11(RB9)
  ChanNum[ChanMax] = 11;
  ANSELBbits.ANSELB9 = 1;
  TRISBbits.TRISB9 = 1;
  ADTRIG2Hbits.TRGSRC11 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN12_Ch      //AN12(RC0)
  ChanNum[ChanMax] = 12;
  ANSELCbits.ANSELC0 = 1;
  TRISCbits.TRISC0 = 1;
  ADTRIG3Lbits.TRGSRC12 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN14_Vddcr      //AN14(RC2)
  ChanNum[ChanMax] = 14;
  ANSELCbits.ANSELC2 = 1;
  TRISCbits.TRISC2 = 1;
  ADTRIG3Hbits.TRGSRC14 = ADC_START_TRIG;
  ChanMax++;
#endif
#if AN15_Vbg      //AN15(RC7)
  ChanNum[ChanMax] = 15;
  ANSELCbits.ANSELC7 = 1;
  TRISCbits.TRISC7 = 1;
  ADTRIG3Hbits.TRGSRC15 = ADC_START_TRIG;
  ChanMax++;
#endif
  ADCON0bits.VCFG = 0b00;  // VCFG      VREF-        VREF+
                           // 00        AVss         AVdd 
                           // 01        AVss         Vref+(AN3)
                           // 10        Vref+(AN2)   AVdd
                           // 11        Vref+(AN2)   Vref+(AN3)
  ADCON1bits.ADFM = 1; // 1 = Right justified; 0 = Left justified
  //ADCON1bits.
          //ADRESH ADRESL
}