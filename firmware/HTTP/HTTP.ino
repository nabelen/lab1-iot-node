/*
  ============================================================
  LAB 1 - NODO IoT DE CONFORT TERMICO
  ESP32 + DHT + LCD 16x2 + VENTILADOR
  WiFi + HTTP / MQTT + JSON
  ============================================================
  Experimento A (parte 1): solo HTTP.
  Cada envio imprime una linea CSV:
  DATA,proto,seq,rssi,latencia_ms,exito,bytes_payload

  LIBRERIAS: DHT sensor library, Adafruit Unified Sensor,
             ArduinoJson (v6), PubSubClient
  ============================================================
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <LiquidCrystal.h>
#include <time.h>

// ============================================================
// CONFIGURACION
// ============================================================

#define NODE_ID "ESP32_01"

const char* WIFI_SSID     = "TP-Link_3718";
const char* WIFI_PASSWORD = "13184025";

#define HTTP_MODE 0
#define MQTT_MODE 1
#define TRANSPORT_MODE HTTP_MODE      // <- cambiar a MQTT_MODE para MQTT

// HTTP: cambia la IP por la de tu PC (ipconfig)
const char* HTTP_SERVER_URL = "http://192.168.0.103:5000/telemetry";
const unsigned long HTTP_TIMEOUT_MS = 3000;

// MQTT
const char* MQTT_BROKER_HOST = "broker.hivemq.com";
const int   MQTT_BROKER_PORT = 1883;
const char* MQTT_TOPIC       = "telecom/lab1/" NODE_ID "/telemetry";

// Sensor
#define DHT_PIN  4
#define DHT_TYPE DHT11 
DHT dht(DHT_PIN, DHT_TYPE);

// LCD 16x2 sin I2C: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(14, 27, 33, 32, 18, 19);

// Rele HW-042 (activo en LOW)
#define FAN_PIN 26
#define FAN_ON  LOW
#define FAN_OFF HIGH

// Umbrales
const float TEMP_MAX_CONFORT = 26.0;
const float TEMP_MIN_CONFORT = 18.0;

// Periodo de muestreo (1000, 5000, 10000)
const unsigned long SAMPLE_PERIOD_MS = 10000;

// Reconexion WiFi
const unsigned long RECONNECT_INTERVAL_MS = 5000;
unsigned long lastReconnectAttempt = 0;

// NTP (timestamp real)
const char* NTP_SERVER = "pool.ntp.org";

// ============================================================
// OBJETOS Y VARIABLES GLOBALES
// ============================================================

WiFiClient   espClient;
PubSubClient mqttClient(espClient);

uint32_t seqCounter   = 0;
uint32_t totalSent    = 0;
uint32_t totalSuccess = 0;
bool     fanState     = false;

struct SensorReading
{
  float temperature;
  float humidity;
  bool  valid;
};

// ============================================================
// ADQUISICION
// ============================================================

SensorReading acquireValue()
{
  SensorReading r;
  r.temperature = dht.readTemperature();
  r.humidity    = dht.readHumidity();
  r.valid       = !(isnan(r.temperature) || isnan(r.humidity));

  if (!r.valid)
    Serial.println("[ADQUISICION] ERROR: No se pudo leer el DHT.");

  return r;
}

// ============================================================
// ACTUACION Y PANTALLA
// ============================================================

void controlFan(float temperature)
{
  fanState = (temperature >= TEMP_MAX_CONFORT);
  digitalWrite(FAN_PIN, fanState ? FAN_ON : FAN_OFF);
}

void updateLCD(const SensorReading& reading)
{
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
// CONECTIVIDAD WiFi
// ============================================================

bool isWifiConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

void setupConnectivity()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WiFi] Conectando");
  unsigned long start = millis();

  while (!isWifiConnected() && millis() - start < 15000)
  {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (isWifiConnected())
  {
    Serial.println("[WiFi] Conectado.");
    Serial.print("[WiFi] IP: ");      Serial.println(WiFi.localIP());
    Serial.print("[WiFi] Gateway: "); Serial.println(WiFi.gatewayIP());
    Serial.print("[WiFi] Mascara: "); Serial.println(WiFi.subnetMask());
    Serial.print("[WiFi] RSSI: ");    Serial.println(WiFi.RSSI());

    configTime(0, 0, NTP_SERVER);     // sincroniza hora (UTC)
  }
  else
  {
    Serial.println("[WiFi] No conectado. Se reintentara.");
  }
}

void maintainConnection()
{
  if (isWifiConnected()) return;

  unsigned long now = millis();
  if (now - lastReconnectAttempt >= RECONNECT_INTERVAL_MS)
  {
    lastReconnectAttempt = now;
    Serial.println("[WiFi] Conexion perdida. Reintentando...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

// Epoch real si NTP ya sincronizo; si no, segundos desde el arranque
uint32_t getTimestamp()
{
  time_t now = time(nullptr);
  if (now > 1700000000) return (uint32_t)now;
  return millis() / 1000;
}

// ============================================================
// CONSTRUCCION DEL MENSAJE JSON
// ============================================================

String buildJson(const SensorReading& reading, uint32_t seq)
{
  StaticJsonDocument<640> doc;   // sube el tamaño porque agregamos campos

  doc["node_id"]     = NODE_ID;
  doc["seq"]         = seq;
  doc["timestamp"]   = getTimestamp();
  doc["value"]       = reading.temperature;
  doc["temperature"] = reading.temperature;
  doc["humidity"]    = reading.humidity;
  doc["fan"]         = fanState;
  doc["rssi"]        = WiFi.RSSI();
  doc["uptime_ms"]   = millis();

  // --- NUEVO: info de red ---
  doc["ip"]          = WiFi.localIP().toString();
  doc["gateway"]     = WiFi.gatewayIP().toString();
  doc["subnet"]      = WiFi.subnetMask().toString();
  doc["mac"]         = WiFi.macAddress();
  doc["ssid"]        = WiFi.SSID();
  // ---------------------------

  if (reading.temperature < TEMP_MIN_CONFORT)
    doc["alert_type"] = "temp_low";
  else if (reading.temperature >= TEMP_MAX_CONFORT)
    doc["alert_type"] = "temp_high";
  else
    doc["alert_type"] = "none";

  String payload;
  serializeJson(doc, payload);
  return payload;
}

// ============================================================
// REGISTRO DE RESULTADOS (CSV para Excel / Python)
// ============================================================

void logResult(const char* proto, uint32_t seq, unsigned long latency,
               bool ok, size_t bytes)
{
  // DATA,protocolo,seq,rssi,latencia_ms,exito,bytes_payload
  Serial.printf("DATA,%s,%lu,%d,%lu,%d,%u\n",
                proto, (unsigned long)seq, WiFi.RSSI(),
                latency, ok ? 1 : 0, (unsigned)bytes);
}

// ============================================================
// TRANSPORTE HTTP
// ============================================================

bool sendHttp(const String& payload, uint32_t seq)
{
  if (!isWifiConnected()) return false;

  HTTPClient http;

  if (!http.begin(HTTP_SERVER_URL))
  {
    Serial.println("[HTTP] ERROR al iniciar HTTP");
    return false;
  }

  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");

  unsigned long t0 = millis();
  int httpCode = http.POST(payload);
  unsigned long latency = millis() - t0;

  bool ok = (httpCode == 200 || httpCode == 201);

  Serial.printf("[HTTP] Codigo=%d | Latencia=%lu ms | %s\n",
                httpCode, latency, ok ? "OK" : "ERROR");

  logResult("HTTP", seq, latency, ok, payload.length());

  http.end();
  return ok;
}

// ============================================================
// TRANSPORTE MQTT (version basica; el ACK se agrega despues)
// ============================================================

void mqttReconnect()
{
  if (!isWifiConnected() || mqttClient.connected()) return;

  Serial.print("[MQTT] Conectando...");
  String clientId = String(NODE_ID) + "-" + String(random(0xffff), HEX);

  if (mqttClient.connect(clientId.c_str()))
  {
    Serial.println(" OK");
    Serial.print("[MQTT] Topic: ");
    Serial.println(MQTT_TOPIC);
  }
  else
  {
    Serial.print(" ERROR rc=");
    Serial.println(mqttClient.state());
  }
}

bool publishMqtt(const String& payload, uint32_t seq)
{
  if (!mqttClient.connected()) mqttReconnect();
  if (!mqttClient.connected())
  {
    Serial.println("[MQTT] No conectado.");
    return false;
  }

  unsigned long t0 = millis();
  bool ok = mqttClient.publish(MQTT_TOPIC, payload.c_str());
  unsigned long latency = millis() - t0;

  Serial.printf("[MQTT] %s | Latencia local=%lu ms | seq=%lu\n",
                ok ? "OK" : "ERROR", latency, (unsigned long)seq);
  return ok;
}

// ============================================================
// ESTADISTICAS
// ============================================================

void showStatistics()
{
  Serial.println();
  Serial.println("========== ESTADISTICAS ==========");
  Serial.print("Mensajes enviados:  "); Serial.println(totalSent);
  Serial.print("Mensajes exitosos:  "); Serial.println(totalSuccess);

  if (totalSent > 0)
  {
    Serial.print("Tasa de exito: ");
    Serial.print((totalSuccess * 100.0) / totalSent, 2);
    Serial.println("%");
  }

  Serial.print("RSSI actual: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.println("==================================");
  Serial.println();
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("====================================");
  Serial.println(" NODO IoT DE CONFORT TERMICO");
  Serial.println(" ESP32 + DHT + LCD + VENTILADOR");
  Serial.println("====================================");
  Serial.print("NODE_ID: ");
  Serial.println(NODE_ID);
  Serial.println(TRANSPORT_MODE == HTTP_MODE ? "MODO: HTTP" : "MODO: MQTT");

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
  delay(2000);

  setupConnectivity();

  mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  mqttClient.setBufferSize(512);

  Serial.println();
  Serial.println("Sistema listo.");
  Serial.println("DATA_HEADER,proto,seq,rssi,lat_ms,exito,bytes");
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  maintainConnection();

  if (TRANSPORT_MODE == MQTT_MODE && isWifiConnected())
  {
    if (!mqttClient.connected()) mqttReconnect();
    mqttClient.loop();
  }

  // ---- Adquisicion ----
  SensorReading reading = acquireValue();

  if (!reading.valid)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error DHT");
    delay(SAMPLE_PERIOD_MS);
    return;
  }

  // ---- Control y pantalla ----
  controlFan(reading.temperature);
  updateLCD(reading);

  Serial.println();
  Serial.println("---------- LECTURA ----------");
  Serial.printf("Temperatura: %.1f C\n", reading.temperature);
  Serial.printf("Humedad: %.1f %%\n", reading.humidity);
  Serial.printf("Ventilador: %s\n", fanState ? "ENCENDIDO" : "APAGADO");
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());

  // ---- Mensaje ----
  String payload = buildJson(reading, seqCounter);
  Serial.print("JSON: ");
  Serial.println(payload);

  // ---- Transporte ----
  if (isWifiConnected())
  {
    totalSent++;
    bool success;

    if (TRANSPORT_MODE == HTTP_MODE)
      success = sendHttp(payload, seqCounter);
    else
      success = publishMqtt(payload, seqCounter);

    if (success) totalSuccess++;
  }
  else
  {
    Serial.println("[TRANSPORTE] Sin WiFi. No se envia.");
  }

  seqCounter++;

  if (seqCounter % 10 == 0) showStatistics();

  delay(SAMPLE_PERIOD_MS);

}