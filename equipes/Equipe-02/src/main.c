#define F_CPU 16000000UL

#include "avr/interrupt.h"
#include "avr/io.h"
#include "display.h"
#include "lm35.h"
#include "serial.h"
#include <stdlib.h>
#include <xc.h>

// ==========================================================================
// Bloco 1: Núcleo — GPIO + PWM + Controle ON-OFF
// ==========================================================================
#define HISTERESE 200 // ±2.00 °C (centésimos)

void GPIO_config() {
  DDRB = (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB3) | (1 << DDB4) |
         (1 << DDB5);
  DDRD = 0xFC; // PD2–PD7 saídas, PD0/PD1 livres (UART)
  DDRC |=
      (1 << DDC1) | (1 << DDC2) | (1 << DDC5); // PC1/PC2 seg. A/B, PC5 buzzer
}

// --- PWM lâmpada no PB1 (OC1A), Timer1 16 bits, modo 14 rápido ---
void PWM_config() {
  TCCR1A = (1 << WGM11) | (0 << WGM10) | (1 << COM1A1) |
           (0 << COM1A0); // não-inversor
  TCCR1B =
      (1 << WGM13) | (1 << WGM12) | (1 << CS11) | (1 << CS10); // prescaler /64
  ICR1 = 255;                                                  // TOP
  OCR1A = 0;                                                   // duty inicial 0
}

void PWM_set_duty(uint8_t duty) {
  if (duty == 0) {
    TCCR1A &= ~((1 << COM1A1) | (1 << COM1A0)); // desconecta OC1A
    PORTB &= ~(1 << PORTB1);                    // LOW manual
  } else {
    OCR1A = duty;            // match 0–255
    TCCR1A |= (1 << COM1A1); // reconecta
  }
}

// ==========================================================================
// Bloco 2: Desafio extra — Amostragem por Timer/Interrupção (2 pts)
// ==========================================================================
#define ADC_AMOSTRAS 61 // 61 Hz / 61 = 1 Hz (decimação)

uint16_t ADC_Result;         // média do bloco, pronta pra main
uint8_t ADC_lido;            // flag: nova média disponível
uint8_t ADC_contador = 0;    // amostras acumuladas no bloco
uint16_t ADC_acumulador = 0; // soma das amostras do bloco

// Timer0: overflow a ~61 Hz dispara ADC (normal mode, /1024)
void Timer0_config() {
  TCCR0A = (0 << WGM01) | (0 << WGM00);
  TCCR0B = (1 << CS02) | (1 << CS00);
}

// ADC: trigger por Timer0 OVF, VREF=1.1V, canal ADC0
void ADC_config() {
  ADMUX = (1 << REFS1) | (1 << REFS0) | (0 << MUX3) | (0 << MUX2) |
          (0 << MUX1) | (0 << MUX0);
  ADCSRA = (1 << ADEN) | (1 << ADATE) | (1 << ADIE) | (1 << ADPS2) |
           (1 << ADPS1) | (1 << ADPS0);                // prescaler /128
  ADCSRB = (1 << ADTS2) | (0 << ADTS1) | (0 << ADTS0); // Timer0 OVF
  DIDR0 = (1 << ADC0D);
  ADCSRA |= (1 << ADSC);
}

ISR(ADC_vect) {
  ADC_acumulador += ADC;
  ADC_contador++;

  if (ADC_contador >= ADC_AMOSTRAS) {
    ADC_Result = ADC_acumulador / ADC_AMOSTRAS;
    ADC_lido = 1;
    ADC_contador = 0;
    ADC_acumulador = 0;
  }
  TIFR0 = (1 << TOV0); // rearma trigger do Timer0
}

// ==========================================================================
// Bloco 3: Desafio extra — Display local 7 segmentos (2 pts)
//    Arquivos: include/display.h + src/display.c
//    Mostra temperatura; ao alterar setpoint/alarme exibe por 3 s.
//    Prioridade: alarme > setpoint > temperatura.
// ==========================================================================
#define CONF_DISPLAY_MS 3000
#define CONF_DISPLAY_TICKS (CONF_DISPLAY_MS / 10) // 300 ticks = 3 s
uint16_t conf_setpoint_ticks = 0; // >0 = exibe setpoint; decrementado no Timer2

