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
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define HYST 2
#define DECAY_RATE 0.01
#define MILLIVOLTS_PER_DIV 25
#define MILLIVOLTS_SHUTDOWN 10200
#define MILLIVOLTS_OFF 1000

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

/*
 * @brief Sample analog pin and update output PWM.
 * @note This is the heart of the main loop.
 * @param[in] pin The analog input pin to sample.  This is the pin connected to the potentiometer.
 * @param[in] lockout_pin Power input sent through a voltage divider, used to measure battery level.
 * @param[out] port Pointer to the PWM output register.
 */
void pwm_update(uint8_t pin, uint8_t lockout_pin, volatile uint8_t *port) {
	int16_t old;
	uint16_t knob, voltage;
	knob = adc_sample(pin, 1<<REFS0) / 4;
	voltage = adc_sample(lockout_pin, 1<<REFS0|1<<REFS1) * MILLIVOLTS_PER_DIV;
	old = *port;
	if ( abs(knob-old) > HYST ) {
		*port = knob;
		//printf_kai("a%d>%d\r\n", pin, sample);
	}
	if (knob < HYST) *port = 0;
	if (knob > 255-HYST)  *port = 255;
	if (voltage < MILLIVOLTS_OFF) *port = 0;
}

void shutdown(void) { // set all outputs low and halt
	puts_kai("Shutdown");
	while (1) { 
		OCR0A=0;
		OCR0B=0;
		OCR1A=0;
		OCR1B=0;
		TCCR0A=0;
		TCCR1A=0;
		PORTD=0;
		PORTB=0;
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
	// Power is sourced from either seat circuit.  We monitor those
	// independently because one may be switched off, making it look like
	// the battery is dead.  High PWM levels also cause some voltage drop.
	// Take the max of the two, since it's most likely to be an accurate
	// read.
	sample = max(
		(adc_sample(5, 1<<REFS0|1<<REFS1) * MILLIVOLTS_PER_DIV),
		(adc_sample(0, 1<<REFS0|1<<REFS1) * MILLIVOLTS_PER_DIV)
	);
	millivolts_avg = (millivolts_avg * (1-DECAY_RATE)) + (sample * DECAY_RATE); // decaying average
	printf_kai("v:%u %u\r", sample, millivolts_avg);
	if (millivolts_avg < MILLIVOLTS_SHUTDOWN) shutdown();
}

int main (void) {
	uart_init();
	pwm_init();
	printf_kai("Boot %x\r\n", MCUSR);
	_delay_ms(100);
	sei();
	while(1) {
		pwm_update(1, 5, &OCR0A);
		pwm_update(2, 5, &OCR0B);
		pwm_update(3, 0, &OCR1AL);
		pwm_update(4, 0, &OCR1BL);
		monitor_voltage();
		_delay_ms(200);
	}
} // int main

