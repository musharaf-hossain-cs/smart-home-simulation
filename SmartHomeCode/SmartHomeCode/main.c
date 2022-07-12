#ifndef F_CPU
#define F_CPU 1000000 // 8 MHz clock speed
#endif
#define D4 eS_PORTD4
#define D5 eS_PORTD5
#define D6 eS_PORTD6
#define D7 eS_PORTD7
#define RS eS_PORTB5
#define EN eS_PORTB4

#define CONVERSION_DELAY 0
#define REFV 5.0

//Definition for PORTC
#define DOOR_EN 1
#define DOOR_OPEN 2
// 1 = opened, 0 = closed
#define DOOR_CHOICE 3
//1 = door1, 0 = door2
#define LIGHT2 4
#define LIGHT1 5
#define FAN1 6
#define AC 7

//Definition for PORTB
#define WINDOW_OPEN 6
#define WINDOW_EN 7
#define ALERT 2
#define H2 0

//Definition for PORTA
#define RAIN_IN 7
#define FIRE_OUT 6
#define GAS_OUT 5
#define ALCOHOL 3
#define CO 4

									// These are for RF Receiver
//set desired baud rate
#define BAUDRATE 1200
//calculate UBRR value
#define UBRRVAL ((F_CPU/(BAUDRATE*16UL))-1)
//define receive parameters
#define SYNC 0XAA// synchro signal
#define RADDR 0x44
#define LEDON 0x11//LED on command
#define LEDOFF 0x22//LED off command
#define SHUTDOWN_SM 0x30 // shutdown command
#define SLEEP_SM 0x32 // sleep command
#define START_SM 0x34 // start command
#define STAT_SM 0x36 // show stat command



#include <avr/io.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include "lcd.h" //Downloaded from online

float value = 0.00;
char charValue[10];
unsigned char adch;
unsigned char adcl;
int output = 0;

//flags
int lightOn = 0;
int fanOn = 0;
int acOn = 0;
int acOn2 = 0;
int acOn1 = 0;
int raining = 0;
int fireOut = 0;
int flameableGasOut = 0;
int door1 = 0;
int door2 = 0;
int airPolluted = 0;
int carbonMonoxide = 0;
int hydrogen = 0;
int alcohol = 0;


int pulse1 = 0, pulse2 = 0;
int i1 = 0, i2 = 0;

//control flags
int active = 1;
int sleep = 0;

//Statistics Count
int fanCount = 0;
int acCount = 0;
int lightCount = 0;
int door1Count = 0;
int door2Count = 0;
int windowCount = 0;
int fireAlertCount = 0;
int gasAlertCount = 0;
int airPolutionCount = 0;


int isActive(){
	return active;
}

int isSleep(){
	return sleep;
}

int isShuttedDown(){
	return !(active || sleep);
}

ISR (ADC_vect){
	adcl = ADCL;
	adch = ADCH;
	output = adch << 2;
	adcl = adcl>>6;
	output += adcl;
	value = (output * REFV) / 1024;
}

ISR (INT0_vect){
	if(i1==1){
		TCCR1B = 0;
		pulse1 = TCNT1;
		TCNT1 = 0;
		i1=0;
	}
	if(i1==0){
		TCCR1B |= (1<<CS10);
		i1=1;
	}
}

ISR (INT1_vect){
	if(i2==1){
		TCCR1B = 0;
		pulse2 = TCNT1;
		TCNT1 = 0;
		i2=0;
	}
	if(i2==0){
		TCCR1B |= (1<<CS10);
		i2=1;
	}
}

void checkAC(){
	if(isActive()){
		if(acOn && !(acOn1 || acOn2)){
			Lcd4_Clear();
			Lcd4_Set_Cursor(1,0);
			Lcd4_Write_String("Temp:L, Humi:G");
			Lcd4_Set_Cursor(2,0);
			Lcd4_Write_String("AC OFF");
			
			PORTC &= ~(1<<AC);
			acOn = 0;
		}
		if(!acOn && (acOn1 || acOn2)){
			Lcd4_Clear();
			Lcd4_Set_Cursor(1,0);
			Lcd4_Write_String("Temp:");
			acOn1 == 1 ? Lcd4_Write_String("H"): Lcd4_Write_String("G");
			Lcd4_Write_String(", Humi:");
			acOn2 == 1 ? Lcd4_Write_String("B"): Lcd4_Write_String("G");
			
			Lcd4_Set_Cursor(2,0);
			Lcd4_Write_String("AC ON");
			
			acOn = 1;
			PORTC |= (1<<AC);
		}
	}
	
	
}

