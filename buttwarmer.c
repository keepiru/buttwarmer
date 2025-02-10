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
ISR(__vector_default) { puts_kai("beep"); }  // This shouldn't happen. :)

int putchr(char, FILE *);
FILE mystdout = FDEV_SETUP_STREAM(putchr, NULL, _FDEV_SETUP_WRITE);

/*
 * @brief initialize the serial port.
 */
inline void uart_init(void) {
#define BAUD 2400
#include <util/setbaud.h>
	OSCCAL = 0x89;       // Oscillator calibration
	UBRR0 = UBRR_VALUE;  // Set the baud rate.  This comes from the included lib.
	UCSR0B |= 1<<TXEN0 ; // Enable TX
	stdout = &mystdout;
} // void uart_init

/*
 * @brief Output a single character to the serial port.
 * @note stdio.h wants this to be public
 * @param[in] c the character to print.
 * @param[in] stream The theoretical stream.  Not actually used.
 * @return 0 This implementation will block until it succeeds.
 */
int putchr(char c, FILE *stream) {
	if (c == '\n')
		putchr('\r', stream);
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
} // void putchr

/*
 * @brief Initialize the hardware PWMs.
 */
inline void pwm_init(void) {
	TCCR0A = 1<<COM0A1 | 1<<COM0B1 | 1<<WGM00 ; // PWM enable, mode
	TCCR1A = 1<<COM1A1 | 1<<COM1B1 | 1<<WGM10 ; // PWM enable, mode
	TCCR0B = 1<<CS00 | 1<<CS01;                 // frequency
	TCCR1B = 1<<CS10 | 1<<CS11;                 // frequency
	TCNT1 = 0;                                  // initialize the counter
	TCNT0 = 128;                                // And this one is on the alternate phase
	DDRD |= 1<<5 | 1<<6;
	DDRB |= 1<<1 | 1<<2;
} // void pwm_init

/*
 * @brief Sample an ADC pin.
 * @param[in] pin The analog pin number to sample.
 * @param[in] ref A bitfield for the voltage reference. Typically:
 *                1<<REFS0              VCC
 *                1<<REFS0 | 1<<REFS1   Internal 1.1V voltage reference
 *                FIXME: This can easily be abstracted.
 * @return The raw sample in ADC counts.
 */
uint16_t adc_sample(uint8_t pin, uint8_t ref) { // sample pin with reference voltage bitfield ref
	ADCSRA = 1<<ADEN | 1<<ADIE | 4 ; // enable, int, prescaler
	SMCR = 3 ;                       // Sleep mode ADC, enable
	ADMUX = ref | pin ;              // Where to sample from
	_delay_ms(10);                   // Take a moment to settle
	sleep_mode();                    // Enter ADC sleep mode, wait for irq
	return ADCW;
}

/*
 * @brief Sample analog pin and update output PWM.
 * @note This is the heart of the main loop.
 * @param[in] pin The analog input pin to sample.  This is the pin connected to the potentiometer.
 * @param[in] lockout_pin Each seat may be turned on or off.  Disable PWMs if this seat is off.
 * @param[out] port Pointer to the PWM output register.
 */
void pwm_update(uint8_t pin, uint8_t lockout_pin, volatile uint8_t *port) {
	int16_t old;
	uint16_t knob, voltage;

	// First check the voltage on the enable switch for this side.
	voltage = adc_sample(lockout_pin, 1<<REFS0|1<<REFS1) * MILLIVOLTS_PER_DIV;
	// If the switch for this side is off, ignore the knob positions and
	// just disable this output.
	if (voltage < MILLIVOLTS_OFF) {
		*port = 0;
		return;
	}

	knob = adc_sample(pin, 1<<REFS0) / 4;  // divide by 4 from 10-bit ADC to 8 bit PWM
	old = *port;

	// Add some hysteresis.  Ignore small changes.
	if ( abs(knob-old) > HYST ) {
		*port = knob;
	}

	// Add a small dead zone at the top and bottom of the potentiometer.
	// The deadband is the same size as the hysteresis width.
	if (knob < HYST) *port = 0;
	if (knob > 255-HYST)  *port = 255;
}

/*
 * @brief Turn off all outputs and halt the system.
 * @note This is typically done when the battery is getting too low.
 */
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

/*
 * @brief Monitor the battery voltage, and shut down if battery is low
 */
void monitor_voltage(void) { // sample voltage, maintain decaying average, shut down if too low
	static uint16_t millivolts_avg=13500;
	uint16_t sample; // The sampled value in millivolts.
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
	if (millivolts_avg < MILLIVOLTS_SHUTDOWN) {
		shutdown();
	}
}

int main (void) {
	uart_init();
	pwm_init();
	printf_kai("Boot %x\r\n", MCUSR);
	_delay_ms(100);
	sei();
	while(1) {
		pwm_update(1, 5, &OCR0A);  // Left top
		pwm_update(2, 5, &OCR0B);  // Left bottom
		pwm_update(3, 0, &OCR1AL); // Right top
		pwm_update(4, 0, &OCR1BL); // Right bottom
		monitor_voltage();
		_delay_ms(200);
	}
} // int main

