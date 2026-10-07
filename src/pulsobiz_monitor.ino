/*

============================================================================

PulsoBiz Monitor - Fase 1 (MVP)

Painel de Monitoramento em Tempo Real com Reconhecimento de Padroes

============================================================================

EMPRESA:  PulsoBiz

PRODUTO:  PulsoBiz Monitor

PROBLEMA (dor do cliente):

Donos de pequenos comercios nao tem visibilidade do que acontece no

espaco fisico da loja fora do horario em que estao presentes: nao

sabem os horarios de pico de movimento, nao percebem variacoes de

temperatura/umidade que afetam clientes e funcionarios, e nao tem

dados para tomar decisao - tudo isso sem pagar por um sistema de

CFTV/analytics corporativo caro.

SOLUCAO (Fase 1):

Sensor PIR detecta presenca/movimento no ambiente.


Sensor DHT22 le temperatura e umidade.


Um rele aciona automaticamente um atuador (ex.: ventilador) quando


a temperatura ultrapassa um limite configuravel.

Um display OLED mostra as leituras localmente, em tempo real.


O ESP32 conecta-se ao Wi-Fi, sincroniza o horario via NTP e monta,


em memoria, um HISTOGRAMA DE MOVIMENTO POR HORA DO DIA. Esse

histograma e o nucleo do "reconhecimento de padroes" do projeto:

identifica o(s) horario(s) de pico de movimento da loja.

Um mini servidor web local (dashboard) expoe as leituras atuais e


o histograma de horarios de pico, para ser visualizado em um

navegador.

ESCALABILIDADE (Fase 2 - proxima apresentacao):

A estrutura de dados (timestamp + leitura) ja criada aqui e a base

para: deteccao de anomalias (movimento fora do padrao historico),

correlacao temperatura x fluxo de clientes, contagem de pessoas via

ESP32-CAM e agregacao de multiplas unidades num Raspberry Pi central.

HARDWARE (ver diagram.json para a fiacao completa):

ESP32 DevKit V1


DHT22 (temperatura/umidade)      -> pino D4


Sensor PIR (presenca/movimento)  -> pino D27


Modulo Rele (atuador)            -> pino D26


Display OLED SSD1306 128x64 (I2C)-> pinos D21 (SDA) / D22 (SCL)


BIBLIOTECAS NECESSARIAS (ver libraries.txt):

DHT sensor library for ESPx


Adafruit SSD1306


Adafruit GFX Library


============================================================================
*/


#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------------------------------------------------------------------
// Configuracao de hardware (pinagem)
// ---------------------------------------------------------------------------
#define DHT_PIN     4
#define DHT_TYPE    DHT22
#define PIR_PIN     27
#define RELAY_PIN   26

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1

// ---------------------------------------------------------------------------
// Parametros do negocio (ajustaveis conforme o comercio-cliente)
// ---------------------------------------------------------------------------
const float TEMPERATURA_LIMITE_C = 28.0;   // acima disso, liga o atuador (rele)
const unsigned long INTERVALO_LEITURA_MS = 5000;   // intervalo de leitura do DHT22
const unsigned long INTERVALO_DISPLAY_MS = 1000;   // intervalo de atualizacao do OLED

// Wokwi-GUEST e a rede Wi-Fi aberta fornecida pelo simulador Wokwi para dar
// acesso a internet ao ESP32 simulado (necessario para sincronizar o relogio
// via NTP). Em hardware real, troque pelo SSID/senha da loja.
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// Fuso horario de Recife/PE: UTC-3, sem horario de verao.
const long GMT_OFFSET_SEC = -3 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

// ---------------------------------------------------------------------------
// Objetos globais
// ---------------------------------------------------------------------------
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);

// Ultima leitura valida dos sensores (compartilhada entre loop() e o dashboard)
float temperaturaAtual = 0;
float umidadeAtual = 0;
bool movimentoAtual = false;
bool releLigado = false;

// Histograma de movimento por hora do dia (0h-23h). Cada posicao acumula
// quantas vezes o PIR detectou uma NOVA deteccao de movimento naquela hora.
// Isso e o "reconhecimento de padrao" da Fase 1: aponta o horario de pico.
unsigned int historicoMovimentoPorHora[24] = {0};

