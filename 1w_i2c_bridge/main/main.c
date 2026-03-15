/**
 * DS18B20 → I²C-Bridge  (ESP-IDF 5.x · ESP32-C3)
 * ================================================
 * Plattform  : ESP32-C3
 * Framework  : ESP-IDF 5.x  (Native IDF, kein Arduino)
 *
 * 1-Wire (RMT-Hardware, kein Bit-Banging):
 *   Data : GPIO1  (4.7 kΩ Pull-Up nach 3.3 V)
 *   VCC  : GPIO0  (OUTPUT HIGH → 3.3 V, Strapping-Pin!)
 *   GND  : GPIO2  (OUTPUT LOW)
 *
 * I²C-Slave (neue IDF-5-API, stabil):
 *   SDA  : GPIO4
 *   SCL  : GPIO3
 *   Addr : 0x48
 *
 * LED  : GPIO8  (active LOW, blinkt 2× bei gültiger Messung)
 *
 * Register-Layout (2 Byte MSB-first, LM75-kompatibel):
 *   int16_raw / 16.0  →  Auflösung 0.0625 °C
 *   25.0 °C → raw=400=0x0190 → gesendet: 0x01 0x90
 *   Sensor ungültig   → 0x80 0x00 (INT16_MIN als Error-Marker)
 *
 * Timing:
 *   • Erste Messung nach START_DELAY_MS (I²C-Master kann hochlaufen)
 *   • Danach alle MEASURE_MS; DS18B20 12-bit = ~750 ms Wandlung
 *   • TX-Puffer wird jede REFILL_MS aufgefrischt → ESPHome liest
 *     jede Sekunde und findet fast immer frische Daten
 *
 * ── ESPHome Gegenseite ────────────────────────────────────────────────────
 *
 *   i2c_device:
 *     - id: temp_bridge
 *       address: 0x48
 *       i2c_id: i2c_bus
 *
 *   sensor:
 *     - platform: template
 *       id: sensor_temp_becken
 *       update_interval: 1s
 *       lambda: |-
 *         uint8_t buf[2];
 *         auto err = id(i2c_bus)->read(0x48, buf, 2);
 *         if (err != i2c::ERROR_OK) return {};
 *         int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
 *         if (raw == (int16_t)0x8000) return {};
 *         return raw / 16.0f;
 * ─────────────────────────────────────────────────────────────────────────
 */

#include <math.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/i2c_slave.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include "esp_log.h"
#include "esp_idf_version.h"

// ── Konfiguration ─────────────────────────────────────────────────────────────
#define OW_GPIO           GPIO_NUM_1   // DS18B20 Datenleitung
#define PIN_SENSOR_VCC    GPIO_NUM_0   // Sensor-VCC  (Strapping-Pin → immer HIGH!)
#define PIN_SENSOR_GND    GPIO_NUM_2   // Sensor-GND
#define I2C_SDA           GPIO_NUM_4   // I²C Slave SDA
#define I2C_SCL           GPIO_NUM_3   // I²C Slave SCL
#define I2C_SLAVE_ADDR    0x48u        // Slave-Adresse
#define PIN_LED           GPIO_NUM_8   // Onboard-LED  (active LOW)

#define START_DELAY_MS    5000         // Wartezeit nach Boot vor erster Messung
#define MEASURE_MS        3000         // 1-Wire Messintervall
#define CONVERSION_MS      800         // DS18B20 12-bit Wandlungszeit (~750 ms)
#define REFILL_MS         1000         // I²C TX-Puffer Auffrischintervall
#define TX_BUF_DEPTH         8         // TX-Ringpuffergröße [Bytes] = 4 Lesezugriffe

#define TEMP_ERROR  INT16_MIN          // 0x8000: Sensor nicht verfügbar
// ─────────────────────────────────────────────────────────────────────────────

static const char *TAG = "bridge";

static i2c_slave_dev_handle_t s_slave;
static SemaphoreHandle_t      s_mutex;
static int16_t                s_temp_raw = TEMP_ERROR;
static bool                   s_valid    = false;

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────

static void i2c_tx_fill(void)
{
    int16_t raw;
    bool    valid;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    raw   = s_temp_raw;
    valid = s_valid;
    xSemaphoreGive(s_mutex);

    if (!valid) raw = TEMP_ERROR;

    uint8_t buf[2] = {
        (uint8_t)((raw >> 8) & 0xFF),   // MSB
        (uint8_t)( raw       & 0xFF),   // LSB
    };
    i2c_slave_write(s_slave, buf, sizeof(buf), 0);  // timeout=0 → nicht blockierend
}

