// ==========================================================================
// Núcleo obrigatório — Comunicação serial UART (9600 bps 8N1)
//   TX: buffer circular de 16 bytes, ISR via UDRE
//   RX: habilitado (RXCIE0), buffer tratado em main.c (ISR USART_RX)
// ==========================================================================
#include "serial.h"
#include "avr/interrupt.h"
#include "avr/io.h"
#include <xc.h>
#define F_CPU 16000000UL

#define BAUD      9600
#define UBRR_VAL  ((F_CPU / (16UL * BAUD)) - 1)

#define BUF_SIZE  16
#define BUF_MASK  (BUF_SIZE - 1)

static volatile uint8_t buf[BUF_SIZE];
static volatile uint8_t head = 0;    // índice de escrita
static volatile uint8_t tail = 0;    // índice de leitura

// --- ISR TX: dispara quando UDR0 está pronto ---
ISR(USART_UDRE_vect) {
    UDR0 = buf[tail];                           // escreve próximo byte no TX
    tail = (tail + 1) & BUF_MASK;
    if (head == tail) UCSR0B &= ~(1 << UDRIE0);  // desliga interrupção (buffer vazio)
}

// --- serial_init: 9600 8N1, TX + RX + interrupção ---
void serial_init(void) {
    // UBRR0: define baudrate 9600 bps (16M / 16 / 9600 - 1)
    UBRR0  = UBRR_VAL;       // 103
    // UCSR0A: velocidade normal (U2X0=0), sem multi-processador (MPCM0=0)
    UCSR0A = (0 << U2X0);    // sem dobro de velocidade
    // UCSR0B: TX enable, RX enable, RX complete interrupção enable
    UCSR0B = (1 << TXEN0)    // habilita transmissor
           | (1 << RXEN0)    // habilita receptor
           | (1 << RXCIE0);  // habilita interrupção ao receber byte
    // UCSR0C: 8 bits, 1 stop, sem paridade, assíncrono
    UCSR0C = (1 << UCSZ01)   // 8-bit (UCSZ0[2:0]=011)
           | (1 << UCSZ00);
    // USBS0=0 (1 stop), UPM0[1:0]=00 (sem paridade),
    // UMSEL0[1:0]=00 (assíncrono), UCSZ02=0 — omitidos (reset default)
}

// --- buffer_put (interna): insere 1 byte no buffer circular ---
static void buffer_put(uint8_t data) {
    uint8_t next = (head + 1) & BUF_MASK;
    while (next == tail);             // espera se cheio
    buf[head] = data;

    cli();
    uint8_t estava_vazio = (head == tail);
    head = next;
    if (estava_vazio) UCSR0B |= (1 << UDRIE0);  // liga interrupção de data register empty
    sei();
}

// --- serial_string: envia string terminada em \0 ---
void serial_string(const char* str) {
    while (*str) buffer_put(*str++);
}

// --- serial_dec: envia uint16_t como dígitos decimais ---
void serial_dec(uint16_t num) {
    char digits[6];                   // 65535 = 5 dígitos
    uint8_t i = 0;

    if (num == 0) {
        buffer_put('0');
        return;
    }

    while (num > 0) {
        digits[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i > 0) buffer_put(digits[--i]);
}
