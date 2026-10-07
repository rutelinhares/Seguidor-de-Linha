/*
  Robô seguidor de linha preta
  Placa: ESP32-S3-WROOM-1 | Sensor: Funduino Tracker Sensor V1.0 (3 canais) | Ponte H: DRV8833
  Core Arduino-ESP32 3.x (se usar o 2.x, veja a nota perto de setupMotores)

  LIGAÇÕES (pinos escolhidos da régua de bornes da sua placa)
  -----------------------------------------------------------
  Sensor  VCC -> 3V3   (alimente com 3,3 V para a saída ser segura para o ESP32)
  Sensor  GND -> GND
  Sensor  L   -> GPIO 4   (esquerdo)
  Sensor  C   -> GPIO 5   (central, o pino do meio, sem legenda na placa)
  Sensor  R   -> GPIO 6   (direito)

  DRV8833 AIN1 -> GPIO 15   (motor esquerdo)
  DRV8833 AIN2 -> GPIO 16
  DRV8833 BIN1 -> GPIO 17   (motor direito)
  DRV8833 BIN2 -> GPIO 18
  DRV8833 VCC  -> bateria (2,7 a 10,8 V)  | GND -> GND comum com o ESP32
  DRV8833 SLEEP/nSLEEP -> 3V3 (se não ligar, o driver fica desligado)

  Importante: todos os GNDs (bateria, DRV8833, sensor, ESP32) devem estar unidos.
*/
#include<Arduino.h>

// ---------------- Pinos ----------------
const int PIN_SENSOR_L = 4;
const int PIN_SENSOR_C = 5;
const int PIN_SENSOR_R = 6;

const int PIN_AIN1 = 15;
const int PIN_AIN2 = 16;
const int PIN_BIN1 = 17;
const int PIN_BIN2 = 18;

// ---------------- Configurações ----------------
// Muitos módulos TCRT5000 dão nível ALTO sobre o preto e BAIXO sobre o branco.
// Se o robô se comportar ao contrário, troque para LOW (ou observe os LEDs verdes
// do módulo e o monitor serial para descobrir).
const int NIVEL_LINHA = HIGH;

const int PWM_FREQ = 20000;   // 20 kHz (silencioso)
const int PWM_RES  = 8;       // 8 bits: 0 a 255

int VEL_BASE   = 150;         // velocidade em linha reta (0-255)
int VEL_CURVA  = 190;         // roda de fora na curva
int VEL_INTERNA = 0;          // roda de dentro na curva (0 = parada; negativo = ré)
int VEL_BUSCA  = 130;         // giro quando perde a linha

// Memória da última direção em que a linha foi vista
enum Lado { NENHUM, ESQUERDA, DIREITA };
Lado ultimoLado = NENHUM;

// ---------------- Motores ----------------
// Velocidade de -255 a 255 (negativo = ré)
void motor(int pinIn1, int pinIn2, int vel) {
  vel = constrain(vel, -255, 255);
  if (vel > 0) {
    ledcWrite(pinIn1, vel);
    ledcWrite(pinIn2, 0);
  } else if (vel < 0) {
    ledcWrite(pinIn1, 0);
    ledcWrite(pinIn2, -vel);
  } else {
    ledcWrite(pinIn1, 0);
    ledcWrite(pinIn2, 0);
  }
}

void motores(int esq, int dir) {
  motor(PIN_AIN1, PIN_AIN2, esq);
  motor(PIN_BIN1, PIN_BIN2, dir);
}

void setupMotores() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_AIN1, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_AIN2, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_BIN1, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_BIN2, PWM_FREQ, PWM_RES);
#else
  // Core 2.x: PWM por canal (0 a 3), não por pino
  ledcSetup(0, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_AIN1, 0);
  ledcSetup(1, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_AIN2, 1);
  ledcSetup(2, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_BIN1, 2);
  ledcSetup(3, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_BIN2, 3);
#endif
  motores(0, 0);
}

// ---------------- Sensores ----------------
bool naLinha(int pin) {
  return digitalRead(pin) == NIVEL_LINHA;
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_SENSOR_L, INPUT);
  pinMode(PIN_SENSOR_C, INPUT);
  pinMode(PIN_SENSOR_R, INPUT);

  setupMotores();

  Serial.println("Seguidor de linha pronto. Iniciando em 2 s...");
  delay(2000);
}

void loop() {
  bool L = naLinha(PIN_SENSOR_L);
  bool C = naLinha(PIN_SENSOR_C);
  bool R = naLinha(PIN_SENSOR_R);

  // Motor esquerdo = A, motor direito = B.
  // Se algum motor girar ao contrário, inverta os fios dele no DRV8833
  // (ou troque os pinos IN1/IN2 correspondentes aqui em cima).

  if (C && !L && !R) {
    // Centralizado: segue reto
    motores(VEL_BASE, VEL_BASE);

  } else if (L && !R) {
    // Linha à esquerda (com ou sem C): vira à esquerda
    motores(VEL_INTERNA, VEL_CURVA);
    ultimoLado = ESQUERDA;

  } else if (R && !L) {
    // Linha à direita: vira à direita
    motores(VEL_CURVA, VEL_INTERNA);
    ultimoLado = DIREITA;

  } else if (L && C && R) {
    // Os três na linha (cruzamento ou faixa larga): segue reto
    motores(VEL_BASE, VEL_BASE);

  } else {
    // Nenhum sensor na linha (ou leitura estranha): procura onde foi vista por último
    if (ultimoLado == ESQUERDA) {
      motores(-VEL_BUSCA, VEL_BUSCA);
    } else if (ultimoLado == DIREITA) {
      motores(VEL_BUSCA, -VEL_BUSCA);
    } else {
      motores(0, 0);
    }
  }

  // Depuração (comente depois de calibrar)
  static unsigned long t = 0;
  if (millis() - t > 200) {
    t = millis();
    Serial.printf("L=%d C=%d R=%d\n", L, C, R);
  }
}