// ==========================================================================
// Bloco 6: Desafio extra — Sistema de alarme configurável (2 pts)
//    Buzzer intermitente em PC5 (50ms on / 50ms off via Timer2).
//    Configuração via serial: A<valor> (ex: A2500 = 25.00 °C), A0 desativa.
// ==========================================================================
#define ALARME_TICKS 10 // (50ms on + 50ms off) / 10ms = 10 ticks

uint16_t alarme_max = 0;        // limite do alarme (0 = desativado)
uint8_t cmd_prefix = 0;         // 0=ignora, 'S'=setpoint, 'A'=alarme
uint8_t alarme_ativo = 0;       // flag: temperatura acima do limite
uint16_t conf_alarme_ticks = 0; // >0 = exibe alarme; decrementado no Timer2

// ==========================================================================
// Bloco 4: Desafio extra — Setpoint via botão físico (1 pt)
//    PC3 (+0.50 °C) e PC4 (-0.50 °C). Debounce por borda + contagem no Timer2.
// ==========================================================================
#define DEBOUNCE_MS 50
#define DEBOUNCE_TICKS (DEBOUNCE_MS / 10) // 5 ticks de 10ms
uint8_t btn_c3_dis = 0, btn_c4_dis = 0;   // flag: pressionamento confirmado

// Timer2: CTC, OCR2A=TOP, ~100 Hz (10 ms) — base do debounce
void Timer2_config() {
  TCCR2A = (1 << WGM21);
  TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20); // /1024
  OCR2A = 155;                                      // (16M/1024/100)-1
  TIMSK2 = (1 << OCIE2A);
}

ISR(TIMER2_COMPA_vect) {
  static uint8_t pc3_contador = 0, pc4_contador = 0;
  static uint8_t pc3_prev = 1, pc4_prev = 1;
  uint8_t pc3 = (PINC >> PINC3) & 1; // 1=solto, 0=pressionado
  uint8_t pc4 = (PINC >> PINC4) & 1;

  // --- PC3: borda de descida (1→0) dispara contador ---
  if (pc3_prev == 1 && pc3 == 0)
    pc3_contador = DEBOUNCE_TICKS;
  if (pc3_contador > 0) {
    pc3_contador--;
    if (pc3_contador == 0 && pc3 == 0)
      btn_c3_dis = 1;
  }
  pc3_prev = pc3;

  // --- PC4 ---
  if (pc4_prev == 1 && pc4 == 0)
    pc4_contador = DEBOUNCE_TICKS;
  if (pc4_contador > 0) {
    pc4_contador--;
    if (pc4_contador == 0 && pc4 == 0)
      btn_c4_dis = 1;
  }
  pc4_prev = pc4;

  // --- timers de exibição (3 s) ---
  if (conf_setpoint_ticks > 0)
    conf_setpoint_ticks--;
  if (conf_alarme_ticks > 0)
    conf_alarme_ticks--;

  // --- buzzer intermitente PC5 (50ms on / 50ms off) ---
  static uint8_t buzzer_tick = 0;
  static uint8_t buzzer_state = 0;

  if (alarme_ativo) {
    if (buzzer_tick > 0) {
      buzzer_tick--;
    } else {
      buzzer_tick = ALARME_TICKS;
      buzzer_state = !buzzer_state;
      if (buzzer_state)
        PORTC |= (1 << PORTC5);
      else
        PORTC &= ~(1 << PORTC5);
    }
  } else {
    PORTC &= ~(1 << PORTC5);
    buzzer_tick = 0;
    buzzer_state = 0;
  }
}

// ==========================================================================
// Bloco 5: Núcleo — Comunicação serial (UART TX + RX)
//    Arquivos: include/serial.h + src/serial.c
// ==========================================================================
uint16_t setpoint_valor = 1800; // 18.00 °C inicial

void log_config(char prefix, uint16_t val); // forward (usada na ISR RX)

// --- RX: recebe dígitos, Enter confirma e atualiza setpoint ---
char setpoint_buffer[8];
uint8_t setpoint_idx = 0;

