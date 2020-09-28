#include "rtc.h"
#include "stm8s.h"
#include "main.h"

void ds1302_update_time(struct rtc_time * time, unsigned char field)	// read time, field can be SEC, MIN, HOUR...
{
	ds1302_comms(time, field, 0, READ);
}

void ds1302_set_time(struct rtc_time * time, unsigned char field, unsigned char w_byte)	{																						
	ds1302_comms(time, field, w_byte, WRITE);
}

void ds1302_comms(struct rtc_time * time, unsigned char field, unsigned char write_byte, unsigned char rw) {
	unsigned char temp;
	if(rw == READ) {
		if(field == SEC) {
			temp = ds1302_read_byte(sec_r);
			time->second = ((temp & 0x0F) + ((temp & 0x70)>>4)*10);
		}
		else if(field == MIN) {
			temp = ds1302_read_byte(min_r);
			time->minute = ((temp & 0x0F) + ((temp & 0x70)>>4)*10);
		}
		else if(field == HOUR) {
			temp = ds1302_read_byte(hour_r);
			if((temp & 0x80) == 1)		// 12 format
			{
				if((temp & 0x20) == 1)	// PM
				time->hour_format = PM;
				else					// AM
				time->hour_format = AM;
				
				time->hour = ((temp & 0x0F) + ((temp & 0x10)>>4)*10);
			}
			else						// 24 format
			{
				time->hour_format = H24;
				time->hour = ((temp & 0x0F) + ((temp & 0x30)>>4)*10);
			}
		}
		else if(field == DAY)
		{
			temp = ds1302_read_byte(day_r);
			time->day = (temp & 0x07);
		}
		else if(field == DATE)
		{
			temp = ds1302_read_byte(date_r);
			time->date = ((temp & 0x0F) + ((temp & 0x30)>>4)*10);
		}
		else if(field == MONTH)
		{
			temp = ds1302_read_byte(month_r);
			time->month = ((temp & 0x0F) + ((temp & 0x10)>>4)*10);
		}
		else if(field == YEAR)
		{
			temp = ds1302_read_byte(year_r);
			time->year = ((temp & 0x0F) + ((temp & 0xF0)>>4)*10);
		}
	}
	else if(rw == WRITE)
	{
		if(field == SEC)
		{
			ds1302_write_byte(sec_w, ((((write_byte/10)<<4) & 0x70) | (write_byte%10)));
		}
		else if(field == MIN)
		{
			ds1302_write_byte(min_w, ((((write_byte/10)<<4) & 0x70)  | (write_byte%10)));
		}
		else if(field == HOUR)
		{
			if(time->hour_format == AM)
			ds1302_write_byte(hour_w, ((((write_byte/10)<<4) & 0x10)  | (write_byte%10)) | 0x80);
			else if(time->hour_format == PM)
			ds1302_write_byte(hour_w, ((((write_byte/10)<<4) & 0x10)  | (write_byte%10)) | 0xA0);
			else if(time->hour_format == H24)
			ds1302_write_byte(hour_w, ((((write_byte/10)<<4) & 0x30) | (write_byte % 10)));
		}
		else if(field == DAY)
		{
			ds1302_write_byte(day_w, write_byte & 0x03);
		}
		else if(field == DATE)
		{
			ds1302_write_byte(date_w, ((((write_byte/10)<<4) & 0x30)  | (write_byte%10)));
		}
		else if(field == MONTH)
		{
			ds1302_write_byte(month_w, ((((write_byte/10)<<4) & 0x10) | (write_byte%10)));
		}
		else if(field == YEAR)
		{
			ds1302_write_byte(year_w, ((((write_byte/10)<<4) & 0xF0) | (write_byte%10)));
		}
	}
}

void ds1302_update(struct rtc_time * time) {
	ds1302_update_time(time, SEC);
	ds1302_update_time(time, MIN);
	ds1302_update_time(time, HOUR);
	ds1302_update_time(time, DAY);
	ds1302_update_time(time, DATE);
	ds1302_update_time(time, MONTH);
	ds1302_update_time(time, YEAR);
}

void ds1302_init(void) // sets all pins as output and low
{
  GPIO_DeInit(GPIOB);
  GPIO_Init(ds1302_PORT, (GPIO_Pin_TypeDef)RTC_CLK, GPIO_MODE_OUT_PP_HIGH_FAST);
  GPIO_Init(ds1302_PORT, (GPIO_Pin_TypeDef)RTC_CE, GPIO_MODE_OUT_PP_HIGH_FAST);
  GPIO_Init(ds1302_PORT, (GPIO_Pin_TypeDef)RTC_DIN, GPIO_MODE_OUT_PP_HIGH_FAST);

	clk_0(); // sclk -> 0
	Delay_us(20);
	rst_0(); // rst -> 0
	Delay_us(20);
	io_0(); // io -> 0
	Delay(100);
	ds1302_write_byte(0x8E, 0x00);
	Delay(100);
	
}


  
void ds1302_reset(void)	 //sets the pins to begin the ds1302 communication
{
	clk_0(); // sclk -> 0
	Delay_us(20);
	rst_0(); // rst -> 0
	Delay_us(20);
	rst_1(); // rst -> 1
	Delay_us(4);
}

unsigned char ds1302_read_byte(unsigned char w_byte)	//read the byte with register w_byte
{
	unsigned char temp;
	ds1302_reset();
	write(w_byte);
	temp = read();
	Delay_us(20);
	rst_0(); // rst -> 0
	Delay_us(20);
	clk_0();
	return temp;
}

void ds1302_write_byte(unsigned char w_byte, unsigned char w_2_byte)	//write the byte with register w_byte
{
	ds1302_reset();
	write(w_byte);
	write(w_2_byte);
	Delay_us(20);
	rst_0(); // rst -> 0
	Delay_us(20);
	clk_0();
}

void write(unsigned char W_Byte)	//writes the W_Byte to the DS1302
{
	unsigned char i;
	GPIO_Init(ds1302_PORT, (GPIO_Pin_TypeDef)RTC_DIN, GPIO_MODE_OUT_PP_HIGH_FAST);
	for(i = 0; i < 8; ++i) {
		io_0();
		if(W_Byte & 0x01) io_1();
		clk_0();
		Delay_us(20);
		clk_1();
		Delay_us(20);
		W_Byte >>=1;
	}
}

unsigned char read()	 //reads the ds1302 reply
{
	unsigned char i, R_Byte, TmpByte;
	R_Byte = 0x00;
	GPIO_Init(ds1302_PORT, (GPIO_Pin_TypeDef)RTC_DIN, GPIO_MODE_IN_FL_NO_IT);
	Delay(20);
	for(i = 0; i < 8; ++i) //get byte
	{
		clk_1();
		Delay_us(20);
		clk_0();
		Delay_us(20);
		TmpByte = 0;
		if(GPIO_ReadInputPin(ds1302_PORT, (GPIO_Pin_TypeDef)RTC_DIN))
		TmpByte = 1;
		TmpByte <<= 7;
		R_Byte >>= 1;
		R_Byte |= TmpByte;
	}
	return R_Byte;
}