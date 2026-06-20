/*
 * main.c
 *
 * Created: 6/12/2026 4:23:51 PM
 * Author: Bruno, Ruan, Roberto
 * Adaptação: Inclusão do LCD 16x2 em modo 4-bits via PORTD
 */ 

#define F_CPU 16000000UL 
#include <xc.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define BAUD_PRESCALE 103
#define LCD_RS     PD2
#define LCD_ENABLE PD3

volatile float temperatura = 0.0;
volatile uint8_t nova_leitura = 0;

/* Inicializa a periferia UART0 para transmissão serial a 9600 baud, 8N1 */
void USART_Init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0B = (1 << TXEN0); 
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); 
}

/* Transmite um único caractere via hardware pela serial */
void USART_TxChar(char caractere) {
    while (!(UCSR0A & (1 << UDRE0))); 
    UDR0 = caractere;                 
}

/* Transmite uma string completa caractere por caractere via serial */
void USART_TxString(const char* str) {
    while (*str) {
        USART_TxChar(*str++);
    }
}

/* Processa matematicamente o float e transmite a temperatura via serial */
void Enviar_Dados_Serial(float temp) {
    if (temp < 0) temp = 0; 
    
    int parte_inteira = (int)temp;
    int parte_decimal = (int)((temp - parte_inteira) * 10); 
    if (parte_decimal < 0) parte_decimal = -parte_decimal;

    char centena = (parte_inteira / 100) % 10;
    char dezena  = (parte_inteira / 10) % 10;
    char unidade = parte_inteira % 10;

    if (parte_inteira >= 100) {
        USART_TxChar(centena + '0');
    }
    if (parte_inteira >= 10) {
        USART_TxChar(dezena + '0');
    }
    
    USART_TxChar(unidade + '0');
    USART_TxChar('.');
    USART_TxChar((parte_decimal % 10) + '0');
    USART_TxString(" C\r\n");
}

/* Envia um nibble de 4 bits para os pinos altos do PORTD (PD4-PD7) acionando o pino Enable */
void LCD_Write4Bits(uint8_t data) {
    PORTD = (PORTD & 0x0F) | (data & 0xF0);
    PORTD |= (1 << LCD_ENABLE);
    _delay_us(1);
    PORTD &= ~(1 << LCD_ENABLE);
    _delay_us(100);
}

/* Envia um comando de configuração (RS em nível baixo) dividindo o byte em dois blocos de 4 bits */
void LCD_Cmd(uint8_t cmd) {
    PORTD &= ~(1 << LCD_RS); 
    LCD_Write4Bits(cmd & 0xF0); 
    LCD_Write4Bits((cmd << 4) & 0xF0); 
    _delay_ms(2); 
}

/* Envia um caractere de dado visível para a memória do LCD (RS em nível alto) */
void LCD_TxChar(char data) {
    PORTD |= (1 << LCD_RS); 
    LCD_Write4Bits(data & 0xF0); 
    LCD_Write4Bits((data << 4) & 0xF0); 
    _delay_us(50);
}

/* Envia uma string completa de texto para ser exibida no display LCD */
void LCD_TxString(const char* str) {
    while (*str) {
        LCD_TxChar(*str++);
    }
}

/* Executa a inicialização física obrigatória do controlador HD44780 em modo de 4 bits */
void LCD_Init(void) {
    DDRD |= 0xFC; 
    _delay_ms(20); 
    
    LCD_Write4Bits(0x30);
    _delay_ms(5);
    LCD_Write4Bits(0x30);
    _delay_us(150);
    LCD_Write4Bits(0x30);
    _delay_us(150);
    
    LCD_Write4Bits(0x20); 
    _delay_ms(1);
    
    LCD_Cmd(0x28); 
    LCD_Cmd(0x0C); 
    LCD_Cmd(0x06); 
    LCD_Cmd(0x01); 
    _delay_ms(2);
}

/* Formata o valor de temperatura e renderiza de forma estática na primeira linha do LCD */
void Exibir_Dados_LCD(float temp) {
    if (temp < 0) temp = 0;
    
    int parte_inteira = (int)temp;
    int parte_decimal = (int)((temp - parte_inteira) * 10);
    if (parte_decimal < 0) parte_decimal = -parte_decimal;

    char centena = (parte_inteira / 100) % 10;
    char dezena  = (parte_inteira / 10) % 10;
    char unidade = parte_inteira % 10;

    LCD_Cmd(0x80); 
    LCD_TxString("TEMP: ");

    if (parte_inteira >= 100) LCD_TxChar(centena + '0'); else LCD_TxChar(' ');
    if (parte_inteira >= 10)  LCD_TxChar(dezena + '0');  else LCD_TxChar(' ');
    
    LCD_TxChar(unidade + '0');
    LCD_TxChar('.');
    LCD_TxChar((parte_decimal % 10) + '0');
    LCD_TxChar(0xDF); 
    LCD_TxString("C ");
}

/* Vetor de Interrupção do ADC: Disparado automaticamente pelo Timer 1 a 4Hz */
ISR(ADC_vect) {
    uint16_t adc_raw = ADC;
    temperatura = adc_raw * 0.48828125;
    nova_leitura = 1; 
    TIFR1 = (1 << OCF1B); 
}

int main(void)
{
    USART_Init(BAUD_PRESCALE);
    LCD_Init();
    
    USART_TxString("--- Monitor de Temperatura (4Hz) ---\r\n");
    LCD_Cmd(0xC0); 
    LCD_TxString("SISTEMA LIGADO  ");

    // Configuração do ADC com Trigger pelo Timer 1
    ADMUX = (1 << REFS0); 
    ADCSRB = (1 << ADTS2) | (0 << ADTS1) | (1 << ADTS0); 
    ADCSRA = (1 << ADEN)  |
             (1 << ADATE) |
             (1 << ADIE)  |
             (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); 

    // Configuração do Timer 1 em modo CTC para estipular a frequência de 4Hz
    TCCR1A = 0; 
    TCCR1B = (1 << WGM12) | (1 << CS12); 
    OCR1A = 15624; 
    OCR1B = 15624; 
    TIFR1 = (1 << OCF1B); 

    sei(); 

    float temp_local = 0.0;

    while(1)
    {
        if (nova_leitura) {
            cli(); 
            temp_local = temperatura;
            nova_leitura = 0; 
            sei(); 
            
            Enviar_Dados_Serial(temp_local); 
            Exibir_Dados_LCD(temp_local);
        }
    }
}