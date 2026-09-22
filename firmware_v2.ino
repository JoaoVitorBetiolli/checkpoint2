#include <WiFi.h>
#include <Update.h>
#include <HTTPClient.h>

#define LED_BLUE  25
#define LED_GREEN 26
#define LED_RED   27

const char* VERSION_ATUAL = "2.0";

const int NUM_LEITURAS = 5;
const unsigned long INTERVALO_LEITURA = 2000;
const unsigned long INTERVALO_SESSAO  = 48000;

float leituras[NUM_LEITURAS];
float leiturasOrdenadas[NUM_LEITURAS];
int indiceLeitura = 0;

unsigned long tempoInicioSessao = 0;
unsigned long tempoUltimaLeitura = 0;
bool sessaoEmAndamento = false;

// Estado do sistema (histerese): true = ALERTA, false = NORMAL
bool estadoAlerta = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  // Firmware 2.0 nao usa o LED azul; comeca no estado NORMAL (verde)
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);

  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 2.0");
  Serial.println("========================================");

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

void ordenar(float origem[], float destino[], int tamanho) {
  for (int i = 0; i < tamanho; i++) {
    destino[i] = origem[i];
  }
  // Ordenacao simples (bubble sort) - suficiente para 5 elementos
  for (int i = 0; i < tamanho - 1; i++) {
    for (int j = 0; j < tamanho - 1 - i; j++) {
      if (destino[j] > destino[j + 1]) {
        float temp = destino[j];
        destino[j] = destino[j + 1];
        destino[j + 1] = temp;
      }
    }
  }
}

void finalizarSessao() {
  float soma = 0;
  for (int i = 0; i < NUM_LEITURAS; i++) {
    soma += leituras[i];
  }
  float media = soma / NUM_LEITURAS;

  ordenar(leituras, leiturasOrdenadas, NUM_LEITURAS);
  float mediana = leiturasOrdenadas[2]; // terceiro elemento (indice 2) do vetor ordenado

  Serial.print("Ordem original: ");
  for (int i = 0; i < NUM_LEITURAS; i++) {
    Serial.print(leituras[i], 1);
    if (i < NUM_LEITURAS - 1) Serial.print(", ");
  }
  Serial.println();

  Serial.print("Ordem crescente: ");
  for (int i = 0; i < NUM_LEITURAS; i++) {
    Serial.print(leiturasOrdenadas[i], 1);
    if (i < NUM_LEITURAS - 1) Serial.print(", ");
  }
  Serial.println();

  Serial.print("Media da sessao: ");
  Serial.print(media, 1);
  Serial.println(" cm");

  Serial.print("Mediana da sessao: ");
  Serial.print(mediana, 1);
  Serial.println(" cm");

  atualizarEstado(mediana);

  unsigned long restante = (INTERVALO_SESSAO - (millis() - tempoInicioSessao)) / 1000;
  Serial.print("Proxima sessao em ");
  Serial.print(restante);
  Serial.println(" segundos.");
  Serial.println("----------------------------------------");

  sessaoEmAndamento = false;
}

void atualizarEstado(float mediana) {
  if (mediana >= 16.0) {
    estadoAlerta = true;
  } else if (mediana <= 14.0) {
    estadoAlerta = false;
  }
  // entre 14 e 16 (exclusive): mantem o estado anterior, nao faz nada

  if (estadoAlerta) {
    Serial.println("Estado: ALERTA");
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
  } else {
    Serial.println("Estado: NORMAL");
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
  }
}