static void led_blink(int n)
{
    for (int i = 0; i < n; i++) {
        gpio_set_level(PIN_LED, 0); vTaskDelay(pdMS_TO_TICKS(40));
        gpio_set_level(PIN_LED, 1); vTaskDelay(pdMS_TO_TICKS(80));
    }
}

// ── I²C TX-Auffrisch-Task ─────────────────────────────────────────────────────
static void refill_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(REFILL_MS));
        i2c_tx_fill();
    }
}

// ── Temperatur-Mess-Task ──────────────────────────────────────────────────────
static void temp_task(void *arg)
{
    onewire_bus_handle_t     bus;
    onewire_bus_config_t     ow_cfg  = { .bus_gpio_num = OW_GPIO };
    onewire_bus_rmt_config_t rmt_cfg = { .max_rx_bytes = 10 };
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&ow_cfg, &rmt_cfg, &bus));

    ds18b20_device_handle_t      ds = NULL;
    onewire_device_iter_handle_t it;
    ESP_ERROR_CHECK(onewire_new_device_iter(bus, &it));

    onewire_device_t dev;
    while (onewire_device_iter_get_next(it, &dev) == ESP_OK) {
        if (ds18b20_new_device(&dev, &ds) == ESP_OK) {
            ESP_LOGI(TAG, "DS18B20  ROM: %016llX", dev.address);
            break;
        }
    }
    onewire_del_device_iter(it);

    if (!ds) {
        ESP_LOGE(TAG, "Kein DS18B20 gefunden – Pull-Up (4.7 kΩ) prüfen!");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Startdelay %d ms …", START_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(START_DELAY_MS));

    while (1) {
        ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(ds));
        vTaskDelay(pdMS_TO_TICKS(CONVERSION_MS));

        float     temp;
        esp_err_t err = ds18b20_get_temperature(ds, &temp);

        if (err == ESP_OK && temp != 85.0f) {
            int16_t raw = (int16_t)roundf(temp * 16.0f);
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            s_temp_raw = raw;
            s_valid    = true;
            xSemaphoreGive(s_mutex);
            ESP_LOGI(TAG, "Temp: %+.4f °C   raw=0x%04X", temp, (uint16_t)raw);
            led_blink(2);
        } else {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            s_valid    = false;
            s_temp_raw = TEMP_ERROR;
            xSemaphoreGive(s_mutex);
            ESP_LOGE(TAG, "Sensorfehler (%.1f °C)", temp);
        }

        vTaskDelay(pdMS_TO_TICKS(MEASURE_MS - CONVERSION_MS));
    }
}

// ── Einstiegspunkt ────────────────────────────────────────────────────────────
void app_main(void)
{
    ESP_LOGI(TAG, "DS18B20 → I²C-Bridge  ESP-IDF %s", esp_get_idf_version());
    ESP_LOGI(TAG, "1-Wire GPIO%d  |  I²C SDA=GPIO%d SCL=GPIO%d Addr=0x%02X",
             OW_GPIO, I2C_SDA, I2C_SCL, I2C_SLAVE_ADDR);

    const gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << PIN_SENSOR_VCC) |
                        (1ULL << PIN_SENSOR_GND) |
                        (1ULL << PIN_LED),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_cfg));
    gpio_set_level(PIN_SENSOR_GND, 0);
    gpio_set_level(PIN_SENSOR_VCC, 1);
    gpio_set_level(PIN_LED, 1);

    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex);

    const i2c_slave_config_t slv_cfg = {
        .i2c_port       = I2C_NUM_0,
        .sda_io_num     = I2C_SDA,
        .scl_io_num     = I2C_SCL,
        .clk_source     = I2C_CLK_SRC_DEFAULT,
        .send_buf_depth = TX_BUF_DEPTH,
        .slave_addr     = I2C_SLAVE_ADDR,
        .addr_bit_len   = I2C_ADDR_BIT_LEN_7,
    };
    ESP_ERROR_CHECK(i2c_new_slave_device(&slv_cfg, &s_slave));
    ESP_LOGI(TAG, "I²C-Slave 0x%02X aktiv", I2C_SLAVE_ADDR);

    i2c_tx_fill();

    xTaskCreate(temp_task,   "temp",   4096, NULL, 5, NULL);
    xTaskCreate(refill_task, "refill", 2048, NULL, 4, NULL);
}