void checkTemperature(){
	//set ADC registers
	
	ADMUX = 0b00100000;	//REFS1:0	= 01	->AREF as reference
	//ADLAR		= 1		-> Left Adjust
	//MUX4:0	=00000	-> ADC0 as input
	
	ADCSRA = 0b10001010;	// ADEN = 1		-> enable ADC
	// ADSC = 0		-> don't start conversion yet
	// ADATE = 0	-> disable auto trigger
	// ADIF = 0		-> Interrupt flag
	// ADIE	= 1		-> enable ADC Interrupt
	//ASPS2:0 = 010	-> prescaler = 4
	
	ADCSRA |= (1 << ADSC);	//start a conversion
	
	while((ADCSRA & (1 << ADSC))){;}	//wait until conversion is completed
	_delay_ms(CONVERSION_DELAY);
	
	// for temperature, 10mV for one degree. therefore multiply the value by 100
	float t = value*100;
	int x = t;
	//char str[20];
	
	//rounding
	t = t-x;
	if(t>0.5) x++;
	
	//if(x/100) str[0] = x/100 + 48;
	//else str[0] = ' ';
	//str[1] = (x%100)/10 + 48;
	//str[2] = x%10 + 48;
	//str[3] = ' ';
	//str[4] = 'D';
	//str[5] = 0;
	
	if(isActive() && !fanOn && x>=25){
		Lcd4_Clear();
		
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("Temperature High");
		
		PORTC |= 1<<FAN1; // fan on		
		fanOn = 1;
		acOn1 = 1;
		acCount++;
		fanCount++;
		
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("FAN ON ");
		_delay_ms(10);
	}
	if(isActive() && fanOn && x<25){
		Lcd4_Clear();
		
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("Temperature Low");
		
		PORTC &= ~(1<<FAN1); // fan off
		
		fanOn = 0;
		acOn1 = 0;
		
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("FAN OFF");
		_delay_ms(10);
	}

}

void checkHumidity(){
	//set ADC registers
	
	ADMUX = 0b00100010;	//REFS1:0	= 01	->AREF as reference
	//ADLAR		= 1		-> Left Adjust
	//MUX4:0	=00010	-> ADC2 as input
	
	ADCSRA = 0b10001010;	// ADEN = 1		-> enable ADC
	// ADSC = 0		-> don't start conversion yet
	// ADATE = 0	-> disable auto trigger
	// ADIF = 0		-> Interrupt flag
	// ADIE	= 1		-> enable ADC Interrupt
	//ASPS2:0 = 010	-> prescaler = 4
	
	ADCSRA |= (1 << ADSC);	//start a conversion
	
	while((ADCSRA & (1 << ADSC))){;}	//wait until conversion is completed
	_delay_ms(CONVERSION_DELAY);
	
	// F(0) = 0.76V									=> *100 = 76
	// F(100) = 3.92V								=> *100 = 392
	// difference = 3.16V							=> *100 = 316
	// difference per percentage = 0.0316 V			=> *100 = 3.16
	
	float t = value * 100;
	t = t - 76;
	t = t / 3.16;
	int x = t;
	
	//rounding
	t = t-x;
	if(t>0.5) x++;
	//Humidity Range for Human Comfort : 30% - 50%
	
	if(isActive()){
		if((x<=30 || x>=50) && !acOn2){
			acOn2 = 1;
		}else if(x>30 && x<50 && acOn2){
			acOn2 = 0;
		}
	}

}

