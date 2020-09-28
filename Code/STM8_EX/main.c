#include "stm8s.h"
#include <stdio.h>
#include <stdbool.h>
#include "MAX72XX.h"
#include "main.h"
#include "rtc.h"
#include <string.h>

//#define DEFINE_CLOCK 1
void clock_setup(void);
void GPIO_setup(void);
void SPI_setup(void);
void ADC_start(int channel);
void I2C_Config(void);
void DS1621_Init();
uint8_t temperature[2] = {0};
__IO uint32_t TimingDelay = 0;
__IO uint32_t TimingDelay_us = 0;
uint16_t Conversion_Value = 0;
bool adcBusy;
int colorcnt = 0;
uint32_t hugeNumColorCnt = 0;
extern const unsigned char text[96];
extern const unsigned char DIGITS[80];
int ixx;
int sensorOneVal, sensorTwoVal;
/* Private function prototypes -----------------------------------------------*/
void TimingDelay_Decrement(void);
void TimingDelay_Decrement_us(void);
static void CLK_Config(void);
static void TIM4_Config(void);
void adjust_PWM();	
void PWM_OUT(int color_type, int duty_cycle);
static void TIM1_Config(void);
static void ADC_Config();
void tempMeasure(void);
void I2C_ReadRegister(unsigned char devAddr,unsigned char regAddr, unsigned char NumByteToRead, unsigned char *DataBuffer);
void I2C_WriteRegister(unsigned char devAddr,unsigned char regAddr, unsigned char NumByteToWrite, unsigned char *DataBuffer);

static void ADC_Config() {
  GPIO_Init(GPIOB, (GPIO_Pin_TypeDef)GPIO_PIN_0, GPIO_MODE_IN_FL_NO_IT);
  GPIO_Init(GPIOB, (GPIO_Pin_TypeDef)GPIO_PIN_1, GPIO_MODE_IN_FL_NO_IT);
  ADC1_DeInit();
  ADC1_ITConfig(ADC1_IT_EOCIE, DISABLE);
}

void ADC_Convert(int channel) {
  if (channel == 1) ADC1_Init(ADC1_CONVERSIONMODE_SINGLE, ADC1_CHANNEL_0, ADC1_PRESSEL_FCPU_D2, \
              ADC1_EXTTRIG_TIM, DISABLE, ADC1_ALIGN_RIGHT, ADC1_SCHMITTTRIG_CHANNEL0,\
              DISABLE);
    else ADC1_Init(ADC1_CONVERSIONMODE_SINGLE, ADC1_CHANNEL_1, ADC1_PRESSEL_FCPU_D2, \
              ADC1_EXTTRIG_TIM, DISABLE, ADC1_ALIGN_RIGHT, ADC1_SCHMITTTRIG_CHANNEL1,\
              DISABLE);
    ADC1_StartConversion();
    while(!ADC1_GetFlagStatus(ADC1_FLAG_EOC));
    if (channel == 1) sensorOneVal = ADC1_GetConversionValue();
    else sensorTwoVal = ADC1_GetConversionValue();
    
}

void ADC_Start(int channel) {
    if (channel == 1) ADC1_Init(ADC1_CONVERSIONMODE_CONTINUOUS, ADC1_CHANNEL_1, ADC1_PRESSEL_FCPU_D2, \
              ADC1_EXTTRIG_TIM, DISABLE, ADC1_ALIGN_RIGHT, ADC1_SCHMITTTRIG_CHANNEL1,\
              DISABLE);
    else ADC1_Init(ADC1_CONVERSIONMODE_CONTINUOUS, ADC1_CHANNEL_2, ADC1_PRESSEL_FCPU_D2, \
              ADC1_EXTTRIG_TIM, DISABLE, ADC1_ALIGN_RIGHT, ADC1_SCHMITTTRIG_CHANNEL2,\
              DISABLE);
    ADC1_ITConfig(ADC1_IT_EOCIE, ENABLE);
    ADC1_StartConversion();
    adcBusy = true;
}

