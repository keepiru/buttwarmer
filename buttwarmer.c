#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <stdio.h>
#include <util/atomic.h>
#include <util/delay.h>

#define printf_kai(fmt, ...) printf_P(PSTR(fmt) , ##__VA_ARGS__)
#define puts_kai(str) puts_P(PSTR(str))

#define HYST 5

ISR(ADC_vect, ISR_NAKED) { asm("reti"); }

int putchr(char, FILE *);
FILE mystdout = FDEV_SETUP_STREAM(putchr, NULL, _FDEV_SETUP_WRITE);

inline void uart_init(void) {
        UCSR0B |= 1<<TXEN0 | 1<<RXEN0 | 1<<RXCIE0; // Enable TX, RX, RX int
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
	TCCR0A = 1<<COM0A1 | 1<<COM0B1 | 1<<WGM00 ; // PWM enable, mode
	TCCR0B = 1<<CS02;                           // frequency
	DDRD |= 1<<5;
	DDRD |= 1<<6;
} // void pwm_init

uint16_t adc_sample(uint8_t pin) {
        ADCSRA = 1<<ADEN | 1<<ADIE | 7 ; // 7 is the prescaler
        SMCR = 3 ;                       // Sleep mode ADC, enable
        ADMUX = (1<<REFS0) | pin ;       // ref = vcc
	sleep_mode();
	return ADCW;
}

int main (void) {
	int16_t old;
	int16_t sample;
	uart_init();
	pwm_init();
	puts_kai("Booted!");
	_delay_ms(1);
	sei();
	while(1) {
		sample = adc_sample(0) / 4;
		old = OCR0A;
		if (sample < (old - HYST)) OCR0A--;
		if (sample > (old + HYST)) OCR0A++;
		_delay_ms(10);
	}
} // int main

