# Equipe 02 - Rachel e Gabriel

## Desafios e pontuação

A nota do projeto é composta por um **núcleo obrigatório** (todas as equipes) mais
**desafios extras**, escolhidos pela equipe para completar pelo menos **10 pontos**.

A lista de desafios extras soma mais de 10 pontos de propósito: se a equipe não
conseguir concluir todos, pode deixar algum de fora e ainda atingir a pontuação máxima.

## Núcleo obrigatório (4 pontos)

Todas as equipes devem entregar, no mínimo, um sistema funcionando com:

| Requisito | Status |
|---|---|
| Leitura do sensor LM35 via ADC e conversão para °C | OK |
| Saída PWM controlando a potência da lâmpada | OK |
| Comunicação serial (UART): envia temperatura periodicamente | OK |
| Comunicação serial (UART): recebe comandos para alterar o setpoint | OK |
| Malha de controle (ON-OFF / histerese) | OK |

## Desafios extras (escolher para completar 10 pontos)

| Desafio | Pontos | Descrição | Status |
|---|---|---|---|
| Controle PID completo | 4 | Implementar PID (ou PI) na malha de temperatura, com Kp, Ki e Kd ajustáveis via porta serial. Cuidado com anti-windup e saturação do PWM. | PENDENTE |
| Caracterização do sistema (resposta ao degrau) | 3 | Aplicar um degrau na saída PWM e registrar a curva de temperatura ao longo do tempo, permitindo estimar ganho estático e constante de tempo do sistema. | PENDENTE |
| Filtro digital no sinal do LM35 | 2 | Implementar um filtro digital (média móvel, exponencial/IIR, etc.) para reduzir ruído na leitura de temperatura, com justificativa do projeto do filtro. | PENDENTE |
| Amostragem por timer/interrupção | 2 | Substituir o uso de `_delay_ms()` no laço de controle por uma base de tempo gerada por timer/interrupção, garantindo período de amostragem constante. | OK |
| Display local (LCD ou 7 segmentos) | 2 | Mostrar temperatura atual e setpoint em um display do kit, sem depender do terminal serial. | OK |
| Log de dados em EEPROM | 2 | Armazenar um histórico de temperaturas na EEPROM interna, recuperável após reset/desligamento. | PENDENTE |
| Sistema de alarme configurável | 2 | Sinalizar (LED/buzzer) quando a temperatura saia de uma faixa configurável via serial. | PENDENTE |
| Setpoint via botão físico | 1 | Permitir ajustar o setpoint usando botões do kit, com debounce implementado via interrupção. | OK |

**Total disponível nos extras: 18 pontos** (4 + 3 + 2 + 2 + 2 + 2 + 2 + 1). Combinado
com o núcleo (4 pontos), a equipe tem margem para escolher os desafios mais adequados
ao seu nível e ainda alcançar 10 pontos.

## Observações

- Equipes sem experiência em Controle podem montar os 10 pontos sem o PID
  (ex.: núcleo + caracterização do sistema + filtro digital + display +
  amostragem por timer = 4 + 3 + 2 + 2 + 2 = 13).
- O PID vale mais pontos justamente por ser mais avançado, mas não é obrigatório.
- Cada desafio extra deve ser identificável no código (comentários e/ou README da
  equipe indicando onde está implementado), pois será conferido na apresentação.