void showRGB(int color) {
	int redIntensity;
	int greenIntensity;
	int blueIntensity;
        
	if (color <= 255) {
		redIntensity = 255 - color;    
		greenIntensity = color;       
		blueIntensity = 0;
	}
	else if (color <= 511) {
		redIntensity = 0;                     
		greenIntensity = 255 - (color - 256); 
		blueIntensity = (color - 256);
	}
	else {
		redIntensity = (color - 512);         
		greenIntensity = 0;                   
		blueIntensity = 255 - (color - 512);  
	}

	PWM_OUT(RED, redIntensity);
	PWM_OUT(BLUE, blueIntensity);
	PWM_OUT(GREEN, greenIntensity);
}

void adjust_PWM(int val) {
  if (val == 0) {
    showRGB(hugeNumColorCnt / 50);
    if (hugeNumColorCnt == 38350) hugeNumColorCnt = 0;
    hugeNumColorCnt++;
  }
  else showRGB(val);
}

void PWM_OUT(int color_type, int duty_cycle) {
  if (color_type == GREEN) {
      TIM1_OC1Init(TIM1_OCMODE_PWM2, TIM1_OUTPUTSTATE_ENABLE, TIM1_OUTPUTNSTATE_ENABLE,
      duty_cycle, TIM1_OCPOLARITY_HIGH, TIM1_OCNPOLARITY_LOW, TIM1_OCIDLESTATE_SET,
      TIM1_OCNIDLESTATE_RESET); 
  }
  if (color_type == BLUE) {
      TIM1_OC2Init(TIM1_OCMODE_PWM2, TIM1_OUTPUTSTATE_ENABLE, TIM1_OUTPUTNSTATE_ENABLE, 
      duty_cycle, TIM1_OCPOLARITY_HIGH, TIM1_OCNPOLARITY_LOW, TIM1_OCIDLESTATE_SET, 
      TIM1_OCNIDLESTATE_RESET);
  }
    if (color_type == RED) {
      TIM1_OC3Init(TIM1_OCMODE_PWM2, TIM1_OUTPUTSTATE_ENABLE, TIM1_OUTPUTNSTATE_ENABLE,
      duty_cycle, TIM1_OCPOLARITY_HIGH, TIM1_OCNPOLARITY_LOW, TIM1_OCIDLESTATE_SET,
      TIM1_OCNIDLESTATE_RESET); 
  }
}

static void TIM1_Config(void) {
  TIM1_DeInit();
  TIM1_TimeBaseInit(0, TIM1_COUNTERMODE_UP, 4095, 0);
  TIM1_Cmd(ENABLE);
  TIM1_CtrlPWMOutputs(ENABLE);
}

void initDevice(void) {
    enableInterrupts();
    CLK_Config(); 
    TIM4_Config(); 
    GPIO_setup();
    SPI_setup();
    MAX72xx_init();
    TIM1_Config();
    ADC_Config();
    I2C_Config();
    ds1302_init();	
    DS1621_Init();
}

void tempMeasure() {
  temperature[0] = 0;
  temperature[1] = 0;
  I2C_ReadRegister(0x90 >> 1, 0xAA, 2, temperature);
}

