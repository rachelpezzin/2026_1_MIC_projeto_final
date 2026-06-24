// ==========================================================================
// Núcleo obrigatório — Comunicação serial UART (TX + RX)
//   9600 bps 8N1, buffer circular de TX, ISR RX para setpoint
// ==========================================================================
#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdint.h>

void serial_init(void);
void serial_string(const char* str);
void serial_dec(uint16_t num);

#endif
