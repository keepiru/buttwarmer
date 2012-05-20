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

ISR(ADC_vect, ISR_NAKED) { asm("reti"); }
ISR(__vector_default) { puts_kai("beep"); }

int putchr(char, FILE *);
FILE mystdout = FDEV_SETUP_STREAM(putchr, NULL, _FDEV_SETUP_WRITE);

inline void uart_init(void) {
        UCSR0B |= 1<<TXEN0 ; // Enable TX
        UBRR0L |= (F_CPU / (16 * 9600UL)) - 1;     // Set baud rate
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
	TCCR0A = 1<<COM0A1 | 1<<COM0B1 | 1<<WGM00 | 1<<WGM01; // PWM enable, mode
	TCCR0B = 1<<CS00 | 1<<CS02;                           // frequency
	DDRD |= 1<<5;
	DDRD |= 1<<6;
} // void pwm_init

uint16_t adc_sample(uint8_t pin, uint8_t ref) {
        ADCSRA = 1<<ADEN | 1<<ADIE | 7 ; // 7 is the prescaler
        SMCR = 3 ;                       // Sleep mode ADC, enable
        ADMUX = ref | pin ;
	sleep_mode();
	return ADCW;
}

void update(uint8_t pin, uint8_t *port) {
	int16_t old;
	int16_t sample;
	sample = adc_sample(pin, 1<<REFS0) / 4;
	old = *port;
	if ( abs(sample-old) > HYST ) {
		*port = sample;
		printf_kai("pin %d value %d\r\n", pin, sample);
	}
	if (sample < HYST) *port = 0;
	if (sample > 255-HYST)  *port = 255;
}


void voltage(void) {
	uint16_t v;
	v = adc_sample(5, 1<<REFS0|1<<REFS1);
	printf_kai("v:%d \r", v*11);
}


int main (void) {
	uart_init();
	pwm_init();
	ADCSRA = 1<<ADEN | 1<<ADIE | 7 ;
	printf_kai("Boot %x\r\n", MCUSR);
	_delay_ms(1000);
	sei();
	while(1) {
		update(1, &OCR0A);
		update(2, &OCR0B);
		voltage();
		_delay_ms(200);
	}
} // int main