void checkLDR(){
	//set ADC registers
	
	ADMUX = 0b00100001;	//REFS1:0	= 01	->AREF as reference
	//ADLAR		= 1		-> Left Adjust
	//MUX4:0	=00001	-> ADC1 as input
	
	ADCSRA = 0b10001010;	// ADEN = 1		-> enable ADC
	// ADSC = 0		-> don't start conversion yet
	// ADATE = 0	-> disable auto trigger
	// ADIF = 0		-> Interrupt flag
	// ADIE	= 1		-> enable ADC Interrupt
	//ASPS2:0 = 010	-> prescaler = 4
	
	ADCSRA |= (1 << ADSC);	//start a conversion
	
	while((ADCSRA & (1 << ADSC))){;}	//wait until conversion is completed
	_delay_ms(CONVERSION_DELAY);
	
	float t = value*100;
	int x = t;
	//char str[20];
	
	//rounding
	t = t-x;
	if(t>0.5) x++;
	
	//str[0] = x/100 + 48;
	//str[1] = '.';
	//str[2] = (x%100)/10 + 48;
	//str[3] = x%10 + 48;
	//str[4] = 'V';
	//str[5] = 0;
	
	if(!lightOn && x<=92 && isActive()){
		Lcd4_Clear();
		
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("Night Time");
		
		PORTC |= 1<<LIGHT1; // light1 on
		PORTC |= 1<<LIGHT2; // light2 on
		
		lightCount++;
		
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("Lights ON");
		
		lightOn = 1;
	}
	
	if(lightOn && x > 92 && isActive()){
		Lcd4_Clear();
		
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("Day Time");
		
		PORTC &= ~(1<<LIGHT1); // light1 off
		PORTC &= ~(1<<LIGHT2); // light2 off
		
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("Lights OFF");
		
		lightOn = 0;
	}
	
}

void USS1(){
	GICR |= (1<<INT0);
	MCUCR |= (1<<ISC00);
	
	int count = 0;
	sei();
	//char show[5];
	
	PORTB |= (1<<PINB3);
	_delay_us(25);
	PORTB &= ~(1<<PINB3);
	_delay_ms(100);
	count = pulse1/(57);
	
	//itoa(count,show,10);
	
	if((door1 == 0) && (count <= 100) && isActive()){
		Lcd4_Clear();
		door1 = 1;
		PORTC |= 1<<DOOR_CHOICE;
		PORTC |= 1<<DOOR_OPEN;
		PORTC |= 1<<DOOR_EN;
		
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("DOOR 1 :");
		//Lcd4_Set_Cursor(1,9);
		//Lcd4_Write_String(show);
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("OPENING...");
		
		_delay_ms(500);
		door1Count++;
		
		PORTC &= ~(1<<PINC1);
		
		
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("OPENEND...");
	}
	if((door1 == 1) && (count > 100) && isActive()){
		Lcd4_Clear();
		door1 = 0;
		PORTC |= 1<<DOOR_CHOICE;
		PORTC &= ~(1<<DOOR_OPEN);
		PORTC |= 1<<DOOR_EN;
		
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("DOOR 1 : ");
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("CLOSING...");
		
		_delay_ms(500);
		PORTC &= ~(1<<DOOR_EN);
		
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("CLOSED... ");
		
	}
	
	

}

void USS2(){
	
	GICR |= (1<<INT1);
	MCUCR |= (1<<ISC10);
	
	int COUNTA = 0;
	sei();
	
	PORTD |= (1<<PIND1);
	_delay_us(25);
	PORTD &= ~(1<<PIND1);
	_delay_ms(100);
	COUNTA = pulse2/(57);
	
	if((door2 == 0) && (COUNTA <= 100) && isActive()){
		Lcd4_Clear();
		door2 = 1;
		PORTC &= ~(1<<DOOR_CHOICE);
		PORTC |= 1<<DOOR_OPEN;
		PORTC |= 1<<DOOR_EN;

		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("DOOR 2: ");
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("OPENING...");
		
		_delay_ms(500);
		door2Count++;
		
		PORTC &= ~(1<<DOOR_EN);
		
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("OPENEND...");
	}
	if((door2 == 1) && (COUNTA > 100) && isActive()){
		Lcd4_Clear();
		door2 = 0;
		PORTC &= ~(1<<DOOR_CHOICE);
		PORTC &= ~(1<<DOOR_OPEN);
		PORTC |= 1<<DOOR_EN;
		
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("DOOR 2 : ");
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("CLOSING...");
		
		_delay_ms(500);
		PORTC &= ~(1<<DOOR_EN);
		
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("CLOSED... ");
	}
	
}

