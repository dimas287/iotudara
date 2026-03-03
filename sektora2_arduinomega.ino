#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>
#include <SD_ZH03B.h>

// ================= SENSOR =================
Adafruit_AHTX0 aht;
RTC_DS3231 rtc;
SD_ZH03B ZH03B(Serial1, SD_ZH03B::SENSOR_ZH03B);

// ================= SD =================
const int SD_CS = 53;
const int ledkirim = 48;
// ================= ANEMOMETER =================
volatile unsigned long rpmcount = 0;
volatile unsigned long lastMicros = 0;

// ================= DATA BUFFER (AKUMULASI 1 MENIT) =================
unsigned long sampleCount = 0;

// AHT20
float sumT = 0;
float sumH = 0;

// PM
int lastPM25 = 0;
int lastPM10 = 0;
unsigned long sumPM25 = 0;
unsigned long sumPM10 = 0;

// Wind speed
unsigned long lastAnemo = 0;
unsigned long anemoCount = 0;
float sumWind = 0;

// Wind direction – 3 detik terakhir
int last3secDirection = -1;
unsigned long lastWindDirTime = 0;

// ================= INTERVAL =================
unsigned long lastSend = 0;
const unsigned long intervalKirim = 60000;

// ================= ANEMO INTERRUPT =================
void rpm_anemometer() {
  if (micros() - lastMicros >= 5000) {
    rpmcount++;
    lastMicros = micros();
  }
}

// ================= WIND DIR =================
int kodeToDegree(String kode) {
  if (kode == "1") return 0;
  if (kode == "2") return 45;
  if (kode == "3") return 90;
  if (kode == "4") return 135;
  if (kode == "5") return 180;
  if (kode == "6") return 225;
  if (kode == "7") return 270;
  if (kode == "8") return 315;
  return -1;
}

void setup() {
  Serial.begin(9600);

  Serial1.begin(9600); // ZH03B
  Serial2.begin(9600); // wind direction
  Serial3.begin(9600); // ESP32

  Wire.begin();
  rtc.begin();
  aht.begin();
  pinMode(ledkirim, OUTPUT);
  ZH03B.setMode(SD_ZH03B::IU_MODE);

  SD.begin(SD_CS);

  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), rpm_anemometer, RISING);

  lastAnemo = millis();
  lastSend  = millis();
}

// ============================================================
// ============================== LOOP =========================
// ============================================================
void loop() {

  DateTime now = rtc.now();
  String timestamp = now.timestamp();

  // ================= AHT20 =================
  sensors_event_t hum, temp;
  aht.getEvent(&hum, &temp);

  float T = temp.temperature;
  float H = hum.relative_humidity;

  sumT += T;
  sumH += H;

  // ================= ZH03B =================
  if (ZH03B.readData()) {
    lastPM25 = ZH03B.getPM2_5();
    lastPM10 = ZH03B.getPM10_0();
  }

  sumPM25 += lastPM25;
  sumPM10 += lastPM10;

  // ================= WIND DIRECTION (3 detik terakhir) =================
  if (Serial2.available()) {
    String raw = Serial2.readStringUntil('#');
    int pos = raw.indexOf('*');
    if (pos != -1) {
      int deg = kodeToDegree(raw.substring(pos + 1));
      if (deg >= 0) {
        if (millis() - lastWindDirTime >= 3000) {
          last3secDirection = deg;
          lastWindDirTime = millis();
        }
      }
    }
  }

  // ================= WIND SPEED (tiap 10 detik) =================
  if (millis() - lastAnemo >= 10000) {

    noInterrupts();
    float rps = rpmcount / 10.0;
    rpmcount = 0;
    interrupts();

    float wind =
      (-0.0181 * rps * rps) +
      (1.3859 * rps) +
      1.4055;

    if (wind < 1.5) wind = 0;

    sumWind += wind;
    anemoCount++;

    lastAnemo = millis();
  }

  sampleCount++;

  // ================= KIRIM & RESET (1 menit) =================
  if (millis() - lastSend >= intervalKirim) {
    lastSend = millis();

    float avgT = sumT / sampleCount;
    float avgH = sumH / sampleCount;
    float avgPM25 = sumPM25 / sampleCount;
    float avgPM10 = sumPM10 / sampleCount;
    float avgWind = (anemoCount > 0) ? (sumWind / anemoCount) : 0;

    int dirFinal = (last3secDirection >= 0) ? last3secDirection : 0;

    // ================= KIRIM KE ESP32 =================
    String paket =
      timestamp + "," +
      String(avgT,2) + "," +
      String(avgH,2) + "," +
      String(avgPM25,0) + "," +
      String(avgPM10,0) + "," +
      String(avgWind,2) + "," +
      String(dirFinal);

    Serial3.println(paket);
    Serial.println("KIRIM KE ESP32:");
    Serial.println(paket);
    digitalWrite(ledkirim, HIGH);
    delay(1000);
    digitalWrite(ledkirim, LOW);
    delay(1000);

    // ================= SIMPAN SD =================
    File f = SD.open("data2.csv", FILE_WRITE);
    if (f) {
      f.print(timestamp); f.print(",");
      f.print(avgT); f.print(",");
      f.print(avgH); f.print(",");
      f.print(avgPM25); f.print(",");
      f.print(avgPM10); f.print(",");
      f.print(avgWind); f.print(",");
      f.println(dirFinal);
      f.close();
    }

    // ================= RESET AKUMULATOR =================
    sampleCount = 0;
    sumT = 0;  
    sumH = 0;
    sumPM25 = 0;
    sumPM10 = 0;
    sumWind = 0;
    anemoCount = 0;
  }

  // ================= DEBUG 1 DETIK =================
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug >= 1000) {
    lastDebug = millis();

    Serial.print("T="); Serial.print(T);
    Serial.print(" PM25="); Serial.print(lastPM25);
    Serial.print(" PM10="); Serial.print(lastPM10);
    Serial.print(" Dir3s="); Serial.print(last3secDirection);
    Serial.print(" Sampel="); Serial.println(sampleCount);
  }
}
