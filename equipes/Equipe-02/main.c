#include "avr/interrupt.h"
#include "avr/io.h"
#include "serial.h"
#include "util/delay.h"
#include <xc.h>

#define F_CPU 16000000UL

const uint8_t DIGITO[10] = {
    0b00111111, //0
    0b00000110, //1
    0b01011011, //2
    0b01001111, //3
    0b01100110, //4
    0b01101101, //5
    0b01111101, //6
    0b00000111, //7
    0b01111111, //8
    0b01101111  //9
};

// LM35: 10mV/°C, VREF=1.1V → centesimos = raw * 11000 / 1024
uint16_t LM35_celcius(uint16_t adc_raw) {
    uint32_t produto = adc_raw * 11000UL; 
    return produto / 1024UL;
}
void mostrar(uint16_t adc_raw) {
  uint16_t valor = LM35_celcius(adc_raw);  // centésimos de °C

  uint8_t d[4];
  d[0] = (valor / 1000) % 10;  // dezenas de °C
  d[1] = (valor / 100) % 10;   // unidades de °C
  d[2] = (valor / 10) % 10;    // décimos de °C
  d[3] = valor % 10;            // centésimos de °C                     
  PORTB |= (1 << PORTB2) | (1 << PORTB3) | (1 << PORTB4) | (1 << PORTB5);

  // Dígito 0
  PORTD = ~DIGITO[d[0]];
  PORTB &= ~(1 << PORTB2);
  _delay_ms(5);
  PORTB |= (1 << PORTB2); // Desliga

  // Dígito 1 (com Ponto Decimal no PD7)
  PORTD = ~(DIGITO[d[1]] | (1 << PORTD7));
  PORTB &= ~(1 << PORTB3);
  _delay_ms(5);
  PORTB |= (1 << PORTB3);

  // Dígito 2
  PORTD = ~DIGITO[d[2]];
  PORTB &= ~(1 << PORTB4);
  _delay_ms(5);
  PORTB |= (1 << PORTB4);

  // Dígito 3
  PORTD = ~DIGITO[d[3]];
  PORTB &= ~(1 << PORTB5);
  _delay_ms(5);
  PORTB |= (1 << PORTB5);
}

// controle PWM da lâmpada
#define PWM_PERIODO 255

#define SETPOINT 8000 // 20.00 °C
#define HISTERESE 100 // 1.00 °C

uint8_t PWM_duty = 0; // duty cycle atual (0=apagada, 255=máxima)

uint16_t ADC_Result;
uint8_t flag_nova_amostra;

void ADC_config() {
	ADMUX = (1 << REFS1) | (1 << REFS0) |
	  (0 << MUX3) | (0 << MUX2) | (0 << MUX1) |(0 << MUX0); 

  ADCSRA = (1 << ADEN) | (1 << ADATE) |                // ADEN ADATE
           (1 << ADIE) |                               // ADIE
           (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // ADPS=111: /128

  ADCSRB = (1 << ADTS2) | (0 << ADTS1) | (0 << ADTS0); // ADTS=100: Timer0 OVF
  DIDR0 = (1 << ADC0D);                                // ADC0D

  ADCSRA |= (1 << ADSC);                                // inicia primeira conversão
}

void GPIO_config() {
  DDRB = (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB3) | (1 << DDB4) | (1 << DDB5); // PB0-PB5 saídas
  DDRD = 0xFF;                                     // PD0-PD7 saídas (segmentos)
}

// Timer0: ADC trigger via overflow (~61 Hz)
void Timer0_config() {
  TCCR0A = (0 << WGM01) | (0 << WGM00);             // Normal mode
  TCCR0B = (1 << CS02) | (1 << CS00);               // CS=101: prescaler /1024
  // overflow = 16MHz / 1024 / 256 ≈ 61 Hz
}

// PWM da lâmpada no PB1 (OC1A) — Timer1 16 bits
void PWM_config() {
	TCCR1A = (1 << WGM11) | (0 << WGM10)   // CORREÇÃO: WGM10 em 0 para o Modo 14!
	| (1 << COM1A1)               // COM1A=10: não-inversor em OC1A
	| (0 << COM1A0);
	TCCR1B = (1 << WGM13) | (1 << WGM12)
	| (1 << CS11) | (1 << CS10);

	ICR1 = PWM_PERIODO;  // TOP = 255
	OCR1A = 0;           // duty inicial = 0 (desligado)
}

void PWM_set_duty(uint8_t duty) {
  PWM_duty = duty;
  if (duty == 0) {
    TCCR1A &= ~((1 << COM1A1) | (1 << COM1A0)); // desconectado
    PORTB &= ~(1 << PORTB1);                     // força LOW
  } else {
    OCR1A = duty;
    TCCR1A |= (1 << COM1A1);                     // COM1A=10: não-inversor
  }
}
ISR(ADC_vect) {
	static uint8_t contador = 0; // A variável 'static' não perde o valor entre as interrupções
	contador++;
	// Quando atingir 15 leituras (aprox. 4 vezes por segundo)
	if (contador >= 15) {
		ADC_Result = ADC;
		flag_nova_amostra = 1;
		contador = 0; // Zera para recomeçar o ciclo
	}
	//limpando a flag de overflow do Timer0 para armar o próximo gatilho
	TIFR0 = (1 << TOV0);
}

int main(void) {
  Timer0_config();
  GPIO_config();
  PWM_config();
  ADC_config();
  log_init();

  sei();

  uint16_t numero = 0;

  while (1) {

    if (flag_nova_amostra) {
      flag_nova_amostra = 0;
      uint16_t raw = ADC_Result;
      numero = LM35_celcius(raw);

      // liga/desliga
      if (numero < SETPOINT - HISTERESE) {
        PWM_set_duty(225);
      } else if (numero > SETPOINT + HISTERESE) {
        PWM_set_duty(0); 
      }
    }

    log_dec(numero);
    log_string("\n");

    log_tx_release();                    // aguarda serial e libera PD1
    mostrar(ADC_Result);
    log_tx_claim();                      //  UART TX assume
  }
}
