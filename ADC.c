#include "ADC.h"
#include "system.h"

uint8_t ChanNum[CHAN_MAX];
int ChanIndx = 0;
int16_t ChanValue[16];

void ADC_cplt_cbk(void)
{
  if(ADIE && ADIF)
  {
    ADIF = 0;
    ChanValue[ChanNum[ChanIndx++]] = (((int16_t)ADRESH) << 8) | ADRESL;
    if(ChanIndx >= CHAN_MAX) {
      ADIE = 0;
      ChanIndx = 0;
    }
    else {
      ADCON0bits.CHS = ChanNum[ChanIndx];
      ADCON0bits.GODONE = 1;
    }
  }
} 

void ADC_ms_cbk(void)
{
  if(ChanIndx == 0)
  {
    ADCON0bits.CHS = ChanNum[ChanIndx];
    ADIF = 0;
    ADIE = 1;
    ADCON0bits.GODONE = 1;
  }
} 

int16_t ADC_getChan(uint8_t chan)
{
  InterruptDis();
  int16_t retval = ChanValue[chan];
  InterruptEn();
  return retval;
}

void ADC_start_IT(void)
{
  ChanIndx = 0;
  ADCON0bits.CHS = ChanNum[ChanIndx];
  ADIF = 0;
  ADIE = 1;
  ADCON0bits.GODONE = 1;
}

void ADC_stop_IT(void)
{
  ChanIndx = 0;
  ADCON0bits.ADON = 0;
  ADIF = 0;
  ADIE = 0;
}

void ADC_init(void)
{
  int chanmax = 0;
  
  ANCON0 = 0xFF;   // all ports are digitall
  ANCON1 = 0x1F;   // all ports are digitall
  
#if AN0_Ch      //AN0(RA0)
  ChanNum[chanmax++] = 0;
  TRISAbits.TRISA0 = 1;
  ANCON0bits.PCFG0 = 0;
#endif
#if AN1_Ch       //AN1(RA1)
  ChanNum[chanmax++] = 1;
  TRISAbits.TRISA1 = 1;
  ANCON0bits.PCFG1 = 0;
#endif
#if AN2_Ch       //AN2(RA2)
  ChanNum[chanmax++] = 2;
  TRISAbits.TRISA2 = 1;
  ANCON0bits.PCFG2 = 0;
#endif
#if AN3_Ch       //AN3(RA3)
  ChanNum[chanmax++] = 3;
  TRISAbits.TRISA3 = 1;
  ANCON0bits.PCFG3 = 0;
#endif
#if AN4_Ch       //AN4(RA5)
  ChanNum[chanmax++] = 4;
  TRISAbits.TRISA5 = 1;
  ANCON0bits.PCFG4 = 0;
#endif
#if AN5_Ch       //AN5(RE0)
  ChanNum[chanmax++] = 5;
  TRISEbits.TRISE0 = 1;
  ANCON0bits.PCFG5 = 0;
#endif
#if AN6_Ch       //AN6(RE1)
  ChanNum[chanmax++] = 6;
  TRISEbits.TRISE1 = 1;
  ANCON0bits.PCFG6 = 0;
#endif
#if AN7_Ch       //AN7(RE2)
  ChanNum[chanmax++] = 7;
  TRISEbits.TRISE2 = 1;
  ANCON0bits.PCFG7 = 0;
#endif
#if AN8_Ch       //AN8(RB2)
  ChanNum[chanmax++] = 8;
  TRISBbits.TRISB2 = 1;
  ANCON1bits.PCFG8 = 0;
#endif
#if AN9_Ch       //AN9(RB3)
  ChanNum[chanmax++] = 9;
  TRISBbits.TRISB3 = 1;
  ANCON1bits.PCFG9 = 0;
#endif
#if AN10_Ch      //AN10(RB1)
  ChanNum[chanmax++] = 10;
  TRISBbits.TRISB1 = 1;
  ANCON1bits.PCFG10 = 0;
#endif
#if AN11_Ch      //AN11(RC2)
  ChanNum[chanmax++] = 11;
  TRISCbits.TRISC2 = 1;
  ANCON1bits.PCFG11 = 0;
#endif
#if AN12_Ch      //AN12(RB0)
  ChanNum[chanmax++] = 12;
  TRISBbits.TRISB0 = 1;
  ANCON1bits.PCFG12 = 0;
#endif
#if AN14_Vddcr   //AN14(Vddcr)
  ChanNum[chanmax++] = 14;
#endif
#if AN15_Vbg     //AN15(Vbg)
  ChanNum[chanmax++] = 15;
  ANCON1bits.VBGEN = 1;
#endif
  ADCON0bits.VCFG = 0b00;  // VCFG      VREF-        VREF+
                           // 00        AVss         AVdd 
                           // 01        AVss         Vref+(AN3)
                           // 10        Vref+(AN2)   AVdd
                           // 11        Vref+(AN2)   Vref+(AN3)
  
  ADCON1bits.ADFM = 1; // 1 = Right justified; 0 = Left justified
  
  ADCON1bits.ADCS = 0b110; // 110 = FOSC/64
                           // 101 = FOSC/16
                           // 100 = FOSC/4
                           // 011 = FRC (clock derived from A/D RC oscillator)(1)
                           // 010 = FOSC/32
                           // 001 = FOSC/8
                           // 000 = FOSC/2
  
  ADCON1bits.ACQT = 0b111; // 111 = 20 TAD
                           // 110 = 16 TAD
                           // 101 = 12 TAD
                           // 100 = 8 TAD
                           // 011 = 6 TAD
                           // 010 = 4 TAD
                           // 001 = 2 TAD
                           // 000 = 0 TAD
  
  IPR1bits.ADIP = 0;
  
  sys_regiter_IRQ_clbk(ADC_cplt_cbk, 0);
  sys_regiter_ms_clbk(ADC_ms_cbk);
  
  ADCON0bits.ADON = 1;
}