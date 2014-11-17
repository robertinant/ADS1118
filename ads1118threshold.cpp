

#include "Energia.h"
#include "ADS1118.h"
#include <LCDDRIVER.h>
#include <SPI.h>

#define LPBUTTON PUSH1

ADS1118 ADS;

void setup(void);
void loop(void);
void LPButton(void);
void half_second(void);
void set_time();
void time_display();
void setThreshTemp();
void ADC_display();

extern volatile unsigned int flag;
volatile unsigned char threshState;
volatile unsigned char timeState;

unsigned long interval = 200;
unsigned long time = 0;	// current time, a Continuous number seconds
unsigned int setTime;	// temporary for setting time
unsigned int threshTemp;// Threshold temperature
unsigned int setTemp;	// temporary for setting Threshold temperature
unsigned int num=0;		// temporary for setting Threshold temperature
int actualTemp;	        // Actual temperature
unsigned long previousMillis = 0;

unsigned int timerCounter = 0;

//CogLCD lcd(LCD_SI, LCD_SCL, LCD_RS, LCD_CSB, LCD_RST);

/*
 * ======== global variable ========
 */
/* flag is used to transmit parameter between interrupt service function and main function.
 * the flag will be changed by ISR in InterruptVectors_init.c     ...\grace\InterruptVectors_init.c
 *
 * Bit0, Launchpad S2 is pushed
 * Bit1, SW1 on BoosterPack is pushed
 * Bit2, SW2 on BoosterPack is pushed
 * Bit3, 1 second timer interrupt
 * Bit4, timer for ADC interrupts
 * Bit5, ADC input flag, 0 is internal temperature sensor, 1 is AIN0-AIN1
 * Bit6, make an inversion every half a second
 * Bit7, half a second interrupt
 * Bit8, for Fahrenheit display
 * Bit9, ADC channel flag, 0 for channel 0, 1 for channel 1.
 */

void setup() {
	flag = 0;				//reset flag
	Serial.begin(9600);
	threshState = 0;		//threshold temperature setting state machine counter
	timeState = 0;			//time setting state machine counter
	threshTemp = 100;		//configure threshold temperature to 100;
	actualTemp = 250;

	LCD_init();
	LCD_clear();								// LCD clear
	LCD_display_string(0,"TH:");				// display "ADS1118"
	LCD_display_time(0,8,time);					// display current time
	LCD_display_string(1,"Temp:        CH1");	// display threshold temp and actual temp;
	LCD_display_char(1,10,0xDF);
	LCD_display_char(1,11,'C');
	LCD_display_number(0,3,threshTemp);			// display threshold temp number

	ADS.begin(0);

	pinMode(9, INPUT);
	pinMode(10, INPUT);

	attachInterrupt(LPBUTTON, LPButton, RISING);

	pinMode(2, OUTPUT);
	noTone(2);			//buzzer on
}

void loop()
{
	unsigned long currentMillis = millis();
	if(currentMillis - previousMillis> interval) {

		previousMillis = currentMillis;

		flag |= BIT4;
		flag ^= BIT5;

		if(timerCounter == 4)
		{
			//2HZ ISR
			flag |= BIT7;
			flag ^= BIT6;

			if (!(flag & BIT6))
			  flag |= BIT3;

			timerCounter = 0;
		}
		else{
			timerCounter++;
		}
	}

	if(!digitalRead(9)){
	flag |= BIT1;		// flag bit 1 is set
	delay(100);
	}

	if(!digitalRead(10)){
	flag |= BIT2;		// flag bit 1 is set
	delay(100);
	}
		// half a second interrupt.
	if(flag & BIT7)
	{
		half_second();
	}

	// read ADC result
	if(flag & BIT4)		//Read ADC result
	{
		ADC_display();
		Serial.println("inside adc display");
	}

	// one second interrupt to display time
	if (flag & BIT3)
	{
		time_display();
	}

	if(flag & BIT1)				// if SW1 is pushed, threshold temperature state machine will be changed
	{
		flag &= ~ BIT1;			// flag is reset
		timeState = 0;			//when threshold temperature is setting, setting time is disable.
		if(threshState >= 3)	// if in state 3, change to state 0;
		{
			threshState = 0;
			threshTemp = setTemp;				// assign threshold temperature
			LCD_display_number(0,3,threshTemp);	// display threshold temperature
		}
		else					//else, threshState is changed to next state
		{
			threshState ++;
		}
	}

	if((flag & BIT2) && (!threshState))			// if SW2 is pushed, and threshState = 0, time setting state machine will be changed
	{
		flag &= ~ BIT2;							// flag is reset

		if(!(digitalRead(9)))
		{
			flag ^= BIT9;						// S2 and SW2 are pushed together to change input channel
			flag ^= BIT8;
			if(flag & BIT9)
				LCD_display_char(1,15,'2');
			else
				LCD_display_char(1,15,'1');
		}
		else
		{
			if(timeState >= 4)					// if in state 4, change to state 0;
			{
				timeState = 0;
				time = setTime;					// assign actual time
				setTime = 0;
				LCD_display_time(0,8,time);		// display setting time
			}
			else
			{
			timeState ++;
			}
		}
	}

	if(flag & BIT0)				// P1.3 service, set the Threshold temperature or Time.
	{
		flag &= ~ BIT0;			// flag is reset
		if(threshState != 0)
		{
			setThreshTemp();	// set threshold temperature.
		}
		else if(timeState != 0)
		{
			set_time();			// set timer.
		}
		else
			flag ^= BIT8;		// display temperature in Fahrenheit
	}
}

void LPButton(void){
	flag ^= BIT0;
}