void I2C_WriteRegister(unsigned char devAddr,unsigned char regAddr, unsigned char NumByteToWrite, unsigned char *DataBuffer)
{
  //set_tout_ms(100);
  while((I2C->SR3 & 2))       									// Wait while the bus is busy
  {
    I2C->CR2 |= 2;                      								// STOP=1, generate stop
    while((I2C->CR2 & 2));      								// wait until stop is performed
  }
  
  I2C->CR2 |= 1;                        									// START=1, generate start
  while(((I2C->SR1 & 1)==0)); 									// Wait for start bit detection (SB)
  __no_operation();                        									// SB clearing sequence
  //if(tout())
  I2C->DR = (devAddr << 1);   							// Send 7-bit device address & Write (R/W = 0)
  while(!(I2C->SR1 & 2));     									// Wait for address ack (ADDR)
  __no_operation();                         									// ADDR clearing sequence
  I2C->SR3;
  while((!(I2C->SR1 & 0x80)));  									// Wait for TxE
  //if(tout())
  I2C->DR = regAddr;                 								// send Offset command
  if(NumByteToWrite)
  {
    while(NumByteToWrite--)          									{																										// write data loop start
      while((!(I2C->SR1 & 0x80)));  								// test EV8 - wait for TxE
      I2C->DR = *DataBuffer++;           								// send next data byte
    }																											// write data loop end
  }
  while(((I2C->SR1 & 0x84) != 0x84)); 					// Wait for TxE & BTF
  __no_operation();                     									// clearing sequence
  
  I2C->CR2 |= 2;                        									// generate stop here (STOP=1)
  while((I2C->CR2 & 2)); 
}

void I2C_ReadRegister(unsigned char devAddr,unsigned char regAddr, unsigned char NumByteToRead, unsigned char *DataBuffer)
{
  //set_tout_ms(100);
  /*--------------- BUSY? _ STOP request ---------------------*/
  while(I2C->SR3 & I2C_SR3_BUSY)	  				// Wait while the bus is busy
  {
    I2C->CR2 |= I2C_CR2_STOP;                  				// Generate stop here (STOP=1)
    while(I2C->CR2 & I2C_CR2_STOP); 				// Wait until stop is performed
  }
  I2C->CR2 |= I2C_CR2_ACK;                    				// ACK=1, Ack enable
  /*--------------- Start communication -----------------------*/  
  I2C->CR2 |= I2C_CR2_START;                   				// START=1, generate start
  while(!(I2C->SR1 & I2C_SR1_SB));				// Wait for start bit detection (SB)
  /*------------------ Address send ---------------------------*/      
  //if(tout())
    I2C->DR = (devAddr << 1);   						// Send 7-bit device address & Write (R/W = 0)
  while(!(I2C->SR1 & I2C_SR1_ADDR)); 				// test EV6 - wait for address ack (ADDR)
  __no_operation();                                  				// ADDR clearing sequence
  I2C->SR3;
  /*--------------- Register/Command send ----------------------*/
  while(!(I2C->SR1 & I2C_SR1_TXE));  				// Wait for TxE
  //if(tout())
  I2C->DR = regAddr;                         			// Send register address                                            					// Wait for TxE & BTF
  while(!((I2C->SR1 & I2C_SR1_TXE) && (I2C->SR1 & I2C_SR1_BTF))); 
  __no_operation();                                  				// clearing sequence
  /*--------------- Restart communication ---------------------*/  
  I2C->CR2 |= I2C_CR2_START;                     				// START=1, generate re-start
  while(!(I2C->SR1 & I2C_SR1_SB)); 				// Wait for start bit detection (SB)
  /*------------------ Address send ---------------------------*/      
  //if(tout())
  I2C->DR = (devAddr << 1) | 1;         	// Send 7-bit device address & Write (R/W = 1)
  
  while(!(I2C->SR1 & I2C_SR1_ADDR));  			// Wait for address ack (ADDR)
  /*------------------- Data Receive --------------------------*/
  if (NumByteToRead > 2)                 						// *** more than 2 bytes are received? ***
  {
    I2C->SR3;                                     			// ADDR clearing sequence    
    while((NumByteToRead > 3))       			// not last three bytes?
    {
      while(!(I2C->SR1 & I2C_SR1_BTF)); 				// Wait for BTF
      *DataBuffer++ = I2C->DR;                   				// Reading next data byte
      --NumByteToRead;																		// Decrease Numbyte to reade by 1
    }
    //last three bytes should be read
    while(!(I2C->SR1 & I2C_SR1_BTF)); 			// Wait for BTF
    I2C->CR2 &= ~I2C_CR2_ACK;                     			// Clear ACK
    __disable_interrupt();                          			// Errata workaround (Disable interrupt)
    *DataBuffer++ = I2C->DR;                     		// Read 1st byte
    I2C->CR2 |= I2C_CR2_STOP;                       		// Generate stop here (STOP=1)
    *DataBuffer++ = I2C->DR;                     		// Read 2nd byte
    __enable_interrupt();																// Errata workaround (Enable interrupt)
    while(!(I2C->SR1 & I2C_SR1_RXNE));			// Wait for RXNE
    *DataBuffer++ = I2C->DR;                   			// Read 3rd Data byte
  }
  else
  {
    if(NumByteToRead == 2)                						// *** just two bytes are received? ***
    {
      I2C->CR2 |= I2C_CR2_POS;                       		// Set POS bit (NACK at next received byte)
      __disable_interrupt();                             		// Errata workaround (Disable interrupt)
      I2C->SR3;                                       	// Clear ADDR Flag
      I2C->CR2 &= ~I2C_CR2_ACK;                        	// Clear ACK 
      __enable_interrupt();																// Errata workaround (Enable interrupt)
      while(!(I2C->SR1 & I2C_SR1_BTF)); 		// Wait for BTF
      __disable_interrupt();                       		// Errata workaround (Disable interrupt)
      I2C->CR2 |= I2C_CR2_STOP;                      	// Generate stop here (STOP=1)
      *DataBuffer++ = I2C->DR;                     	// Read 1st Data byte
      __enable_interrupt();																// Errata workaround (Enable interrupt)
      *DataBuffer = I2C->DR;													// Read 2nd Data byte
    }
    else                                      					// *** only one byte is received ***
    {
      I2C->CR2 &= ~I2C_CR2_ACK;                     		// Clear ACK 
      __disable_interrupt();                          		// Errata workaround (Disable interrupt)
      I2C->SR3;                                       	// Clear ADDR Flag   
      I2C->CR2 |= I2C_CR2_STOP;                       	// generate stop here (STOP=1)
      __enable_interrupt();																// Errata workaround (Enable interrupt)
      while(!(!I2C->SR1 & I2C_SR1_RXNE)); 		// test EV7, wait for RxNE
      *DataBuffer = I2C->DR;                     		// Read Data byte
    }
  }
  /*--------------- All Data Received -----------------------*/
  while(I2C->CR2 & I2C_CR2_STOP);     		// Wait until stop is performed (STOPF = 1)
  I2C->CR2 &= ~I2C_CR2_POS;                          		// return POS to default state (POS=0)
}