// Controle de tempo (nao-bloqueante, sem usar delay() no loop principal)
unsigned long ultimaLeituraSensores = 0;
unsigned long ultimaAtualizacaoDisplay = 0;
bool movimentoAnterior = false;

// ---------------------------------------------------------------------------
// conectarWiFi()
// Conecta o ESP32 na rede Wi-Fi e sincroniza o relogio interno via NTP.
// O relogio sincronizado e o que permite montar o histograma por hora do dia.
// ---------------------------------------------------------------------------
void conectarWiFi() {
Serial.printf("Conectando ao Wi-Fi "%s"...\n", WIFI_SSID);
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

unsigned long inicio = millis();
while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
delay(250);
Serial.print(".");
}

if (WiFi.status() == WL_CONNECTED) {
Serial.println("\nWi-Fi conectado. IP: " + WiFi.localIP().toString());
configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
} else {
Serial.println("\nFalha ao conectar no Wi-Fi. Dashboard/horario podem nao funcionar.");
}
}

// ---------------------------------------------------------------------------
// horaAtual()
// Retorna a hora atual (0-23) segundo o relogio sincronizado por NTP.
// Se o relogio ainda nao sincronizou, retorna -1 (leitura invalida).
// ---------------------------------------------------------------------------
int horaAtual() {
struct tm timeinfo;
if (!getLocalTime(&timeinfo, 100)) {
return -1;
}
return timeinfo.tm_hour;
}

// ---------------------------------------------------------------------------
// registrarMovimento()
// Chamada sempre que o PIR detecta uma NOVA deteccao (borda de subida).
// Incrementa o histograma na hora correspondente e registra no Serial -
// essa e a fonte de dados bruta que alimenta o reconhecimento de padrao.
// ---------------------------------------------------------------------------
void registrarMovimento() {
int hora = horaAtual();
if (hora >= 0) {
historicoMovimentoPorHora[hora]++;
}
Serial.printf("[EVENTO] Movimento detectado (hora=%d) - total acumulado nesta hora: %u\n",
hora, hora >= 0 ? historicoMovimentoPorHora[hora] : 0);
}

// ---------------------------------------------------------------------------
// horarioDePico()
// Varre o histograma e retorna a hora do dia com mais deteccoes de
// movimento ate o momento. Essa funcao E o reconhecimento de padrao:
// identifica o horario de pico do comercio a partir dos dados coletados.
// ---------------------------------------------------------------------------
int horarioDePico() {
int horaPico = 0;
for (int h = 1; h < 24; h++) {
if (historicoMovimentoPorHora[h] > historicoMovimentoPorHora[horaPico]) {
horaPico = h;
}
}
return horaPico;
}

// ---------------------------------------------------------------------------
// lerSensores()
// Le o DHT22 e o PIR, atualiza as variaveis globais e aciona o rele
// automaticamente conforme o limite de temperatura configurado.
// ---------------------------------------------------------------------------
void lerSensores() {
float t = dht.readTemperature();
float h = dht.readHumidity();

if (!isnan(t) && !isnan(h)) {
temperaturaAtual = t;
umidadeAtual = h;
}

movimentoAtual = digitalRead(PIR_PIN) == HIGH;

// Detecta borda de subida (transicao sem movimento -> com movimento)
// para so contar UMA vez por evento, e nao a cada leitura do sensor.
if (movimentoAtual && !movimentoAnterior) {
registrarMovimento();
}
movimentoAnterior = movimentoAtual;

// Automacao simples: liga o atuador (ex.: ventilador) se estiver quente.
releLigado = temperaturaAtual > TEMPERATURA_LIMITE_C;
digitalWrite(RELAY_PIN, releLigado ? HIGH : LOW);
}