/*
 * function name: half_second()
 * description: it is executed every half a second. it has three functions,
 * the first one is to compare the Actual temperature and threshold temperature, if Actual temperature is higher than threshold
 * temperature, buzzer will work
 * the second one is to flicker the threshold temperature bit which is being set.
 * the third one is to flicker the time bit which is being set.
 */
void half_second()
{
	flag &= ~ BIT7;

	// judge actual temperature is higher than threshold temperature. if higher, buzzer will work
	if((actualTemp >= (10*threshTemp)) && (flag & BIT6))
	{
	   	tone(2, 262);
	}
	else if(time >0 && time <= 3)
	{
		tone(2, 262);
	}
	else
	{
		noTone(2);
	}

	//display threshold temperature setting
	if(threshState == 0x01)						//threshold temperature state machine output.
	{
		if (flag & BIT6)
			LCD_display_char(0,4,' ');			//display blank space for half a second
		else
			LCD_display_number(0,3,setTemp);	//display hundred place for half a second
	}
	else if(threshState == 0x02)
	{
		if (flag & BIT6)
			LCD_display_char(0,5,' ');			//display blank space for half a second
		else
			LCD_display_number(0,3,setTemp);	//display decade for half a second
	}
	else if(threshState == 0x03)
	{
		if (flag & BIT6)
			LCD_display_char(0,6,' ');			//display blank space for half a second
		else
			LCD_display_number(0,3,setTemp); 	//display unit's digit for half a second
	}

	// display time setting
	if(timeState == 0x01)
	{
		if (flag & BIT6)
			LCD_display_char(0,11,' ');			//display blank space for half a second
		else
			LCD_display_time(0,8,setTime);;		//display hundred place for half a second
	}

	else if(timeState == 0x02)
	{
		if (flag & BIT6)
			LCD_display_char(0,12,' ');			//display blank space for half a second
		else
			LCD_display_time(0,8,setTime);;		//display hundred place for half a second
	}
	else if(timeState == 0x03)
	{
		if (flag & BIT6)
			LCD_display_char(0,14,' ');			//display blank space for half a second
		else
			LCD_display_time(0,8,setTime);;		//display hundred place for half a second
	}
	else if(timeState == 0x04)
	{
		if (flag & BIT6)
			LCD_display_char(0,15,' ');			//display blank space for half a second
		else
			LCD_display_time(0,8,setTime);;		//display hundred place for half a second
	}
}

/*
 * function name: setTime()
 * description: set the time. the temporary is saved in variable setTime.
 */
void set_time()
{
	if (timeState == 0x01)
	{
		if (setTime/600 >= 6)
		{
			setTime = setTime % 600;
	   	}
	   	else
	   	{
	   		setTime += 600;
	   	}
	}
	else if (timeState  == 0x02)
	{
		if (setTime%600/60 == 9)
	   	{
	   		setTime = setTime-540;
	   	}
	   	else
	   	{
	   		setTime += 60;
	   	}
	}
	else if (timeState  == 0x03)
	{
	   	if (setTime%60/10 == 5)
	   	{
	   		setTime = setTime-50;
	   	}
	   	else
	   	{
	   		setTime += 10;;
	   	}
	}
	else if (timeState  == 0x04)
	{
	   	if (setTime%10 == 9)
	   	{
	   		setTime = setTime-9;
	   	}
	   	else
	   	{
	   		setTime++;
	   	}
	}
}

void time_display()
{
	flag &= ~BIT3;						// flag is reset

	if (timeState == 0)
	{
  		if(time >= 86400 || time <= 0)	// if current is more than 24 hours
  		{
  			time = 0;
  		}
  		else
  		{
  			time--;
  		}
  		LCD_display_time(0,8,time);		// display time on LCD
	}
}

/*
 * function name:setThreshTemp()
 * description: set the threshold temperature. the temporary is saved in variable setTemp.
 */
void setThreshTemp()
{
	if (threshState == 0x01)
	{
		if (setTemp/100 == 9)
		{
			setTemp = setTemp-900;
	   	}
	   	else
	   	{
	   		setTemp += 100;
	   	}
	}
	else if (threshState == 0x02)
	{
		if (setTemp%100/10 == 9)
	   	{
	   		setTemp = setTemp-90;
	   	}
	   	else
	   	{
	   		setTemp += 10;
	   	}
	}
	else if (threshState == 0x03)
	{
	   	if (setTemp%10 == 9)
	   	{
	   		setTemp = setTemp-9;
	   	}
	   	else
	   	{
	   		setTemp ++;
	   	}
	}
}


void ADC_display()
{
	static signed int local_data, far_data;
	signed int temp;
	flag &= ~ BIT4;						// flag is reset
	if (!(flag & BIT5))
	{
		local_data = ADS.ADSread(1);	//read local temperature data,and start a new convertion for far-end temperature sensor.
	}
	else
	{
		far_data = ADS.ADSread(0);		//read far-end temperature,and start a new convertion for local temperature sensor.
		temp = far_data + ADS.localCompensation(local_data);	// transform the local_data to compensation codes of far-end.

		temp = ADS.ADCcode2temp(temp);	// transform the far-end thermocouple codes to temperature.
		Serial.println(temp);
		if(flag & BIT8)					// display temperature in Fahrenheit
		{
			actualTemp = temp * 9 / 5 +320;
			LCD_display_temp(1,5,actualTemp);
			LCD_display_char(1,11,'F');
		}
		else
		{
			actualTemp = temp;
			LCD_display_temp(1,5,actualTemp);
			LCD_display_char(1,11,'C');
		}
	}
}
