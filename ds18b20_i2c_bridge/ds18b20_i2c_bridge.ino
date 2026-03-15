/**
 * DS18B20 → I²C-Bridge
 * =====================
 * Plattform : ESP32-C3
 * 1-Wire    : GPIO1  (DS18B20 Data, 4.7 kΩ Pull-Up nach 3.3 V)
 * Sensor-VCC: GPIO0  (OUTPUT HIGH → 3.3 V) — Strapping-Pin, muss HIGH bleiben!
 * Sensor-GND: GPIO2  (OUTPUT LOW  → GND)
 * Direktanschluss DS18B20: Pin0=3V3, Pin1=Data, Pin2=GND
 * I²C Slave : SDA = GPIO4, SCL = GPIO3
 * Onboard-LED: GPIO8 (blau) — 2× kurz blinken bei gültiger Messung
 *
 * Register-Layout (2 Byte, MSB first, LM75-kompatibel):
 *   Wert = int16_raw / 16.0  →  Auflösung 0.0625 °C
 *   z. B. 25.0 °C → raw = 400 = 0x0190, gesendet: 0x01 0x90
 *   Bei ungültigem Sensor → 0x8000 als Error-Marker
 *
 * Board-Manager : esp32 by Espressif  3.x.x  (IDF 5.x)
 * Board         : "ESP32C3 Dev Module"
 * Libraries     : OneWire, DallasTemperature, WiFi, ArduinoOTA
 *
 * Wire-Init (SO-Fix): Wire.begin(uint8_t addr, SDA, SCL) — Reihenfolge: addr ZUERST!
 * Erster Parameter MUSS uint8_t sein (nicht int), sonst falscher Überladungsaufruf.
 * Callbacks VOR begin() registrieren!
 *
 * OTA-Flash     : Arduino IDE → Werkzeuge → Port → "temp_bridge (ESP32-C3)" (Netzwerk)
 *                 Passwort: siehe OTA_PASSWORD unten
 *
 * ── ESPHome Gegenseite ────────────────────────────────────────────────────────
 *
 *   sensor:
 *     - platform: template
 *       id: sensor_temp_becken
 *       update_interval: 3s
 *       lambda: |-
 *         uint8_t buf[2];
 *         auto err = id(i2c_bus)->read(0x48, buf, 2);
 *         if (err != i2c::ERROR_OK) return {};
 *         int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
 *         if (raw == (int16_t)0x8000) return {};
 *         return raw / 16.0f;
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

// ── Konfiguration ─────────────────────────────────────────────────────────────
#define WIFI_SSID       "DEIN_WLAN_NAME"   // ← anpassen
#define WIFI_PASSWORD   "DEIN_WLAN_PASSWORT" // ← anpassen
#define OTA_HOSTNAME    "temp_bridge"
#define OTA_PASSWORD    "ota1234"           // ← anpassen

#define ONE_WIRE_PIN    1        // DS18B20 Datenleitung
#define PIN_SENSOR_VCC  0        // OUTPUT HIGH → 3.3 V für Sensor (Strapping-Pin!)
#define PIN_SENSOR_GND  2        // OUTPUT LOW  → GND für Sensor
#define I2C_SDA         4        // I²C Slave SDA
#define I2C_SCL         3        // I²C Slave SCL
#define I2C_ADDR        0x48     // Slave-Adresse
#define UPDATE_MS       3000     // Messintervall [ms]
#define PIN_LED           8      // Onboard-LED (blau, active LOW)
// ─────────────────────────────────────────────────────────────────────────────

OneWire           oneWire(ONE_WIRE_PIN);
DallasTemperature ds18b20(&oneWire);

portMUX_TYPE tempMux = portMUX_INITIALIZER_UNLOCKED;
int16_t      g_temp_raw = 0;
bool         g_valid    = false;

volatile bool    g_req_fired = false;
volatile int16_t g_req_sent  = 0;
volatile bool    g_req_valid = false;

// ── I²C-Callbacks ─────────────────────────────────────────────────────────────

void onReceive(int /*len*/) {
  while (Wire.available()) Wire.read();  // Register-Byte ignorieren
}

void onRequest() {
  int16_t snap;
  bool    valid;
  portENTER_CRITICAL_ISR(&tempMux);
    snap  = g_temp_raw;
    valid = g_valid;
  portEXIT_CRITICAL_ISR(&tempMux);

  if (!valid) {
    Wire.write((uint8_t)0x80);
    Wire.write((uint8_t)0x00);
    g_req_sent  = (int16_t)0x8000;
    g_req_valid = false;
  } else {
    Wire.write((uint8_t)((snap >> 8) & 0xFF));
    Wire.write((uint8_t)( snap       & 0xFF));
    g_req_sent  = snap;
    g_req_valid = true;
  }
  g_req_fired = true;
}

