/*
  ============================================================
  LAB 1 - NODO IoT DE CONFORT TERMICO
  ESP32 + DHT + LCD 16x2 + VENTILADOR
  WiFi + MQTT (Mosquitto Local) / HTTP + JSON
  ============================================================
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <LiquidCrystal.h>

// ============================================================
// IDENTIFICACIÓN DEL NODO Y WiFi
// ============================================================

#define NODE_ID "ESP32_01"

const char* WIFI_SSID     = "TP-Link_3718";
const char* WIFI_PASSWORD = "13184025";

// ============================================================
// SELECCIÓN DEL PROTOCOLO
// ============================================================

#define HTTP_MODE 0
#define MQTT_MODE 1

#define TRANSPORT_MODE MQTT_MODE // MODO MQTT SELECCIONADO

// ============================================================
// CONFIGURACIÓN HTTP Y MQTT (MOSQUITTO LOCAL)
// ============================================================

const char* HTTP_SERVER_URL  = "http://192.168.0.103:5000/telemetry";

// IP de la PC donde corre Mosquitto
const char* MQTT_BROKER_HOST = "192.168.0.103"; 
const int   MQTT_BROKER_PORT = 1883;

const char* MQTT_TOPIC     = "telecom/lab1/" NODE_ID "/telemetry";
const char* MQTT_ACK_TOPIC = "telecom/lab1/" NODE_ID "/ack";

// ============================================================
// PERIFÉRICOS
// ============================================================

#define DHT_PIN 4
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

LiquidCrystal lcd(14, 27, 33, 32, 18, 19);

#define FAN_PIN 26
#define FAN_ON  LOW
#define FAN_OFF HIGH

const float TEMP_MAX_CONFORT = 26.0;
const unsigned long SAMPLE_PERIOD_MS = 10000;
unsigned long lastSampleTime = 0;

const unsigned long RECONNECT_INTERVAL_MS = 5000;
unsigned long lastReconnectAttempt = 0;

// ============================================================
// OBJETOS Y VARIABLES GLOBALES
// ============================================================

WiFiClient espClient;
PubSubClient mqttClient(espClient);

uint32_t seqCounter   = 0;
uint32_t totalSent    = 0;
uint32_t totalSuccess = 0;

volatile bool ackReceived = false;
volatile uint32_t ackSeq  = 0;
bool fanState = false;

struct SensorReading {
  float temperature;
  float humidity;
  bool valid;
};

// ============================================================
// ADQUISICIÓN Y CONTROL
// ============================================================

SensorReading acquireValue() {
  SensorReading r;
  r.temperature = dht.readTemperature();
  r.humidity    = dht.readHumidity();
  r.valid       = !(isnan(r.temperature) || isnan(r.humidity));

  if (!r.valid) {
    Serial.println("\n[ADQUISICION] ERROR: No se pudo leer el DHT.");
  }
  return r;
}

void controlFan(float temperature) {
  if (temperature >= TEMP_MAX_CONFORT) {
    digitalWrite(FAN_PIN, FAN_ON);
    fanState = true;
  } else {
    digitalWrite(FAN_PIN, FAN_OFF);
    fanState = false;
  }
}

void updateLCD(const SensorReading& reading) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(reading.temperature, 1);
  lcd.write(byte(223));
  lcd.print("C H:");
  lcd.print(reading.humidity, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Vent:");
  lcd.print(fanState ? "ON " : "OFF");
  lcd.print(" ID:");
  lcd.print(NODE_ID);
}

// ============================================================
// REGISTRO DE RESULTADOS (FORMATO CSV)
// ============================================================

void logResult(const char* proto, uint32_t seq, unsigned long latency, bool ok, size_t bytes) {
  // DATA,protocolo,seq,rssi,latencia_ms,exito,bytes_payload
  Serial.printf("DATA,%s,%lu,%d,%lu,%d,%u\n",
                proto, (unsigned long)seq, WiFi.RSSI(),
                latency, ok ? 1 : 0, (unsigned)bytes);
}

// ============================================================
// CONECTIVIDAD WiFi
// ============================================================

void setupConnectivity() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Conectando");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Conectado.");
    Serial.print("[WiFi] IP: "); Serial.println(WiFi.localIP());
    Serial.print("[WiFi] RSSI: "); Serial.println(WiFi.RSSI());
  } else {
    Serial.println("[WiFi] No conectado. Se reintentara en segundo plano.");
  }
}

void maintainConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt >= RECONNECT_INTERVAL_MS) {
      lastReconnectAttempt = now;
      Serial.println("[WiFi] Reconectando...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}

bool isWifiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

// ============================================================
// CREAR JSON
// ============================================================

String buildJson(const SensorReading& reading, uint32_t seq) {
  StaticJsonDocument<512> doc;

  doc["node_id"]     = NODE_ID;
  doc["seq"]         = seq;
  doc["timestamp"]   = millis() / 1000;
  doc["value"]       = reading.temperature;
  doc["temperature"] = reading.temperature;
  doc["humidity"]    = reading.humidity;
  doc["fan"]         = fanState;
  doc["rssi"]        = WiFi.RSSI();
  doc["uptime_ms"]   = millis();

  if (reading.temperature < 18.0) {
    doc["alert_type"] = "temp_low";
  } else if (reading.temperature > 26.0) {
    doc["alert_type"] = "temp_high";
  } else {
    doc["alert_type"] = "none";
  }

  String payload;
  serializeJson(doc, payload);
  return payload;
}

// ============================================================
// CALLBACK Y CONEXIÓN MQTT
// ============================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, payload, length);

  if (err) {
    Serial.print("[MQTT] ACK invalido: ");
    Serial.println(err.c_str());
    return;
  }

  ackSeq = doc["seq"] | 0;
  ackReceived = true;
}

void mqttReconnect() {
  if (!isWifiConnected() || mqttClient.connected()) {
    return;
  }

  Serial.print("[MQTT] Conectando...");
  String clientId = String(NODE_ID) + "-" + String(random(0xffff), HEX);

  if (mqttClient.connect(clientId.c_str())) {
    Serial.println(" OK");
    mqttClient.subscribe(MQTT_ACK_TOPIC);
    Serial.print("[MQTT] Suscrito a ACK: ");
    Serial.println(MQTT_ACK_TOPIC);
  } else {
    Serial.print(" ERROR rc=");
    Serial.println(mqttClient.state());
  }
}

// ============================================================
// PUBLICAR MQTT
// ============================================================

bool publishMqtt(const String& payload, uint32_t seq) {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }

  if (!mqttClient.connected()) {
    Serial.println("[MQTT] No conectado.");
    return false;
  }

  ackReceived = false;
  unsigned long t0 = millis();

  bool sent = mqttClient.publish(MQTT_TOPIC, payload.c_str());

  const unsigned long ACK_TIMEOUT_MS = 4000;
  while (!ackReceived && (millis() - t0) < ACK_TIMEOUT_MS) {
    mqttClient.loop();
    delay(10);
  }

  unsigned long latency = millis() - t0;
  bool ok = sent && ackReceived && (ackSeq == seq);

  Serial.print("[MQTT] ");
  if (ok) {
    Serial.print("OK");
  } else if (!ackReceived) {
    Serial.print("TIMEOUT (sin ACK)");
  } else {
    Serial.print("ERROR");
  }

  Serial.print(" | Latencia RTT=");
  Serial.print(latency);
  Serial.print(" ms | seq=");
  Serial.println(seq);

  Serial.print("[MQTT] JSON: ");
  Serial.println(payload);

  // Línea CSV para el análisis estadístico
  logResult("MQTT", seq, latency, ok, payload.length());

  return ok;
}

// ============================================================
// ENVÍO HTTP
// ============================================================

bool sendHttp(const String& payload, uint32_t seq) {
  if (!isWifiConnected()) return false;

  HTTPClient http;
  Serial.print("[HTTP] Enviando...");

  if (!http.begin(HTTP_SERVER_URL)) {
    Serial.println(" ERROR al iniciar HTTP");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  unsigned long t0 = millis();
  int httpCode = http.POST(payload);
  unsigned long latency = millis() - t0;

  bool ok = (httpCode == 200 || httpCode == 201);

  Serial.print("[HTTP] Codigo=");
  Serial.print(httpCode);
  Serial.print(" | Latencia=");
  Serial.print(latency);
  Serial.println(ok ? " ms | OK" : " ms | ERROR");

  Serial.print("[HTTP] JSON: ");
  Serial.println(payload);

  logResult("HTTP", seq, latency, ok, payload.length());

  http.end();
  return ok;
}

// ============================================================
// ESTADÍSTICAS
// ============================================================

void showStatistics() {
  Serial.println("\n========== ESTADISTICAS ==========");
  Serial.print("Mensajes enviados: ");
  Serial.println(totalSent);
  Serial.print("Mensajes exitosos: ");
  Serial.println(totalSuccess);

  if (totalSent > 0) {
    float successRate = (totalSuccess * 100.0) / totalSent;
    Serial.print("Tasa de exito: ");
    Serial.print(successRate, 2);
    Serial.println("%");
  }

  Serial.print("RSSI actual: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.println("==================================\n");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n====================================");
  Serial.println(" NODO IoT DE CONFORT TERMICO");
  Serial.println("====================================");

  dht.begin();

  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, FAN_OFF);
  fanState = false;

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nodo IoT");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  setupConnectivity();

  mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  mqttClient.setCallback(mqttCallback);

  Serial.println("Sistema listo.\n====================================");
}

// ============================================================
// LOOP (NO BLOQUEANTE)
// ============================================================

void loop() {
  maintainConnection();

  if (TRANSPORT_MODE == MQTT_MODE && isWifiConnected()) {
    if (!mqttClient.connected()) {
      mqttReconnect();
    }
    mqttClient.loop();
  }

  unsigned long now = millis();
  if (now - lastSampleTime >= SAMPLE_PERIOD_MS) {
    lastSampleTime = now;

    SensorReading reading = acquireValue();

    if (!reading.valid) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Error DHT");
      return;
    }

    controlFan(reading.temperature);
    updateLCD(reading);

    // Muestra los detalles de lectura que querías conservar
    Serial.println("\n---------- LECTURA ----------");
    Serial.print("Temperatura: ");
    Serial.print(reading.temperature, 1);
    Serial.println(" C");
    Serial.print("Humedad: ");
    Serial.print(reading.humidity, 1);
    Serial.println(" %");
    Serial.print("Ventilador: ");
    Serial.println(fanState ? "ENCENDIDO" : "APAGADO");
    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    String payload = buildJson(reading, seqCounter);

    bool success = false;
    if (isWifiConnected()) {
      totalSent++;
      if (TRANSPORT_MODE == HTTP_MODE) {
        success = sendHttp(payload, seqCounter);
      } else {
        success = publishMqtt(payload, seqCounter);
      }

      if (success) {
        totalSuccess++;
      }
    } else {
      Serial.println("[TRANSPORTE] Sin WiFi. No se envia.");
    }

    seqCounter++;

    if (seqCounter % 10 == 0) {
      showStatistics();
    }
  }
  if (seqCounter >= 6) {
  Serial.println("\n[FIN] Se completaron las 6 muestras. Sistema en pausa.");
  while (true) { delay(1000); } // Congela el bucle
}
}