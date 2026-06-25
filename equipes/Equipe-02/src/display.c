// ==========================================================================
// Desafio extra — Display local 7 segmentos (2 pts)
//   4 dígitos multiplexados (PB2–PB5), segmentos em PD2–PD7 + PC1–PC2.
//   Mostra centésimos de °C com 2 casas decimais (ex: 22.50).
// ==========================================================================
#include "display.h"
#include "util/delay.h"
#include <xc.h>

const uint8_t SEGMENTO[10] = {
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111  // 9
};

void aciona_segmentos(uint8_t valor_tabela) {
  uint8_t segs = ~valor_tabela;                              // inverte: catodo comum
  PORTD = (PORTD & ((1 << PORTD0) | (1 << PORTD1)))         // preserva PD0(RX), PD1(TX)
        | (segs & ~((1 << PORTD0) | (1 << PORTD1)));        // segmentos C–DP em PD2–PD7

  if ((segs & (1 << 0)))
    PORTC |= (1 << PORTC1);  // segmento A aceso
  else
    PORTC &= ~(1 << PORTC1); // segmento A apagado

  if ((segs & (1 << 1)))
    PORTC |= (1 << PORTC2);  // segmento B aceso
  else
    PORTC &= ~(1 << PORTC2); // segmento B apagado
}

void atualiza_display(uint16_t centesimos) {
  uint8_t d[4];
  d[0] = (centesimos / 1000) % 10; // dezenas
  d[1] = (centesimos / 100) % 10;  // unidades
  d[2] = (centesimos / 10) % 10;   // décimos
  d[3] = centesimos % 10;          // centésimos

  PORTB |= (1 << PORTB2) | (1 << PORTB3) | (1 << PORTB4) | (1 << PORTB5);
  // desliga todos os dígitos (catodo comum: HIGH = desligado)

  // dígito 0 (dezenas)
  aciona_segmentos(SEGMENTO[d[0]]);
  PORTB &= ~(1 << PORTB2); // liga dígito 0 (catodo comum)
  _delay_ms(5);
  PORTB |= (1 << PORTB2);  // desliga dígito 0

  // dígito 1 (unidades, com ponto decimal)
  aciona_segmentos(SEGMENTO[d[1]] | (1 << 7));
  PORTB &= ~(1 << PORTB3); // liga dígito 1
  _delay_ms(5);
  PORTB |= (1 << PORTB3);  // desliga dígito 1

  // dígito 2 (décimos)
  aciona_segmentos(SEGMENTO[d[2]]);
  PORTB &= ~(1 << PORTB4); // liga dígito 2
  _delay_ms(5);
  PORTB |= (1 << PORTB4);  // desliga dígito 2

  // dígito 3 (centésimos)
  aciona_segmentos(SEGMENTO[d[3]]);
  PORTB &= ~(1 << PORTB5); // liga dígito 3
  _delay_ms(5);
  PORTB |= (1 << PORTB5);  // desliga dígito 3
}
