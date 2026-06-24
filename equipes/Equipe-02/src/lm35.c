// ==========================================================================
// Núcleo obrigatório — Leitura do sensor LM35 via ADC
//   10mV/°C, VREF=1.1V, ADC 10 bits (1024 passos).
//   centésimos = raw * (1100mV / 1024) * (100 centésimos / 10mV)
//              = raw * 11000 / 1024
// ==========================================================================
#include "lm35.h"

uint16_t LM35_celcius(uint16_t adc_raw) {
    uint32_t produto = adc_raw * 11000UL;
    return produto / 1024UL;
}
