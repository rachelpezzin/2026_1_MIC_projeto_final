#ifndef SERIAL_LOGGER_H_
#define SERIAL_LOGGER_H_

#include <stdint.h>

// ??? API do Serial Logger ???????????????????????????????????????
// Logger TX n�o-bloqueante com buffer circular e interrup��o UDRE.
// Exemplo:
//   log_init();
//   log_string("adc: ");
//   log_dec(adc_val);
//   log_string("\r\n");

void    log_init(void);               // configura USART 9600 8N1, prepara buffer
void    log_string(const char* str);   // envia string (terminada em \0)
void    log_byte(uint8_t byte);        // envia 1 byte cru
void    log_dec(uint16_t num);         // envia n�mero decimal (0�65535)
void    log_hex(uint8_t byte);         // envia 2 d�gitos hex (00�FF)
uint8_t log_is_busy(void);             // 1 = ainda tem dados no buffer
void    log_tx_release(void);          // aguarda TX, limpa flag e desativa TXEN0 (libera PD1)
void    log_tx_claim(void);            // reativa TXEN0 (PD1 volta a ser TXD)

#endif /* SERIAL_LOGGER_H_ */
