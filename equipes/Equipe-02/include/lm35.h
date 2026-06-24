// ==========================================================================
// Núcleo obrigatório — Leitura do sensor LM35 via ADC
//   Conversão raw ADC → centésimos de °C (VREF=1.1V, 10mV/°C)
// ==========================================================================
#ifndef LM35_H_
#define LM35_H_

#include <stdint.h>

uint16_t LM35_celcius(uint16_t adc_raw);

#endif
