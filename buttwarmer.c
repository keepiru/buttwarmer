#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <util/atomic.h>
#include <util/delay.h>

#define printf_kai(fmt, ...) printf_P(PSTR(fmt) , ##__VA_ARGS__)
#define puts_kai(str) puts_P(PSTR(str))

#define HYST 2
#define DECAY_RATE 0.01
#define MILLIVOLTS_PER_DIV 25
#define MILLIVOLTS_SHUTDOWN 10200

ISR(ADC_vect, ISR_NAKED) { asm("reti"); }
ISR(__vector_default) { puts_kai("beep"); }

int putchr(char, FILE *);
FILE mystdout = FDEV_SETUP_STREAM(putchr, NULL, _FDEV_SETUP_WRITE);

inline void uart_init(void) {
#define BAUD 1200
#include <util/setbaud.h>
        UBRR0 = UBRR_VALUE;
	UCSR0B |= 1<<TXEN0 ; // Enable TX
	stdout = &mystdout;
} // void uart_init

int putchr(char c, FILE *stream) { // stdio.h wants this to be public
	if (c == '\n')
		putchr('\r', stream);
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
} // void putchr

inline void pwm_init(void) {
	TCCR0A = 1<<COM0A1 | 1<<COM0B1 | 1<<WGM00 ; // PWM enable, mode
	TCCR1A = 1<<COM1A1 | 1<<COM1B1 | 1<<WGM10 ; // PWM enable, mode
	TCCR0B = 1<<CS00 | 1<<CS01;                 // frequency
	TCCR1B = 1<<CS10 | 1<<CS11;                 // frequency
	TCNT1 = 0;
	TCNT0 = 128;
	DDRD |= 1<<5 | 1<<6;
	DDRB |= 1<<1 | 1<<2;
} // void pwm_init

uint16_t adc_sample(uint8_t pin, uint8_t ref) { // sample pin with reference voltage bitfield ref
        ADCSRA = 1<<ADEN | 1<<ADIE | 4 ; // enable, int, prescaler
        SMCR = 3 ;                       // Sleep mode ADC, enable
        ADMUX = ref | pin ;
	_delay_ms(10);
	sleep_mode();
	return ADCW;
}

void pwm_update(uint8_t pin, volatile uint8_t *port) { // Sample analog pin and update PWM OCR on *port
	int16_t old;
	uint16_t sample;
	sample = adc_sample(pin, 1<<REFS0) / 4;
	old = *port;
	if ( abs(sample-old) > HYST ) {
		*port = sample;
		//printf_kai("a%d>%d\r\n", pin, sample);
	}
	if (sample < HYST) *port = 0;
	if (sample > 255-HYST)  *port = 255;
}

void shutdown(void) { // set all outputs low and halt
	puts_kai("Shutdown");
	while (1) { 
		OCR0A=OCR0B=OCR1A=OCR1B=TCCR0A=TCCR1A=PORTD=PORTB=0; // Shut down
		DDRB |= 1<<7;
		PORTB = 1<<7; // Error light
		_delay_ms(1000);
		cli();
		SMCR = 1<<SM1; // power-down
		sleep_mode();
	}
}

void monitor_voltage(void) { // sample voltage, maintain decaying average, shut down if too low
	static uint16_t millivolts_avg=13500;
	uint16_t sample;
	sample = adc_sample(5, 1<<REFS0|1<<REFS1) * MILLIVOLTS_PER_DIV;
	millivolts_avg = (millivolts_avg * (1-DECAY_RATE)) + (sample * DECAY_RATE); // decaying average
	printf_kai("v:%u %u\r", sample, millivolts_avg);
	if (millivolts_avg < MILLIVOLTS_SHUTDOWN) shutdown();
}

int main (void) {
	uart_init();
	pwm_init();
	printf_kai("Boot %x\r\n", MCUSR);
	_delay_ms(1000);
	sei();
	while(1) {
		pwm_update(1, &OCR0A);
		pwm_update(2, &OCR0B);
		pwm_update(3, &OCR1AL);
		pwm_update(4, &OCR1BL);
		monitor_voltage();
		_delay_ms(200);
	}
} // int main