// ---------------------------------------------------------------------------
// atualizarDisplay()
// Mostra as leituras atuais e a hora de pico detectada ate agora no OLED.
// ---------------------------------------------------------------------------
void atualizarDisplay() {
display.clearDisplay();
display.setTextSize(1);
display.setTextColor(SSD1306_WHITE);

display.setCursor(0, 0);
display.println("PulsoBiz Monitor");
display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

display.setCursor(0, 16);
display.printf("Temp:  %.1f C\n", temperaturaAtual);
display.setCursor(0, 26);
display.printf("Umid:  %.1f %%\n", umidadeAtual);
display.setCursor(0, 36);
display.printf("Mov:   %s\n", movimentoAtual ? "SIM" : "nao");
display.setCursor(0, 46);
display.printf("Rele:  %s\n", releLigado ? "LIGADO" : "desligado");

display.setCursor(0, 56);
display.printf("Pico ate agora: %02dh", horarioDePico());

display.display();
}

// ---------------------------------------------------------------------------
// handleRaiz() / handleDados()
// Rotas do dashboard web hospedado no proprio ESP32. handleRaiz() serve uma
// pagina HTML simples com as leituras e o histograma; handleDados() expoe
// os mesmos dados em JSON, para futura integracao (ex.: Fase 2, Raspberry Pi).
// ---------------------------------------------------------------------------
void handleRaiz() {
String html = "<html><head><meta charset='utf-8'>";
html += "<meta http-equiv='refresh' content='5'>";
html += "<title>PulsoBiz Monitor</title>";
html += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:20px}";
html += "h1{color:#4caf50} .barra{background:#4caf50;height:14px;margin:2px 0}</style>";
html += "</head><body>";
html += "<h1>PulsoBiz Monitor - Painel em Tempo Real</h1>";
html += "<p>Temperatura: " + String(temperaturaAtual, 1) + " °C</p>";
html += "<p>Umidade: " + String(umidadeAtual, 1) + " %</p>";
html += "<p>Movimento agora: " + String(movimentoAtual ? "SIM" : "nao") + "</p>";
html += "<p>Atuador (rele): " + String(releLigado ? "LIGADO" : "desligado") + "</p>";
html += "<p><b>Horario de pico detectado: " + String(horarioDePico()) + "h</b></p>";
html += "<h3>Historico de movimento por hora</h3>";
for (int h = 0; h < 24; h++) {
int largura = historicoMovimentoPorHora[h] * 20;
html += "<div>" + String(h) + "h <div class='barra' style='width:" + String(largura) + "px'></div></div>";
}
html += "</body></html>";
server.send(200, "text/html", html);
}

void handleDados() {
String json = "{";
json += ""temperatura":" + String(temperaturaAtual, 1) + ",";
json += ""umidade":" + String(umidadeAtual, 1) + ",";
json += ""movimento":" + String(movimentoAtual ? "true" : "false") + ",";
json += ""rele":" + String(releLigado ? "true" : "false") + ",";
json += ""horarioPico":" + String(horarioDePico()) + ",";
json += ""historico":[";
for (int h = 0; h < 24; h++) {
json += String(historicoMovimentoPorHora[h]);
if (h < 23) json += ",";
}
json += "]}";
server.send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
Serial.begin(115200);

pinMode(PIR_PIN, INPUT);
pinMode(RELAY_PIN, OUTPUT);
digitalWrite(RELAY_PIN, LOW);

dht.begin();

if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
Serial.println("Falha ao iniciar o display OLED.");
}
display.clearDisplay();
display.setTextColor(SSD1306_WHITE);
display.setCursor(0, 0);
display.println("PulsoBiz Monitor");
display.println("Iniciando...");
display.display();

conectarWiFi();

server.on("/", handleRaiz);
server.on("/dados", handleDados);
server.begin();
Serial.println("Dashboard web iniciado.");
}

// ---------------------------------------------------------------------------
// loop()
// Loop principal, nao-bloqueante: le sensores e atualiza o display em
// intervalos proprios, e atende as requisicoes do dashboard web a cada
// iteracao.
// ---------------------------------------------------------------------------
void loop() {
unsigned long agora = millis();

if (agora - ultimaLeituraSensores >= INTERVALO_LEITURA_MS) {
ultimaLeituraSensores = agora;
lerSensores();
}

if (agora - ultimaAtualizacaoDisplay >= INTERVALO_DISPLAY_MS) {
ultimaAtualizacaoDisplay = agora;
atualizarDisplay();
}

server.handleClient();
}
