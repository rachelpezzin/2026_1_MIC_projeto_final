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
  // DDRB: saídas — PWM e dígitos do display
  DDRB = (1 << DDB1)  // OC1A PWM lâmpada
       | (1 << DDB2)  // dígito 0 display (dezenas)
       | (1 << DDB3)  // dígito 1 display (unidades)
       | (1 << DDB4)  // dígito 2 display (décimos)
       | (1 << DDB5); // dígito 3 display (centésimos)
  // DDRD: saídas — segmentos C, D, E, F, G, DP (PD0/PD1 entradas UART)
  DDRD = (1 << DDD2)  // segmento C
       | (1 << DDD3)  // segmento D
       | (1 << DDD4)  // segmento E
       | (1 << DDD5)  // segmento F
       | (1 << DDD6)  // segmento G
       | (1 << DDD7); // segmento DP (ponto decimal)
  // DDRC: saídas — segmentos A, B e buzzer (PC0/PC3/PC4 entradas)
  DDRC |= (1 << DDC1)  // segmento A
        | (1 << DDC2)  // segmento B
        | (1 << DDC5); // buzzer
        // PC0=ADC0 (LM35)
        // PC3/PC4=botões
}

// --- PWM lâmpada no PB1 (OC1A), Timer1 16 bits, modo 14 rápido ---
void PWM_config() {
  // TCCR1A: Fast PWM modo 14 (WGM11:10=10), não-inversor (COM1A1:0=10)
  TCCR1A = (1 << WGM11)  // modo 14 (TOP=ICR1)
         | (0 << WGM10)
         | (1 << COM1A1) // não-inversor: limpa OC1A
         | (0 << COM1A0); // na comparação, seta no TOP

  // TCCR1B: modo 14 (WGM13:12=11), prescaler /64 (CS12:10=011)
  TCCR1B = (1 << WGM13)  // modo 14 (TOP=ICR1)
         | (1 << WGM12)
         | (0 << CS12)   // prescaler /64
         | (1 << CS11)   // (16 MHz / 64 = 250 kHz)
         | (1 << CS10); 
  ICR1 = 255;  // resolução 8 bits (0–255 passos)
  OCR1A = 0;   // duty cycle inicial 0%
}

void PWM_set_duty(uint8_t duty) {
  if (duty == 0) { // desliga pino
    TCCR1A &= ~((1 << COM1A1) | (1 << COM1A0)); // desconecta OC1A
    PORTB &= ~(1 << PORTB1);                    // LOW manual
  } else {
    OCR1A = duty;            // comparação 0–255 (ICR1=TOP)
    TCCR1A |= (1 << COM1A1); // reconecta OC1A não-inversor
  }
}

// ==========================================================================
// Bloco 2: Desafio extra — Amostragem por Timer/Interrupção (2 pts)
// ==========================================================================
#define ADC_AMOSTRAS 61 // 61 Hz / 61 = 1 Hz (decimação)

uint16_t ADC_Result;         // média do bloco, pronta pra main
uint8_t ADC_lido;            // flag de nova média disponível
uint8_t ADC_contador = 0;    // amostras acumuladas no bloco
uint16_t ADC_acumulador = 0; // soma das amostras do bloco

// Timer0: overflow a ~61 Hz dispara ADC (normal mode, /1024)
void Timer0_config() {
  // TCCR0A: modo normal (WGM02:0=000), WGM02 em TCCR0B
  TCCR0A = (0 << WGM01)  // modo normal: conta até
         | (0 << WGM00); // 0xFF, overflow dispara ADC

  // TCCR0B: prescaler /1024 (CS02:0=101), WGM02=0 implícito
  TCCR0B = (1 << CS02)   // prescaler /1024
         | (0 << CS01)   // (16 MHz / 1024 / 256) ≈ 61 Hz overflow
         | (1 << CS00); 
}

