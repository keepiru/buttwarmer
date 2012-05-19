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
static FILE mystdout = FDEV_SETUP_STREAM(putchr, NULL, _FDEV_SETUP_WRITE);

void uart_init(void) {
	UCSR0B |= 1<<TXEN0 | 1<<RXEN0 | 1<<RXCIE0;  // Enable TX, RX, RX int
	UBRR0L |= (F_CPU / (16 * 9600UL)) - 1;  // Set baud rate
	stdout = &mystdout;
} // void uart_init

int putchr(char c, FILE *stream) { // stdio.h wants this to be public
	if (c == '\n')
		putchr('\r', stream);
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
} // void putchr

void pwm_init(void) {
	/*
	 *TCCR1A = 1<<COM1A1; // inv PWM on OC1A/PB1
	 *TCCR1B = 1<<CS11 | 1<<WGM13; // clk/8, phase/freq correct PWM
	 */
	TCCR0A = 1<<COM0A1 | 1<<COM0B1 | 1<<WGM00 ;
	TCCR0B = 1<<CS02;
	DDRD |= 1<<5;
	DDRD |= 1<<6;
} // void pwm_init

void adc_init(void) {
	ADCSRA = 1<<ADEN | 1<<ADIE | 7 ; // 7 is the prescaler
	SMCR = 3 ; // Sleep mode ADC, enable
} // void adc_init


void adc_pin(uint8_t pin) {
	ADMUX = (1<REFS0) | pin ; // ref = vcc
}

int main (void) {
	int16_t old;
	int16_t sample;
	uart_init();
	pwm_init();
	adc_init();
	puts_kai("Booted!");
	_delay_ms(1);
	sei();
	while(1) {
		sleep_mode();
		sample = ADCW / 4;
		old = OCR0A;
		if (sample < (old - HYST))
			OCR0A--;
		if (sample > (old + HYST))
			OCR0A++;
		_delay_ms(100);
	}
} // int main

