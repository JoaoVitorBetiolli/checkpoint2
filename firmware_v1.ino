#include <WiFi.h>
#include <Update.h>
#include <HTTPClient.h>

#define LED_BLUE  25
#define LED_GREEN 26
#define LED_RED   27

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* VERSION_ATUAL = "1.0";
const char* URL_VERSION_JSON = "https://raw.githubusercontent.com/JoaoVitorBetiolli/checkpoint2/main/version.json";

const int NUM_LEITURAS = 5;
const unsigned long INTERVALO_LEITURA = 2000;
const unsigned long INTERVALO_SESSAO  = 48000;

float leituras[NUM_LEITURAS];
int indiceLeitura = 0;

unsigned long tempoInicioSessao = 0;
unsigned long tempoUltimaLeitura = 0;
bool sessaoEmAndamento = false;

int contadorSessoes = 0;
bool wifiConectado = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);

  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 1.0");
  Serial.println("========================================");

  conectarWiFi();

  iniciarNovaSessao();
}

void loop() {
  unsigned long agora = millis();

  if (sessaoEmAndamento) {
    if (indiceLeitura < NUM_LEITURAS && (agora - tempoUltimaLeitura >= INTERVALO_LEITURA || indiceLeitura == 0)) {
      realizarLeitura();
      tempoUltimaLeitura = agora;
    }

    if (indiceLeitura >= NUM_LEITURAS) {
      finalizarSessao();
    }
  }

  if (!sessaoEmAndamento && (agora - tempoInicioSessao >= INTERVALO_SESSAO)) {
    iniciarNovaSessao();
  }
}

void conectarWiFi() {
  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConectado = true;
    Serial.println();
    Serial.println("Wi-Fi conectado com sucesso!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    wifiConectado = false;
    Serial.println();
    Serial.println("ERRO: nao foi possivel conectar ao Wi-Fi.");
  }
}

void iniciarNovaSessao() {
  tempoInicioSessao = millis();
  tempoUltimaLeitura = tempoInicioSessao - INTERVALO_LEITURA;
  indiceLeitura = 0;
  sessaoEmAndamento = true;
}

void realizarLeitura() {
  float valor = random(100, 201) / 10.0;
  leituras[indiceLeitura] = valor;

  Serial.print("Leitura ");
  Serial.print(indiceLeitura + 1);
  Serial.print(": ");
  Serial.print(valor, 1);
  Serial.println(" cm");

  indiceLeitura++;
}

void finalizarSessao() {
  float soma = 0;
  for (int i = 0; i < NUM_LEITURAS; i++) {
    soma += leituras[i];
  }
  float media = soma / NUM_LEITURAS;

  Serial.print("Media da sessao: ");
  Serial.print(media, 1);
  Serial.println(" cm");

  unsigned long restante = (INTERVALO_SESSAO - (millis() - tempoInicioSessao)) / 1000;
  Serial.print("Proxima sessao em ");
  Serial.print(restante);
  Serial.println(" segundos.");
  Serial.println("----------------------------------------");

  sessaoEmAndamento = false;
  contadorSessoes++;

  if (contadorSessoes >= 3) {
    verificarAtualizacao();
  }
}

void verificarAtualizacao() {
  if (!wifiConectado) {
    Serial.println("AVISO: sem Wi-Fi, nao e possivel verificar atualizacao.");
    return;
  }

  Serial.println("Verificando atualizacao disponivel...");

  HTTPClient http;
  http.begin(URL_VERSION_JSON);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("Manifesto recebido:");
    Serial.println(payload);

    int idx = payload.indexOf("\"version\"");
    if (idx == -1) {
      Serial.println("ERRO: manifesto invalido (campo version nao encontrado).");
      http.end();
      return;
    }

    int aspasInicio = payload.indexOf('"', idx + 10);
    int aspasFim = payload.indexOf('"', aspasInicio + 1);
    String versaoDisponivel = payload.substring(aspasInicio + 1, aspasFim);

    Serial.print("Versao instalada: ");
    Serial.println(VERSION_ATUAL);
    Serial.print("Versao disponivel: ");
    Serial.println(versaoDisponivel);

    if (versaoDisponivel != VERSION_ATUAL) {
      Serial.println(">> Nova versao disponivel! Iniciando atualizacao...");

      int urlIdx = payload.indexOf("\"url\"");
      int urlAspasInicio = payload.indexOf('"', urlIdx + 6);
      int urlAspasFim = payload.indexOf('"', urlAspasInicio + 1);
      String urlFirmware = payload.substring(urlAspasInicio + 1, urlAspasFim);

      http.end();
      baixarEAtualizar(urlFirmware);
      return;
    } else {
      Serial.println(">> Versao instalada ja e a mais recente.");
    }

  } else {
    Serial.print("ERRO: nao foi possivel acessar o manifesto. Codigo HTTP: ");
    Serial.println(httpCode);
  }

  http.end();
}

void baixarEAtualizar(String urlFirmware) {
  Serial.print("Baixando firmware de: ");
  Serial.println(urlFirmware);

  HTTPClient http;
  http.begin(urlFirmware);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.print("ERRO: nao foi possivel baixar o firmware. Codigo HTTP: ");
    Serial.println(httpCode);
    http.end();
    return;
  }

  int tamanho = http.getSize();
  if (tamanho <= 0) {
    Serial.println("ERRO: tamanho do arquivo de firmware invalido.");
    http.end();
    return;
  }

  Serial.print("Tamanho do firmware: ");
  Serial.print(tamanho);
  Serial.println(" bytes");

  Serial.print("Espaco livre para atualizacao: ");
  Serial.println(ESP.getFreeSketchSpace());

  if (!Update.begin(tamanho)) {
    Serial.print("ERRO ao iniciar atualizacao: ");
    Serial.println(Update.errorString());
    http.end();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t escrito = Update.writeStream(*stream);

  if (escrito != tamanho) {
    Serial.println("ERRO: falha ao gravar o firmware completo.");
    http.end();
    return;
  }

  if (!Update.end()) {
    Serial.print("ERRO no processo de atualizacao: ");
    Serial.println(Update.getError());
    http.end();
    return;
  }

  if (!Update.isFinished()) {
    Serial.println("ERRO: atualizacao nao foi finalizada corretamente.");
    http.end();
    return;
  }

  Serial.println("Atualizacao concluida com sucesso! Reiniciando...");
  http.end();
  delay(1000);
  ESP.restart();
}