/*
 * Remote.c
 *
 * Created: 17-Jul-21 10:41:32 PM
 * Author : HP
 */ 


#include <avr/io.h>

#ifndef F_CPU
//define cpu clock speed if not defined
#define F_CPU 1000000
#endif
#include <util/delay.h>
//set desired baud rate
#define BAUDRATE 1200
//calculate UBRR value
#define UBRRVAL ((F_CPU/(BAUDRATE*16UL))-1)
//define receive parameters
#define SYNC 0XAA// synchro signal
#define RADDR 0x44
#define LEDON 0x11//switch led on command
#define LEDOFF 0x22//switch led off command
#define SHUTDOWN_SM 0x30 // shutdown command
#define SLEEP_SM 0x32 // sleep command
#define START_SM 0x34 // start command
#define STAT_SM 0x36 // show stat command


void USART_Init(void)
{
	//Set baud rate
	UBRRL=(uint8_t)UBRRVAL;     //low byte
	UBRRH=(UBRRVAL>>8);   //high byte
	//Set data frame format: asynchronous mode,no parity, 1 stop bit, 8 bit size
	UCSRC=(1<<URSEL)|(0<<UMSEL)|(0<<UPM1)|(0<<UPM0)|
	(0<<USBS)|(0<<UCSZ2)|(1<<UCSZ1)|(1<<UCSZ0);
	//Enable Transmitter and Receiver and Interrupt on receive complete
	UCSRB=(1<<TXEN);
}
void USART_vSendByte(uint8_t u8Data)
{
	// Wait if a byte is being transmitted
	while((UCSRA&(1<<UDRE)) == 0);
	// Transmit data
	UDR = u8Data;
}
void Send_Packet(uint8_t addr, uint8_t cmd)
{
	USART_vSendByte(SYNC);//send synchro byte
	USART_vSendByte(addr);//send receiver address
	USART_vSendByte(cmd);//send increment command
	USART_vSendByte((addr+cmd));//send checksum
}


void ShutDown(){
	Send_Packet(RADDR, SHUTDOWN_SM);
	_delay_ms(100);
}

void StartUp(){
	Send_Packet(RADDR, START_SM);
	_delay_ms(100);
}

void Sleep_SM(){
	Send_Packet(RADDR, SLEEP_SM);
	_delay_ms(100);
}

void ShowStat(){
	Send_Packet(RADDR, STAT_SM);
	_delay_ms(100);
}

int main(void)
{
	DDRB = 0b00000000;
	USART_Init();
	
	while(1)
	{//endless transmission
		char command = PINB;
		if(command & 0x01){ // start
			StartUp();
		}
		if(command & 0x02){ // sleep
			Sleep_SM();
		}
		if(command & 0x04){ // shutdown
			ShutDown();
		}
		if(command & 0x08){ // show stat
			ShowStat();
		}
		
	}
	return 0;
}