// ADC: trigger por Timer0 OVF, VREF=1.1V, canal ADC0
void ADC_config() {
  // ADMUX: VREF=1.1V, canal ADC0
  ADMUX = (1 << REFS1)  // VREF=1.1V
        | (1 << REFS0)
        | (0 << MUX3)   // canal ADC0
        | (0 << MUX2)
        | (0 << MUX1)
        | (0 << MUX0);

  // ADCSRA: habilita ADC, auto-trigger, ISR, prescaler /128
  ADCSRA = (1 << ADEN)   // habilita ADC
         | (1 << ADATE)  // habilita auto-trigger
         | (1 << ADIE)   // habilita interrupção no fim de conversão
         | (1 << ADPS2)  // define prescaler /128
         | (1 << ADPS1)  // (16 MHz / 128) = 125 kHz ADC clock
         | (1 << ADPS0);

  // ADCSRB: trigger = Timer0 overflow (ADTS2:0=100)
  ADCSRB = (1 << ADTS2)  // usando a ISR Timer0 overflow
         | (0 << ADTS1)
         | (0 << ADTS0);
  // DIDR0: desabilita buffer digital no pino ADC0 (reduz ruído)
  DIDR0 = (1 << ADC0D);  // desabilita entrada digital no ADC0
  ADCSRA |= (1 << ADSC); // inicia primeira conversão
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
  TIFR0 = (1 << TOV0); // limpa flag de overflow (rearama trigger)
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

// Timer2: CTC, OCR2A=TOP, ~100 Hz (10 ms) — base do debounce e ticks
void Timer2_config() {
  // TCCR2A: CTC modo 2
  TCCR2A = (1 << WGM21);  // CTC, zera ao atingir OCR2A

  // TCCR2B: prescaler /1024 (CS22:0=111), WGM22=0 implícito
  TCCR2B = (1 << CS22)   // prescaler /1024
         | (1 << CS21)   // (16 MHz / 1024 / 156) ≈ 100 Hz, 10 ms
         | (1 << CS20);

  // OCR2A: TOP do Timer2 para ~100 Hz (10 ms)
  OCR2A = 155;           // (16M / 1024 / 100Hz) - 1 = 155
  // TIMSK2: habilita interrupção ao atingir OCR2A
  TIMSK2 = (1 << OCIE2A); // habilita ISR ao atingir OCR2A
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
      buzzer_state = !buzzer_state; // alterna o beep
      if (buzzer_state)
        PORTC |= (1 << PORTC5); 
      else
        PORTC &= ~(1 << PORTC5);
    }
  } else {
    PORTC &= ~(1 << PORTC5); // alarme desativado
    buzzer_tick = 0;
    buzzer_state = 0;
  }
}

// ==========================================================================
// Bloco 5: Núcleo — Comunicação serial (UART TX + RX)
//    Arquivos: include/serial.h + src/serial.c
// ==========================================================================
uint16_t setpoint_valor = 1800; // 18.00 °C inicial

// --- TX: eco da configuração (ex: ">A:25.00\n") ---
void log_config(char prefix, uint16_t val) {
  char p[4] = {'>', prefix, ':', '\0'}; // usando '>' para plot no teleplot.
  serial_string(p);
  serial_dec(val / 100);
  serial_string(".");
  uint8_t frac = val % 100;
  if (frac < 10) serial_string("0");
  serial_dec(frac);
  serial_string("\n");
}

// --- TX: envia temperatura no formato TelePlot (ex: ">T:22.50\n") ---
void log_temperatura(uint16_t t) {
  uint8_t frac = t % 100;
  serial_string(">T:");
  serial_dec(t / 100);
  serial_string(".");
  if (frac < 10) serial_string("0");
  serial_dec(frac);
  serial_string("\n");
}

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

  // configuração setpoint e alarme através do serial
  uint8_t cmd_setpoint = (c == 'S' || c == 's'); 
  uint8_t cmd_alarme = (c == 'A' || c == 'a');
  
  if (cmd_setpoint) {
    cmd_prefix = 'S';
    setpoint_idx = 0;
    return;
  }
  if (cmd_alarme) {
    cmd_prefix = 'A';
    setpoint_idx = 0;
    return;
  }

  // após identificar o comando, identifica os valores
  uint8_t numerico = (c >= '0' && c <= '9');
  uint8_t enter = (c == '\n' || c == '\r');

  if (numerico && cmd_prefix) {
    if (setpoint_idx < 7) setpoint_buffer[setpoint_idx++] = c;
  } else if (enter && cmd_prefix && setpoint_idx > 0) {
    setpoint_buffer[setpoint_idx] = '\0'; // adiciona caractere de fim
    uint16_t val = atoi(setpoint_buffer); // converte texto para inteiro
    if (val <= 9999) {
      if (cmd_prefix == 'A') { // alarme
        alarme_max = val;
        conf_alarme_ticks = CONF_DISPLAY_TICKS;
        log_config('A', alarme_max);
      } else { // setpoint
        setpoint_valor = val;
        conf_setpoint_ticks = CONF_DISPLAY_TICKS;
        log_config('S', setpoint_valor);
      }
    }
    cmd_prefix = 0;
    setpoint_idx = 0;
  }
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
      if (setpoint_valor > 9999) setpoint_valor = 9999;
      conf_setpoint_ticks = CONF_DISPLAY_TICKS;
      log_config('S', setpoint_valor);
    }
    if (btn_c4_dis) {
      btn_c4_dis = 0;
      if (setpoint_valor >= 50) setpoint_valor -= 50;
      conf_setpoint_ticks = CONF_DISPLAY_TICKS;
      log_config('S', setpoint_valor);
    }

    if (ADC_lido) {
      ADC_lido = 0;
      temperatura = LM35_celcius(ADC_Result);
      
      // --- controle ON-OFF com histerese ---
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
