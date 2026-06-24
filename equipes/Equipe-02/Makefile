# ==============================================================================
# Makefile - Equipe 02 - Projeto Final MIC
# ==============================================================================
# Uso:
#   make          - Compila o projeto (default)
#   make clean    - Remove arquivos de build
#   make upload   - Compila e grava no microcontrolador
#   make help     - Mostra esta ajuda
# ==============================================================================

# Configurações do Compilador AVR
MCU       = atmega328p
F_CPU     = 16000000UL
COMPILER  = avr-gcc
OBJCOPY   = avr-objcopy
SIZE      = avr-size

# Diretórios de include (xc.h + headers AVR do XC8)
XC8_AVR   = /opt/microchip/xc8/v3.10/avr
INCLUDES  = -Iinclude
INCLUDES += -I$(XC8_AVR)/lib/gcc/avr/5.4.0/include
INCLUDES += -I$(XC8_AVR)/avr/include

# Configurações do AVRDUDE (upload)
AVRDUDE     = avrdude
UPLOAD_MCU  = atmega328p
UPLOAD_PROG = arduino
UPLOAD_PORT = $(if $(PORT),$(PORT),/dev/ttyACM0)
UPLOAD_BAUD = $(if $(BAUD),$(BAUD),115200)

# Arquivos do projeto
SOURCES = src/main.c src/serial.c src/display.c src/lm35.c
TARGET  = main
OUTDIR  = build

# Flags de compilação
CFLAGS  = -mmcu=$(MCU)
CFLAGS += -DF_CPU=$(F_CPU)
CFLAGS += -Os
CFLAGS += -Wall -Wextra
CFLAGS += $(INCLUDES)

# ==============================================================================
# ALVOS
# ==============================================================================

.DEFAULT_GOAL := all

# Compilar o projeto
all: $(OUTDIR) $(OUTDIR)/$(TARGET).hex
	@echo ""
	@echo "==> Build concluído: $(OUTDIR)/$(TARGET).hex"
	@echo ""

$(OUTDIR):
	@mkdir -p $(OUTDIR)

$(OUTDIR)/$(TARGET).elf: $(SOURCES)
	@echo "==> Compilando: $(SOURCES)"
	$(COMPILER) $(CFLAGS) -o $@ $(SOURCES)
	@echo "  Tamanho:"
	@$(SIZE) $@

$(OUTDIR)/$(TARGET).hex: $(OUTDIR)/$(TARGET).elf
	@echo "  Gerando HEX: $@"
	$(OBJCOPY) -O ihex $< $@

# Compilar e gravar no microcontrolador
upload: all
	@echo "==> Gravando em $(UPLOAD_PORT)"
	$(AVRDUDE) -v -p $(UPLOAD_MCU) -c $(UPLOAD_PROG) \
		-P $(UPLOAD_PORT) -b $(UPLOAD_BAUD) \
		-D -U flash:w:$(OUTDIR)/$(TARGET).hex:i
	@echo ""

# Limpar arquivos de build
clean:
	@echo "==> Limpando build"
	@rm -rf $(OUTDIR)
	@echo ""

# Ajuda
help:
	@echo "================================================================"
	@echo "  Makefile - Equipe 02 - Projeto Final MIC"
	@echo "================================================================"
	@echo ""
	@echo "Comandos:"
	@echo "  make              - Compila o projeto"
	@echo "  make clean        - Remove arquivos de build"
	@echo "  make upload       - Compila e grava no microcontrolador"
	@echo "  make help         - Mostra esta ajuda"
	@echo ""
	@echo "Upload (variáveis configuráveis):"
	@echo "  PORT=<porta>      (padrão: $(UPLOAD_PORT))"
	@echo "  BAUD=<baud>       (padrão: $(UPLOAD_BAUD))"
	@echo ""
	@echo "Exemplos:"
	@echo "  make upload PORT=/dev/ttyUSB0"
	@echo "  make upload PORT=/dev/ttyUSB0 BAUD=57600"
	@echo ""

.PHONY: all upload clean help
