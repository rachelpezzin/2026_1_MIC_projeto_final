#include "avr/interrupt.h"
#include "avr/io.h"
#include "serial_logger.h"
#include <xc.h>

#define F_CPU 16000000UL

void ADC_config() {
  ADMUX = (1 << REFS1) | (1 << REFS0) | // REFS=11: 1.1V
          (0 << MUX3) | (0 << MUX2) | (0 << MUX1) |
          (0 << MUX0); // MUX=0000: ADC0

  ADCSRA = (1 << ADEN) | (1 << ADATE) |                // ADEN ADATE
           (1 << ADIE) |                               // ADIE
           (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // ADPS=111: /128

  ADCSRB = (1 << ADTS2) | (1 << ADTS1) | (0 << ADTS0); // ADTS=110: Timer1 OVF
  DIDR0 = (1 << ADC0D);                                // ADC0D
}

void GPIO_config() { DDRB = (1 << DDB0) | (1 << DDB1) | (1 << DDB2); }

void Timer1_config() {
  TCCR1A = (0 << WGM11) | (0 << WGM10); // WGM=000: Normal
  TCCR1B =
      (0 << WGM12) | (0 << CS12) | (1 << CS11) | (0 << CS10); // CS=011: /64
  OCR1A = 62500;
  OCR1B = 62500;
  TIMSK1 = (1 << TOIE1); // TOIE1
}

// LM35: 10mV/°C, VREF=1.1V → decimos = raw * 1100 / 1024
uint16_t LM35_celcius(uint16_t adc_raw) {
  return (uint16_t)(((uint32_t)adc_raw * 1100UL) / 1024UL);
}

uint16_t ADC_Result;
volatile uint8_t flag_nova_amostra;

ISR(ADC_vect) {
  ADC_Result = ADC;
  flag_nova_amostra = 1;
}

ISR(TIMER1_OVF_vect) { PORTB ^= (1 << PORTB0); }

int main(void) {
  Timer1_config();
  GPIO_config();
  ADC_config();
  log_init();
  sei();

  PORTB |= (1 << PORTB1);
  log_string("Lampada ON\r\n");

  while (1) {
    if (flag_nova_amostra) {
      flag_nova_amostra = 0;
      uint16_t raw = ADC_Result;
      uint16_t t = LM35_celcius(raw);

      log_string("ADC=");
      log_dec(raw);
      log_string(" T=");
      log_dec(t / 10);
      log_string(".");
      log_dec(t % 10);
      log_string(" C\r\n");
    }
  }
}