void checkRain(){
	int A7;
	if(PINA & (1<<RAIN_IN)){
		A7 = 1;
	}
	else{
		A7 = 0;
	}
	
	if(!raining && A7 && isActive()){
		Lcd4_Clear();
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("Raining.");
		
		raining = 1; // set rain flag
		
		PORTB &= ~(1<<WINDOW_OPEN); // Closing windows
		PORTB |= 1<<WINDOW_EN; //Enable Window motor
		
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("Windows closing");
		
		_delay_ms(500);
		
		PORTB &= ~(1<<WINDOW_EN); // Disable Window Motor
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("Windows closed.");
		
	}
	
	if(raining && !A7 && isActive()){
		Lcd4_Clear();
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("Not Raining.");
		
		raining = 0; // reset rain flag
		
		PORTB |= 1<<WINDOW_OPEN; // Opening windows
		PORTB |= 1<<WINDOW_EN; //Enable Window motor
		
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("Windows Opening");
		windowCount++;
		
		_delay_ms(500);
		
		PORTB &= ~(1<<WINDOW_EN); // Disable Window Motor
		Lcd4_Set_Cursor(2,0);
		Lcd4_Write_String("Windows Opened.");
	}
}

void checkFire(){
	int A6;
	if(PINA & (1<<FIRE_OUT)){
		A6 = 1;
	}
	else{
		A6 = 0;
	}
	
	if(!fireOut && A6 && !isShuttedDown()){
		Lcd4_Clear();
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("Fire Alert On");
		
		fireOut = 1; // set fireOut flag
		PORTB |= (1<<ALERT);
		fireAlertCount++;
		_delay_ms(10);
		
	}
	
	if(fireOut && !A6 && !isShuttedDown()){
		Lcd4_Clear();
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("Fire Alert Off");
		
		fireOut = 0; // reset fireOut flag
		PORTB &= ~(1<<ALERT);
		_delay_ms(10);
	}
}

void checkFlameableGas(){
	int A5;
	if(PINA & (1<<GAS_OUT)){
		A5 = 1;
	}
	else{
		A5 = 0;
	}
	
	if(!flameableGasOut && A5 && !isShuttedDown()){
		Lcd4_Clear();
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("Gas Alert On");
		
		flameableGasOut = 1; // set gasOut flag
		PORTB |= (1<<ALERT);
		gasAlertCount++;
		_delay_ms(10);
		
	}
	
	if(flameableGasOut && !A5 && !isShuttedDown()){
		Lcd4_Clear();
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String("Gas Alert Off");
		
		flameableGasOut = 0; // reset gasOut flag
		PORTB &= ~(1<<ALERT);
		_delay_ms(10);
	}
}

void checkAirPolution(){
	int alc = 0;
	int h2 = 0;
	int co = 0;

	if( PINA & (1<<ALCOHOL) ){
		alc = 1;
	}
	if( PINB & (1<<H2) ){
		h2 = 2;
	}
	if( PINA & (1<<CO) ){
		co = 4;
	}
	
	int c = alc + h2 + co;
	
	if(c != airPolluted && isActive()){
		if(c){
			Lcd4_Clear();
			Lcd4_Set_Cursor(1,0);
			Lcd4_Write_String("Air polluted!");
			Lcd4_Set_Cursor(2,0);
			if(co){
				Lcd4_Write_String("CO");
			}
			if(h2){
				if(co) Lcd4_Write_String(", H2");
				else Lcd4_Write_String("H2");
			}
			if(alc){
				if(co || h2) Lcd4_Write_String(", Alcohol");
				else Lcd4_Write_String("Alcohol");
			}
			airPolutionCount++;
		}
		else{
			Lcd4_Clear();
			Lcd4_Set_Cursor(1,0);
			Lcd4_Write_String("No Air Polution");
		}
		airPolluted = c;
	}
	
	
	
}

// For Remote Control
void StartUp(){
	Lcd4_Clear();
	Lcd4_Set_Cursor(1,0);
	Lcd4_Write_String("Start Up!");
	active = 1;
	sleep = 0;
	_delay_ms(100);
}

void ShutDown(){
	Lcd4_Clear();
	Lcd4_Set_Cursor(1,0);
	Lcd4_Write_String("ShutDown!");
	active = 0;
	sleep = 0;
	_delay_ms(100);
}