void DS1621_Init() {
  I2C_WriteRegister(0x90 >> 1, 0xAC, 1, 0);
  I2C_WriteRegister(0x90 >> 1, 0xEE, 0, 0);
  }

void I2C_Config() {
CLK->PCKENR1 |= CLK_PCKENR1_I2C;
  
  //define SDA, SCL outputs, HiZ, Open drain, Fast
  GPIOB->ODR |= 0x30;               
  GPIOB->DDR |= 0x30;
  GPIOB->CR2 |= 0x30;
  I2C->FREQR = 16;              // 16MHz clock source
  I2C->CCRH &= ~I2C_CCRH_FS;        // standard speed
  I2C->CCRL = 50;               // 50KHz I2C clock
  I2C->TRISER = 100;             // maximum rise time 
  I2C->CR1 |= (uint8_t)(I2C_CR1_NOSTRETCH);
  I2C->OARH |= I2C_OARH_ADDCONF;
  I2C->CR1 |= I2C_CR1_PE;          // I2C enabled
}

unsigned char timeBuff[56] = {0};
unsigned char dateBuff[96] = {0};
unsigned char tempBuff[64] = {0};



const unsigned char DOTS[8] = { 0x00, 0x00, 0x00, 0x66, 0x66, 0x00, 0x00, 0x00 };
const unsigned char DASH[8] = { 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00 };
const unsigned char DEGR[8] = { 0x00, 0x00, 0x06, 0x09, 0x09, 0x06, 0x00, 0x00 };
const unsigned char CCHAR[8] = { 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x24, 0x00 };
const unsigned char DOTDEC[8] = { 0x00, 0x00, 0x00, 0xC0, 0xC0, 0x00, 0x00, 0x00 };