// --- RX: protocolo serial ---
//   S<valor>  → setpoint  (ex: S1800 = 18.00 °C)
//   A<valor>  → alarme    (ex: A2500 = 25.00 °C)
//   sem prefixo → ignorado
//   eco TX: ">S:18.00\n" ou ">A:25.00\n"
ISR(USART_RX_vect) {
  char c = UDR0;

  if (c == 'S' || c == 's') {
    cmd_prefix = 'S';
    setpoint_idx = 0;
    return;
  }
  if (c == 'A' || c == 'a') {
    cmd_prefix = 'A';
    setpoint_idx = 0;
    return;
  }

  uint8_t numerico = (c >= '0' && c <= '9');
  uint8_t enter = (c == '\n' || c == '\r');

  if (numerico && cmd_prefix) {
    if (setpoint_idx < 7)
      setpoint_buffer[setpoint_idx++] = c;
  } else if (enter && cmd_prefix && setpoint_idx > 0) {
    setpoint_buffer[setpoint_idx] = '\0';
    uint16_t val = atoi(setpoint_buffer);
    if (val <= 9999) {
      if (cmd_prefix == 'A') {
        alarme_max = val;
        conf_alarme_ticks = CONF_DISPLAY_TICKS;
        log_config('A', alarme_max);
      } else {
        setpoint_valor = val;
        conf_setpoint_ticks = CONF_DISPLAY_TICKS;
        log_config('S', setpoint_valor);
      }
    }
    cmd_prefix = 0;
    setpoint_idx = 0;
  }
}

// --- TX: eco da configuração (ex: ">A:25.00\n") ---
void log_config(char prefix, uint16_t val) {
  char p[4] = {'>', prefix, ':', '\0'}; // usando '>' para plot no teleplot.
  serial_string(p);
  serial_dec(val / 100);
  serial_string(".");
  uint8_t frac = val % 100;
  if (frac < 10)
    serial_string("0");
  serial_dec(frac);
  serial_string("\n");
}

// --- TX: envia temperatura no formato TelePlot (ex: ">T:22.50\n") ---
void log_temperatura(uint16_t t) {
  uint8_t frac = t % 100;
  serial_string(">T:");
  serial_dec(t / 100);
  serial_string(".");
  if (frac < 10)
    serial_string("0");
  serial_dec(frac);
  serial_string("\n");
}

// ==========================================================================
// execução principal
// ==========================================================================
int main(void) {
  Timer0_config();
  Timer2_config();
  GPIO_config();
  PWM_config();
  ADC_config();
  serial_init();
  sei();

  uint16_t temperatura = 0;

  while (1) {
    // --- botoes: C3 +0.50 °C, C4 -0.50 °C ---
    if (btn_c3_dis) {
      btn_c3_dis = 0;
      setpoint_valor += 50;
      if (setpoint_valor > 9999)
        setpoint_valor = 9999;
      conf_setpoint_ticks = CONF_DISPLAY_TICKS;
      log_config('S', setpoint_valor);
    }
    if (btn_c4_dis) {
      btn_c4_dis = 0;
      if (setpoint_valor >= 50)
        setpoint_valor -= 50;
      conf_setpoint_ticks = CONF_DISPLAY_TICKS;
      log_config('S', setpoint_valor);
    }

    // --- controle ON-OFF com histerese ---
    if (ADC_lido) {
      ADC_lido = 0;
      temperatura = LM35_celcius(ADC_Result);

      if (temperatura < setpoint_valor - HISTERESE) {
        PWM_set_duty(127);
      } else if (temperatura > setpoint_valor + HISTERESE) {
        PWM_set_duty(0);
      }
      // --- alarme: dispara se temp > alarme_max (histerese) ---
      if (alarme_max > 0 && temperatura > alarme_max)
        alarme_ativo = 1;
      else if (alarme_max == 0 || temperatura < alarme_max - HISTERESE)
        alarme_ativo = 0;

      log_temperatura(temperatura);
    }

    // --- display: alarme > setpoint > temperatura (3 s cada) ---
    if (conf_alarme_ticks > 0)
      atualiza_display(alarme_max);
    else if (conf_setpoint_ticks > 0)
      atualiza_display(setpoint_valor);
    else
      atualiza_display(temperatura);
  }
}