// ── Setup ──────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.println("║  DS18B20 → I²C-Bridge   (ESP32-C3)  ║");
  Serial.println("╚══════════════════════════════════════╝");
  Serial.printf("  1-Wire  : GPIO%d\n",       ONE_WIRE_PIN);
  Serial.printf("  Sens-VCC: GPIO%d (HIGH)\n", PIN_SENSOR_VCC);
  Serial.printf("  Sens-GND: GPIO%d (LOW)\n",  PIN_SENSOR_GND);
  Serial.printf("  I²C SDA : GPIO%d\n",        I2C_SDA);
  Serial.printf("  I²C SCL : GPIO%d\n",        I2C_SCL);
  Serial.printf("  Adresse : 0x%02X\n",        I2C_ADDR);
  Serial.printf("  Intervall: %d ms\n\n",      UPDATE_MS);

  // Sensor-Versorgung per GPIO
  pinMode(PIN_SENSOR_GND, OUTPUT);
  digitalWrite(PIN_SENSOR_GND, LOW);
  pinMode(PIN_SENSOR_VCC, OUTPUT);
  digitalWrite(PIN_SENSOR_VCC, HIGH);
  delay(10);

  // Onboard-LED (active LOW → HIGH = aus)
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  // 1-Wire
  ds18b20.begin();
  ds18b20.setResolution(12);
  ds18b20.setWaitForConversion(false);  // nicht-blockierend: requestTemperatures() kehrt sofort zurück
  int devCount = ds18b20.getDeviceCount();
  if (devCount == 0) {
    Serial.println("  WARNUNG: Kein DS18B20 gefunden!");
  } else {
    Serial.printf("  DS18B20 gefunden: %d Sensor(en)\n", devCount);
    DeviceAddress addr;
    if (ds18b20.getAddress(addr, 0)) {
      Serial.printf("  ROM-Code: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                    addr[0], addr[1], addr[2], addr[3],
                    addr[4], addr[5], addr[6], addr[7]);
    }
  }

  // I²C Slave — end() zuerst löscht versteckte Default-Init des Arduino-Cores
  // Callbacks VOR begin() → kein Race-Condition-Fenster
  Wire.end();
  delay(1);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
  uint8_t slaveAddr = I2C_ADDR;
  bool ok = Wire.begin(slaveAddr, I2C_SDA, I2C_SCL);
  Serial.printf("  Wire.begin(addr=0x%02X, SDA=%d, SCL=%d) -> %s\n",
                I2C_ADDR, I2C_SDA, I2C_SCL, ok ? "OK" : "FEHLER!");

  // WiFi + OTA
  Serial.printf("  WiFi: verbinde mit '%s' …", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint8_t wifiTries = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTries < 30) {
    delay(500);
    Serial.print(".");
    wifiTries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf(" OK  IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println(" FEHLER (kein OTA, I²C läuft trotzdem)");
  }

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]()  { Serial.println("  [OTA] Start …"); });
  ArduinoOTA.onEnd([]()    { Serial.println("  [OTA] Fertig – Reboot."); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("  [OTA] Fehler [%u]\n", e); });
  ArduinoOTA.begin();
  Serial.printf("  OTA  : aktiv als '%s'\n\n", OTA_HOSTNAME);
  Serial.println("  I²C-Slave aktiv – warte auf Anfragen …\n");
}

// ── Loop ───────────────────────────────────────────────────────────────────────
// Erste Messung nach 5 s Startdelay (I²C-Slave hat Zeit zum Hochlaufen).
// Danach alle 3 s. requestTemperatures() blockiert NICHT (setWaitForConversion=false)
// → liest das Ergebnis der VORHERIGEN Wandlung, startet sofort die nächste.
// ESPHome liest per I²C jede Sekunde; 2 von 3 Reads treffen frische Daten.
static uint32_t nextMs = 5000UL;

void loop() {
  ArduinoOTA.handle();

  if ((int32_t)(millis() - nextMs) < 0) return;
  nextMs = millis() + UPDATE_MS;

  ds18b20.requestTemperatures();          // startet Wandlung, kehrt sofort zurück
  float t = ds18b20.getTempCByIndex(0);   // liest Ergebnis der vorherigen Wandlung

  if (t == DEVICE_DISCONNECTED_C || t == 85.0f) {
    portENTER_CRITICAL(&tempMux);
      g_valid = false;
    portEXIT_CRITICAL(&tempMux);
    Serial.println("  [ERR] Sensor nicht erreichbar");
    return;
  }

  int16_t raw = (int16_t)roundf(t * 16.0f);
  portENTER_CRITICAL(&tempMux);
    g_temp_raw = raw;
    g_valid    = true;
  portEXIT_CRITICAL(&tempMux);

  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_LED, LOW);  delay(40);
    digitalWrite(PIN_LED, HIGH); delay(80);
  }

  Serial.printf("  Temp: %+7.4f °C   raw=0x%04X\n", t, (uint16_t)raw);

  if (g_req_fired) {
    g_req_fired = false;
    if (!g_req_valid) {
      Serial.println("  [I2C] onRequest -> ERROR-Marker gesendet");
    } else {
      Serial.printf("  [I2C] onRequest -> 0x%02X 0x%02X  (%.4f °C)\n",
                    (g_req_sent >> 8) & 0xFF, g_req_sent & 0xFF, g_req_sent / 16.0f);
      digitalWrite(PIN_LED, LOW);  delay(20);
      digitalWrite(PIN_LED, HIGH);
    }
  }
}