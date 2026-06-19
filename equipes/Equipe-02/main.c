#include "avr/interrupt.h"
#include "avr/io.h"
#include "util/delay.h"
#include "serial.h"
#include <xc.h>

#define F_CPU 16000000UL
const uint8_t DIGITO[10] = {
	0b00111111, 
	0b00000110, 
	0b01011011, 
	0b01001111,
	0b01100110, 
	0b01101101, 
	0b01111101, 
	0b00000111,
	0b01111111, 
	0b01101111};
	
void mostrar(uint16_t numero) {
	// Separa os 4 dígitos
	uint8_t d[4];
	d[0] = (numero / 1000) % 10;  // dezena
	d[1] = (numero / 100)  % 10;  // unidade
	d[2] = (numero / 10)   % 10;  // décimo
	d[3] =  numero         % 10;  // centésimo
	PORTB |= (1 << PORTB2) | (1 << PORTB3) | (1 << PORTB4) | (1 << PORTB5);

	// Dígito 0
	PORTD = ~DIGITO[d[0]];
	PORTB &= ~(1 << PORTB2); // Liga apenas PB2 (nível baixo)
	_delay_ms(10);
	PORTB |= (1 << PORTB2);  // Desliga

	// Dígito 1 (com Ponto Decimal no PD0)
	PORTD = ~(DIGITO[d[1]] | ~(1 << PORTD0));
	PORTB &= ~(1 << PORTB3); // Liga apenas PB3
	_delay_ms(10);
	PORTB |= (1 << PORTB3);

	// Dígito 2
	PORTD = ~DIGITO[d[2]];
	PORTB &= ~(1 << PORTB4); // Liga apenas PB4
	_delay_ms(10);
	PORTB |= (1 << PORTB4);

	// Dígito 3
	PORTD = ~DIGITO[d[3]];
	PORTB &= ~(1 << PORTB5); // Liga apenas PB5
	_delay_ms(10);
	PORTB |= (1 << PORTB5);
}

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
  DDRD = 0xFF;
  DDRB = 0xFF;
  
  sei();

  PORTB |= (1 << PORTB1);
 	 uint16_t numero =0;
 

 while (1) {

	 // A leitura do sensor ocorre de tempos em tempos
	 if (flag_nova_amostra) {
		 flag_nova_amostra = 0;
		 uint16_t raw = ADC_Result;
		 numero = LM35_celcius(raw); // Você pode usar essa variável depois
		 	  }
	mostrar(numero);
	log_dec(numero);
	log_string("\n");
 }
}