int printStat(int value){
	char str[10];
	if(value>=100){
		Lcd4_Write_String("Many ");
		return 5;
	}
	else if(value>=10){
		str[0] = (value/10) + 48;
		str[1] = value%10 + 48;
		str[3] = 0;
		Lcd4_Write_String(str);
		return 3;
	}
	else{
		str[0] = value + 48;
		str[1] = 0;
		Lcd4_Write_String(str);
		return 2;
	}
}

void ShowStat(){
	int line;
	int counts[10];
	counts[0] = fanCount;
	counts[1] = acCount;
	counts[2] = lightCount;
	counts[3] = door1Count;
	counts[4] = door2Count;
	counts[5] = windowCount;
	counts[6] = fireAlertCount;
	counts[7] = gasAlertCount;
	counts[8] = airPolutionCount;
	char msg[9][20] = {
		"Fan Turned On   ",
		"AC Turned On    ",
		"Light Turned On ",
		"Door 1 Opened   ",
		"Door 2 Opened   ",
		"Windows Opened  ",
		"Fire Warning    ",
		"Gas Leak Warning",
		"Air Polluted    "
	};
	Lcd4_Clear();
	Lcd4_Set_Cursor(1,0);
	Lcd4_Write_String("Show Stat!");
	_delay_ms(100);
	for(int i=0;i<9;i++){
		Lcd4_Clear();
		Lcd4_Set_Cursor(1,0);
		Lcd4_Write_String(msg[i]);
		Lcd4_Set_Cursor(2,0);
		line = printStat(counts[i]);
		Lcd4_Set_Cursor(2,line);
		Lcd4_Write_String("Times     ");
		_delay_ms(100);
	}
}

void SleepSM(){
	Lcd4_Clear();
	Lcd4_Set_Cursor(1,0);
	Lcd4_Write_String("Sleep SM!");
	active = 0;
	sleep = 1;
	_delay_ms(100);
}




// for RF Module

void USART_Init(void)
{
	//Set baud rate
	UBRRL=(uint8_t)UBRRVAL;     //low byte
	UBRRH=(UBRRVAL>>8);   //high byte
	//Set data frame format: asynchronous mode,no parity, 1 stop bit, 8 bit size
	UCSRC=(1<<URSEL)|(0<<UMSEL)|(0<<UPM1)|(0<<UPM0)|
	(0<<USBS)|(0<<UCSZ2)|(1<<UCSZ1)|(1<<UCSZ0);
	//Enable Transmitter and Receiver and Interrupt on receive complete
	UCSRB=(1<<RXEN)|(1<<RXCIE);//|(1<<TXEN);
	//enable global interrupts
}
uint8_t USART_vReceiveByte(void)
{
	// Wait until a byte has been received
	while((UCSRA&(1<<RXC)) == 0);
	// Return received data
	return UDR;
}
ISR(USART_RXC_vect)
{
	//define variables
	uint8_t raddress, data, chk;//transmitter address
	//receive destination address
	raddress=USART_vReceiveByte();
	//receive data
	data=USART_vReceiveByte();
	//receive checksum
	chk=USART_vReceiveByte();
	//compare received checksum with calculated
	if(chk==(raddress+data))//if match perform operations
	{
		//if transmitter address match
		if(raddress==RADDR)
		{
			if(data == START_SM){
				StartUp();
			}
			else if(data == SHUTDOWN_SM){
				ShutDown();
			}
			else if(data == STAT_SM){
				ShowStat();
			}
			else if(data == SLEEP_SM){
				SleepSM();
			}
			else if(data==LEDON)
			{
				PORTC&=~(1<<0);//LED ON
			}
			else if(data==LEDOFF)
			{
				PORTC|=(1<<0);//LED OFF
			}
			
		}
	}
}
void Main_Init(void)
{
	PORTC|=(1<<0);//LED OFF
	//enable global interrupts
	sei();
}


int main(void)
{
	DDRA = 0b00000000;
	DDRB = 0b11111110;
	DDRC = 0b11111111;
	DDRD = 0b11110010;
	
	
	Lcd4_Init();
	Main_Init();
	USART_Init();
	
	sei();
	
	
	while(1){
		checkTemperature();
		checkHumidity();
		checkAC();
		checkLDR();
		USS1();
		USS2();
		checkRain();
		checkFire();
		checkFlameableGas();
		checkAirPolution();
	}
	
}