#define left 10
#define right 11

int getDigit(int number, int pos) {
  if (pos == left) return (number / 10);
  else return (number % 10);
}

void showTime() {
    unsigned char i = 0x00;
    unsigned char j = 0x00;
    unsigned char temp[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for(i = 0; i < sizeof(temp); i++) temp[i] = 0x00;
                                
          for(i = 0; i < sizeof(timeBuff); i++) {
              for(j = 0; j < sizeof(temp); j++) {
                  temp[j] = timeBuff[(i + j)];
                  MAX72xx_write((1 + j), temp[j]);
                  Delay(5);
              }
           }
}

void showDate() {
    unsigned char i = 0x00;
    unsigned char j = 0x00;
    unsigned char temp[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for(i = 0; i < sizeof(temp); i++) temp[i] = 0x00;
                                
          for(i = 0; i < sizeof(dateBuff); i++) {
              for(j = 0; j < sizeof(temp); j++) {
                  temp[j] = dateBuff[(i + j)];
                  MAX72xx_write((1 + j), temp[j]);
                  Delay(5);
              }
           }
}

void showTemp() {
  unsigned char i = 0x00;
    unsigned char j = 0x00;
    unsigned char temp[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for(i = 0; i < sizeof(temp); i++) temp[i] = 0x00;                      
          for(i = 0; i < sizeof(tempBuff); i++) {
              for(j = 0; j < sizeof(temp); j++) {
                  temp[j] = tempBuff[(i + j)];
                  MAX72xx_write((1 + j), temp[j]);
                  Delay(5);
              }
           }
}

uint8_t pollSwitch() {
  if (!GPIO_ReadInputPin(GPIOD, GPIO_PIN_6)) {
      while (!GPIO_ReadInputPin(GPIOD, GPIO_PIN_6));
      return 1; // UP 
  }
  else if (!GPIO_ReadInputPin(GPIOD, GPIO_PIN_7)) {
      while (!GPIO_ReadInputPin(GPIOD, GPIO_PIN_7));
      return 2; // DOWN 
  }
  else return 0; // NONE
}

void main(void) {
    adcBusy = false;
    sensorOneVal = 0;
    sensorTwoVal = 0;
    Conversion_Value = 0;
    ixx = 1;
    int pollSwVal = 0;
    initDevice();
    struct rtc_time ds1302;				
    struct rtc_time *rtc;
    rtc = &ds1302;					
    ds1302_update(rtc);	
#if DEFINE_CLOCK
    ds1302_set_time(rtc, SEC, 00);
    ds1302_set_time(rtc, MIN, 00);
    ds1302_set_time(rtc, HOUR, 21);
    ds1302_set_time(rtc, DATE, 29);
    ds1302_set_time(rtc, MONTH, 11);
    ds1302_set_time(rtc, YEAR, 19);
#endif
    int leftDigit = 0, rightDigit = 0;
      sensorOneVal = 0;
      sensorTwoVal = 0;
      //while(1) tempMeasure();
    while(TRUE) {
      pollSwVal = pollSwitch();
      if (pollSwVal == 1) {
        ds1302_update_time(rtc, HOUR); 
        if (rtc->hour == 23) ds1302_set_time(rtc, HOUR, 0);
        else ds1302_set_time(rtc, HOUR, rtc->hour + 1);
      }
      else if (pollSwVal == 2) {
        ds1302_update_time(rtc, HOUR);
        if (rtc->hour == 0) ds1302_set_time(rtc, HOUR, 23);
        else ds1302_set_time(rtc, HOUR, rtc->hour - 1);
      }
      sensorOneVal = 0;
      sensorTwoVal = 0;
      if (ixx == 1) ixx = 2;
      else ixx = 1;
      ADC_Convert(ixx);
      adjust_PWM(0);
      if (sensorOneVal > 500) {
        while(sensorOneVal > 500) {
          ADC_Convert(1);
        }  
        while((sensorTwoVal < 500)) {
          ADC_Convert(2);
        }
        if ((sensorTwoVal > 500) && (sensorOneVal < 500)) {
          tempMeasure();
          memcpy(tempBuff + 40, DEGR, 8);
          memcpy(tempBuff + 48, CCHAR, 8);
          memcpy(tempBuff + 24, DOTDEC, 8);
          leftDigit = getDigit(temperature[0], left);
          rightDigit = getDigit(temperature[0], right);
          memcpy(tempBuff + 8, &DIGITS[8*leftDigit], 8);
          memcpy(tempBuff + 16, &DIGITS[8*rightDigit], 8);
          if (temperature[1]) memcpy(tempBuff + 32, &DIGITS[8*5], 8);
          else memcpy(tempBuff + 32, &DIGITS[0], 8);
          showTemp();
          memcpy(dateBuff + 24, DASH, 8);
          leftDigit = getDigit(rtc -> date, left);
          rightDigit = getDigit(rtc -> date, right);
          memcpy(dateBuff + 8, &DIGITS[8*leftDigit], 8);
          memcpy(dateBuff + 16, &DIGITS[8*rightDigit], 8);
          leftDigit = getDigit(rtc -> month, left);
          rightDigit = getDigit(rtc -> month, right);
          memcpy(dateBuff + 32, &DIGITS[8*leftDigit], 8);
          memcpy(dateBuff + 40, &DIGITS[8*rightDigit], 8);
          memcpy(dateBuff + 48, DASH, 8);
          memcpy(dateBuff + 56, &DIGITS[8*2], 8);
          memcpy(dateBuff + 64, &DIGITS[0], 8);
          leftDigit = getDigit(rtc -> year, left);
          rightDigit = getDigit(rtc -> year, right);
          memcpy(dateBuff + 72, &DIGITS[8*leftDigit], 8);
          memcpy(dateBuff + 80, &DIGITS[8*rightDigit], 8);
          showDate();
      }
      }
      else if (sensorTwoVal > 500) {
        while((sensorTwoVal > 500)) {
          ADC_Convert(2);
        }
        while((sensorOneVal < 500)) {
          ADC_Convert(1);
        }
        if ((sensorTwoVal < 500) && (sensorOneVal > 500)) {
          ds1302_update_time(rtc, SEC);
          ds1302_update_time(rtc, MIN);
          ds1302_update_time(rtc, HOUR);       
          ds1302_update_time(rtc, DATE);
          ds1302_update_time(rtc, MONTH);
          ds1302_update_time(rtc, YEAR);
          memcpy(timeBuff + 24, DOTS, 8);
          leftDigit = getDigit(rtc -> hour, left);
          rightDigit = getDigit(rtc -> hour, right);
          memcpy(timeBuff + 8, &DIGITS[8*leftDigit], 8);
          memcpy(timeBuff + 16, &DIGITS[8*rightDigit], 8);
          leftDigit = getDigit(rtc -> minute, left);
          rightDigit = getDigit(rtc -> minute, right);
          memcpy(timeBuff + 32, &DIGITS[8*leftDigit], 8);
          memcpy(timeBuff + 40, &DIGITS[8*rightDigit], 8);
          showTime();
      }
      }
    };
}

void GPIO_setup(void) {
     GPIO_DeInit(GPIOC);
     GPIO_Init(GPIOC, (GPIO_Pin_TypeDef)GPIO_PIN_5, GPIO_MODE_OUT_PP_HIGH_FAST);
     GPIO_Init(GPIOC, (GPIO_Pin_TypeDef)GPIO_PIN_6, GPIO_MODE_OUT_PP_HIGH_FAST);
     GPIO_Init(GPIOD, (GPIO_Pin_TypeDef)GPIO_PIN_6, GPIO_MODE_IN_PU_NO_IT);
     GPIO_Init(GPIOD, (GPIO_Pin_TypeDef)GPIO_PIN_7, GPIO_MODE_IN_PU_NO_IT);
     GPIO_Init(GPIOB, (GPIO_Pin_TypeDef)GPIO_PIN_5, GPIO_MODE_OUT_PP_LOW_FAST);
     GPIO_Init(GPIOB, (GPIO_Pin_TypeDef)GPIO_PIN_4, GPIO_MODE_OUT_PP_LOW_FAST);
}
 
void SPI_setup(void) {
     SPI_DeInit();
     SPI_Init(SPI_FIRSTBIT_MSB, 
              SPI_BAUDRATEPRESCALER_2, 
              SPI_MODE_MASTER, 
              SPI_CLOCKPOLARITY_HIGH, 
              SPI_CLOCKPHASE_1EDGE, 
              SPI_DATADIRECTION_1LINE_TX, 
              SPI_NSS_SOFT, 
              0x01);
     SPI_Cmd(ENABLE);
}

static void CLK_Config(void)
{
    /* Initialization of the clock */
    /* Clock divider to HSI/1 */
     CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1);    
     CLK_PeripheralClockConfig(CLK_PERIPHERAL_SPI, ENABLE);
}

static void TIM4_Config(void)
{
  /* TIM4 configuration:
   - TIM4CLK is set to 16 MHz, the TIM4 Prescaler is equal to 128 so the TIM1 counter
   clock used is 16 MHz / 128 = 125 000 Hz
  - With 125 000 Hz we can generate time base:
      max time base is 2.048 ms if TIM4_PERIOD = 255 --> (255 + 1) / 125000 = 2.048 ms
      min time base is 0.016 ms if TIM4_PERIOD = 1   --> (  1 + 1) / 125000 = 0.016 ms
  - In this example we need to generate a time base equal to 1 ms
   so TIM4_PERIOD = (0.001 * 125000 - 1) = 124 */

  /* Time base configuration */
  TIM4_TimeBaseInit(TIM4_PRESCALER_128, TIM4_PERIOD);
  /* Clear TIM4 update flag */
  TIM4_ClearFlag(TIM4_FLAG_UPDATE);
  /* Enable update interrupt */
  TIM4_ITConfig(TIM4_IT_UPDATE, ENABLE);
  
  TIM2_TimeBaseInit(TIM2_PRESCALER_1, TIM2_PERIOD);
  TIM2_ClearFlag(TIM2_FLAG_UPDATE);
  TIM2_ITConfig(TIM2_IT_UPDATE, ENABLE);
  /* enable interrupts */
  TIM2_Cmd(DISABLE);
  TIM4_Cmd(DISABLE);
  /* Enable TIM4 */
  //TIM4_Cmd(ENABLE);
  //TIM2_Cmd(ENABLE);
}

void Delay(__IO uint32_t nTime) {
  TimingDelay = nTime;
  TIM4_Cmd(ENABLE);
  while (TimingDelay != 0);
  TIM4_Cmd(DISABLE);
}

void Delay_us(__IO uint32_t nTime) {
  TimingDelay_us = nTime;
  TIM2_Cmd(ENABLE);
  while (TimingDelay_us != 0);
  TIM2_Cmd(DISABLE);
}

void TimingDelay_Decrement_us(void) {
  if (TimingDelay_us != 0x00) TimingDelay_us--;
}

void TimingDelay_Decrement(void) {
  if (TimingDelay != 0x00) TimingDelay--;
}

#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param file: pointer to the source file name
  * @param line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
