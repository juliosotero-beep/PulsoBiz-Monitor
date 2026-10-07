# PulsoBiz Monitor

## Painel de Monitoramento em Tempo Real com Reconhecimento de Padrões

Projeto de Sistemas Embarcados — Fase 1 (MVP).

## Empresa

**PulsoBiz**

## Produto

**PulsoBiz Monitor**

## Problema

Donos de pequenos comércios não possuem visibilidade do que acontece no espaço físico da loja fora do horário em que estão presentes.

O sistema busca fornecer informações sobre:

- Movimento no ambiente;
- Temperatura;
- Umidade;
- Horários de maior movimento;
- Acionamento automático de um atuador.

## Solução

O PulsoBiz Monitor utiliza um ESP32 conectado a sensores para coletar informações do ambiente.

O sistema:

- Detecta movimento através de um sensor PIR;
- Mede temperatura e umidade através do DHT22;
- Aciona um relé quando a temperatura ultrapassa o limite configurado;
- Exibe informações em um display OLED;
- Sincroniza o horário através de NTP;
- Registra movimentos por hora;
- Identifica o horário de pico de movimento;
- Disponibiliza um dashboard web no próprio ESP32.

## Tecnologias

- ESP32 DevKit V1
- DHT22
- Sensor PIR
- Módulo Relé
- Display OLED SSD1306
- Arduino
- C++
- Wi-Fi
- NTP
- Wokwi

## Pinagem

| Componente | ESP32 |
|---|---|
| DHT22 | GPIO 4 |
| PIR | GPIO 27 |
| Relé | GPIO 26 |
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |

## Reconhecimento de Padrões

O sistema mantém um histograma de movimento por hora do dia.

Cada nova detecção realizada pelo sensor PIR é registrada na hora correspondente.

Com o acúmulo dos dados, o sistema identifica automaticamente o horário que apresenta maior quantidade de detecções.

## Dashboard

O ESP32 hospeda um dashboard web que apresenta:

- Temperatura;
- Umidade;
- Movimento;
- Estado do relé;
- Horário de pico;
- Histórico de movimento por hora.

Também existe uma rota JSON:

```text
/dados
