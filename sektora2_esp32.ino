#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ================= WIFI =================
const char* ssid = "Rumah Oppung";
const char* password = "Oppung789!";

// ================= MQTT (HiveMQ Cloud) =================
const char* mqtt_server = "3f45b22d5630410eae9db48c42d47df2.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "naufalyogi";
const char* mqtt_pass = "Naufalyogi123";

WiFiClientSecure espClient;
PubSubClient client(espClient);

// ================= DEVICE =================
String deviceID = "SECTOR_A1";

// ================= SERIAL MEGA (UART2 ESP32) =================
HardwareSerial megaSerial(2);
#define RXD2 16
#define TXD2 17

const int ledkirim = 18;
const int ledwifi = 19;
// ================= DATA =================
String timestampMega;
float suhu;
float kelembaban;
int pm25;
int pm10;
float kecepatan_angin;
int arah_angin;

unsigned long lastSend = 0;
bool newDataReady = false;

// =======================================================
// ================= WIFI ================================
// =======================================================

void connectWiFi() {
  Serial.print("Connecting WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  digitalWrite(ledwifi, HIGH);
  delay(5000);
  digitalWrite(ledwifi, LOW);
  delay(100);
}

// =======================================================
// ================= MQTT ================================
// =======================================================

void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");

    if (client.connect(deviceID.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Connected to HiveMQ");
    } else {
      Serial.print("Failed rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// =======================================================
// ================= PARSE DATA DARI MEGA =================
// FORMAT BARU:
// timestamp,temp,hum,pm25,pm10,kecepatan,arah_derajat
// =======================================================

bool parseMega(String data) {

  int i1 = data.indexOf(',');
  int i2 = data.indexOf(',', i1 + 1);
  int i3 = data.indexOf(',', i2 + 1);
  int i4 = data.indexOf(',', i3 + 1);
  int i5 = data.indexOf(',', i4 + 1);
  int i6 = data.indexOf(',', i5 + 1);

  if (i1 < 0 || i2 < 0 || i3 < 0 || i4 < 0 || i5 < 0 || i6 < 0) {
    Serial.println("Format data salah!");
    return false;
  }

  timestampMega     = data.substring(0, i1);
  suhu              = data.substring(i1 + 1, i2).toFloat();
  kelembaban        = data.substring(i2 + 1, i3).toFloat();
  pm25              = data.substring(i3 + 1, i4).toInt();
  pm10              = data.substring(i4 + 1, i5).toInt();
  kecepatan_angin   = data.substring(i5 + 1, i6).toFloat();
  arah_angin      = data.substring(i6 + 1).toInt();   // langsung derajat

  return true;
}

// =======================================================
// ================= SETUP ===============================
// =======================================================

void setup() {

  Serial.begin(115200);
  megaSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  pinMode(ledkirim, OUTPUT);
  pinMode(ledwifi, OUTPUT);
  connectWiFi();

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
}

// =======================================================
// ================= LOOP ================================
// =======================================================

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!client.connected()) {
    connectMQTT();
  }

  client.loop();

  // ===== BACA DATA DARI MEGA =====
  if (megaSerial.available()) {

    String data = megaSerial.readStringUntil('\n');
    data.trim();

    if (parseMega(data)) {
      newDataReady = true;
      Serial.println("Data dari Mega:");
      Serial.println(data);
    }
  }

  // ===== Publish jika ada data baru =====
  if (newDataReady && millis() - lastSend > 5000) {

    String payload = "{";
    payload += "\"device\":\"" + deviceID + "\",";
    payload += "\"timestamp\":\"" + timestampMega + "\",";
    payload += "\"suhu\":" + String(suhu, 2) + ",";
    payload += "\"kelembaban\":" + String(kelembaban, 2) + ",";
    payload += "\"pm25\":" + String(pm25) + ",";
    payload += "\"pm10\":" + String(pm10) + ",";
    payload += "\"kecepatan_angin\":" + String(kecepatan_angin, 2) + ",";
    payload += "\"arah_angin\":" + String(arah_angin);
    payload += "}";

    bool ok = client.publish(("air/" + deviceID).c_str(), payload.c_str());

    if (ok) {
    Serial.println("MQTT Publish OK");
    digitalWrite(ledkirim, HIGH);
    delay(1500);
    digitalWrite(ledkirim, LOW);
    delay(100);
    } else {
      Serial.println("MQTT Publish FAILED");
    }

    Serial.println(payload);

    lastSend = millis();
    newDataReady = false;
  }
}
