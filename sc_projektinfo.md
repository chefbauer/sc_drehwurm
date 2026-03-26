# SC Drehwurm – Projektinfo

---

> **KI-Arbeitsanweisung:** Diese Datei ist die zentrale Projektdokumentation.
> **Nach jeder Änderung an YAML-Dateien** diese Projektinfo automatisch aktualisieren –
> ohne dass der Benutzer es explizit fordert. Relevante Daten: Pin-Belegungen,
> Plattform-/Bus-Entscheidungen, Sensor-IDs, neue Komponenten, geänderte Logik,
> offene TODOs. Änderungen im Abschnitt **Changelog** dokumentieren.
>
> **YAML-Komponenten-Übersichten:** Jede YAML-Datei hat am Anfang einen `# ── Komponenten ──`
> Kommentarblock mit allen ESPHome-Typen und ihren IDs. Dieser Kommentarblock **muss
> ebenfalls aktualisiert werden**, wenn Komponenten hinzukommen, wegfallen oder umbenannt werden.
> Gilt für alle 9 Dateien: `ble.yaml`, `cooler.yaml`, `display.yaml`, `hardware.yaml`,
> `lvgl_basis.yaml`, `lvgl_overlay.yaml`, `motorcontrol_can-bus.yaml`, `schwenker.yaml`,
> `sensorphalanx.yaml`.

---

## Was ist das hier?

**Schnellkühlanlage** auf Basis eines Kompressorkühlers.

- Eine **Umwälzpumpe** für die Kühlung des Kühlwassers
- Eine **Pumpe** für Vorratsbecken → Überlaufbecken
- **Überlaufbecken mit Dreher** per Schrittmotor

Ausgelegt für **Dosen und Flaschen** — durch das Drehen wird die Kühlleistung deutlich erhöht.

---

## Projekt-Typ

**ESPHome Standalone-Projekt** für den "Supercooler Drehwurm Kühler".

- Läuft vollständig eigenständig auf einem **ESP32-P4** mit **integriertem 7" Display**
- Kein Home Assistant erforderlich – alle Logik (Timer, Kühler-Steuerung, UI) läuft direkt auf dem Gerät
- Konfiguration in modulare YAML-Pakete aufgeteilt (via `packages:`)

`lvgl_basis.yaml` enthält Fonts, Globals, Interval, LVGL-Pages.  
`lvgl_overlay.yaml` enthält alle Top-Layer Overlays.  
`hardware.yaml` enthält Sensoren, Outputs, MCP4728, Climate, AMG8833.  
`sensorphalanx.yaml` enthält die externe Sensor-Gruppe (MLX90632, VL53L1X, SHT4x, BMP581, VEML7700).  
`schwenker.yaml` enthält Sinus-Pendel-Steuerung via F5 (MKS Servo42D CAN-Bus).  
`lights.yaml` enthält WS2812-LED-Ring außen um das Becken (Slot-Farb-Effekt, motorpositionsbasiert).  
`display_7z_settings.yaml` (umbenannt von `display.yaml`) enthält Hardware-Substitutions (Pins, CAN-IDs, Konstanten).  
**Nur in lvgl_basis/overlay generieren:** `font:`, `globals:`, `interval:`, `lvgl:`  
**Nicht generieren:** sonstiger ESPHome-Code (sensors, lights, etc.)

**Extern definiert (nicht in diesen Dateien generieren):**  
`esphome:` Grundkonfiguration, `display:`, `i2c:` Bus

---

## Hardware

| Parameter | Wert |
|---|---|
| Plattform | ESP32-P4 |
| Display-Modul | **Guition ESP32-P4 JC1060P470** |
| Display | 4,7" integriert, 1024 × 600 px |
| PSRAM | 32 MB |
| LVGL Version | 8.x (kein LVGL 9!) |
| Betrieb | Standalone (kein Home Assistant nötig) |
| Zeit-Quelle | SNTP (NTP) — RTC wird später hinzugefügt |
| Radio-Koprocessor | ESP32-C6 via `esp32_hosted` (SDIO 4-bit, extern definiert) |
| BLE | ESP32-C6 stellt BLE bereit; Scanner in `ble.yaml` |
| 1-Wire Bus | DS18B20 an ESP32-C3 I²C-Bridge (`0x48`) → `temp_bridge` in `hardware.yaml` |

---

## Becken-Temperatursensor

**Sensor:** DS18B20 via I²C-Bridge (ESP32-C3, ESP-IDF 5.x, `1w_i2c_bridge/`)

**I²C-Adresse:** `0x48` → `i2c_device: temp_bridge` in `hardware.yaml`  
**Sensor-ID:** `sensor_temp_becken` in `hardware.yaml` (Template-Sensor, `update_interval: 3s`)  
**Datenformat:** `int16_raw / 16.0` → 0,0625 °C Auflösung; Error-Marker `0x8000`

**Bridge-Firmware (`1w_i2c_bridge/main/main.c`):**

| Pin | Funktion | Wert |
|---|---|---|
| GPIO0 | Sensor-VCC | OUTPUT HIGH (3,3 V) |
| GPIO1 | 1-Wire Data | DS18B20, 4,7 kΩ Pull-Up |
| GPIO2 | Sensor-GND | OUTPUT LOW |
| GPIO3 | I²C SCL | Slave |
| GPIO4 | I²C SDA | Slave |
| GPIO8 | LED | active LOW, 2× Blinken bei gültiger Messung |

- Messintervall: 3 s (`MEASURE_MS`), 12-bit → ~800 ms Wandlung
- TX-Puffer wird alle 1 s aufgefrischt (`REFILL_MS`)
- Kein WiFi / kein OTA (reine IDF-Firmware)

**Alle sensorphalanx-Sensoren direkt am Haupt-Bus** (`i2c_bus`):
- MLX90632 (`i2c_device`, `0x3A`)
- VL53L4CD (`0x29`)
- SHT4x (`0x44`)
- BMP581 (`0x46`)
- VEML7700 (`0x10`)

| ID | Typ | Wert |
|---|---|---|
| `sensor_temp_becken` | Temperatur | Thermostat + Statusleiste |

---

## 1-Wire / Temperatursensoren

**Bus:** DS18B20 hängt am ESP32-C3 I²C-Bridge (GPIO1, RMT-Hardware).
ESPHome liest den Wert nicht direkt per 1-Wire, sondern über den I²C-Slave `temp_bridge` (0x48).

| Rolle | Sensor | Bridge-ID | ESPHome-Sensor-ID | Datei |
|---|---|---|---|---|
| Becken-Temperatur | DS18B20 (ESP32-C3, GPIO1) | `temp_bridge` (0x48) | `sensor_temp_becken` | `hardware.yaml` |

**Sensor-Einbindung (`hardware.yaml`):** Template-Sensor liest 2 Bytes von 0x48, rechnet `int16_raw / 16.0f`; Thermostat verwendet ihn direkt.

---

## Fonts

**Regel für große Fonts (Größe ≥ 60):** Glyphs auf die tatsächlich verwendeten Zeichen beschränken – vollständige Zeichensätze erzeugen hier zu große Binaries.  
Bei Änderungen von Texten mit `font_title` muss die Glyph-Liste manuell aktualisiert werden.

**Fonts < 60:** Vollständiger Standardzeichensatz (`a–z`, `A–Z`, `0–9`, Sonderzeichen) ist OK und bevorzugt.

| ID | Datei | Größe | Verwendung |
|---|---|---|---|
| `font_title` | Roboto 700 (gfonts) | 60 | Seitentitel (minimale Glyph-Liste!) |
| `font_normal` | Roboto 400 (gfonts) | 28 | Labels, Buttons, allgemein |
| `font_tab` | Roboto 400 (gfonts) | 30 | Tab-Beschriftungen |
| `font_small` | Roboto 700 (gfonts) | 18 | Beschriftungen, Overlay-Hilfstexte |
| `font_preset_num` | Noto Sans Symbols 400 (gfonts) | 28 | Eingekreiste Ziffern ①②③④⑤ (Preset-Labels) |
| `font_icons` | Font Awesome Solid 6.5.0 (CDN) | 40 | Icons: U+F773 (Water), U+F021 (arrows-rotate), U+F011 (Power), U+F013 (Gear), U+F186 (Mond/Standby) … |
| `font_timer` | 5x7-dot-matrix.ttf (lokal) | 35 | Timer-Anzeige MM:SS |
| `font_clock` | Roboto 400 (gfonts) | 22 | Uhrzeit HH:MM:SS in Statusleiste |
| `font_arc_val` | Roboto 400 (gfonts) | 56 | Arc-Wert-Labels (Glyphs: `0–9 . s % `) |
| `font_icon_xl` | Font Awesome Solid (CDN) | 120 | Nur U+F021 – aktuell nicht mehr in Overlays verwendet |
| `font_icon_ts` | Font Awesome Solid (CDN) | 80 | IR-Overlay Fadenkreuz-Icon (U+F05B) – aktuell nicht im Overlay angezeigt |
| `font_ts_xl` | Roboto Bold 700 (gfonts) | 120 | IR-Temperaturwert `overlay_temp_messung` (Glyphs: `0123456789.,-°C `) |

**Sonderzeichen in Glyphs** (Text-Fonts): `äöüÄÖÜß°·–%`  
`font_preset_num` enthält **nur** `①②③④⑤` (Roboto hat diese Zeichen nicht → Noto Sans Symbols)  
Icon-Bytes in Lambdas: Play = `\xef\x81\x8b`, Pause = `\xef\x81\x8c`, Schneeflocke = `\xef\x8b\x9c`

---

## Globale Variablen

Für 6 Timer-Slots (Index 0–5):

| ID | Typ | Bedeutung |
|---|---|---|
| `slot_start_ms` | `std::array<uint32_t, 6>` | millis() beim letzten Start |
| `slot_elapsed_ms` | `std::array<uint32_t, 6>` | akkumulierte Zeit in ms |
| `slot_status` | `std::array<int, 6>` | 0=gestoppt, 1=läuft, 2=pausiert |
| `amg_pixel_temps` | `std::array<float, 64>` | AMG8833 8×8 Pixel-Temperaturen |
| `global_leds_per_slot` | int | Anzahl LEDs pro Slot im WS2812-Ring (Tab Licht) |
| `global_lvgl_is_idle` | bool | LVGL-Idle-Flag — per `interval: 10s` via `lvgl.is_idle` gesetzt |
| `global_emissivity` | float (0.9) | IR-Emissivität — Laufzeitwert, kein NVS-Save |
| `global_mat_type` | int (0) | Materialtyp: 0=Glas, 1=Dose matt, 2=Dose glänzend |
| `global_mat_color` | int (3) | Farbindex 0–5 (Farb-Swatch im Emissivitäts-Selector) |

**Interval:** 500 ms → aktualisiert alle 6 Timer-Labels wenn Status = 1.  
Aktualisiert auch das AMG8833-Overlay wenn es sichtbar ist.  
**Interval:** 10 s → prüft `lvgl.is_idle: timeout: ${c_standby_lvgl_idle}` → setzt `global_lvgl_is_idle`

---

## Substitutionen (`display_7z_settings.yaml`)

| Substitution | Wert | Bedeutung |
|---|---|---|
| `c_tof_update_interval` | 500ms | TOF-Sensor Update-Interval (Normal) |
| `c_tof_update_interval_fast` | 200ms | TOF-Sensor Update-Interval (Overlay aktiv) |
| `c_ir_update_interval` | 1s | MLX90632 Update-Interval (Normal) |
| `c_ir_update_interval_fast` | 200ms | MLX90632 Update-Interval (Overlay aktiv) |
| `c_standby_delay` | 300s | Verzögerung bis Standby aktiv (bin_standby `delayed_on`) |
| `c_standby_lvgl_idle` | 120s | LVGL-Idle-Timeout für Standby-Flag |
| `c_standby_tof_min_mm` | "800" | Mindestdistanz für Standby (kein Objekt vor Sensor) |

---

## Page 1: Hauptseite (`page_main`)

### Titel
- Text: "SC Schwippschwenker"
- Font: `font_title`, Farbe: `#003366`, oben mittig, y=20

### System-Notschalter (`btn_system_power`)
- 80×80 px, rund (radius=40), oben rechts (x=-10, y=10)
- Rot = System AUS, Grün = System EIN
- Icon: U+F011 (Power, `font_icons`, weiß)
- **Dient gleichzeitig als Notschalter!**
  - **AUS: sofort bei einfachem Druck** (`on_press`) — stoppt Schwenker, Thermostat, Pumpen sofort
  - **EIN: nur per Long Press** (`on_long_press`) — verhindert versehentliches Einschalten
- AUS löst `script_system_aus` aus: Schwenker stopp + Thermostat OFF + DAC-Pumpen aus
- **TODO: durch echtes PNG-Bild ersetzen**
- Position: zentriert, y=20, 360×360 px
- Aufbau aus LVGL `obj`-Widgets (Zylinder-Illusion):
  - `cyl_rim`: oben, grau, oval (Öffnung)
  - `cyl_body`: Mantelrechteck, grau
  - `water_surface`: hellblaue Ellipse (Wasseroberfläche)
  - `water_body`: blaues Rechteck (Wasserkörper)
  - `cyl_bottom`: dunkelgraue Ellipse (Boden)

### 6 Farbslots (Timer-Buttons)

Anordnung im Uhrzeigersinn nach Farbrad:

| Slot | Farbe | Seite | y-Position | Index |
|---|---|---|---|---|
| 1 | Rot `#FF0000` | links | 110 | 0 |
| 2 | Gelb `#FFFF00` | links | 250 | 1 |
| 3 | Grün `#00FF00` | links | 390 | 2 |
| 4 | Cyan `#00FFFF` | rechts | 110 | 3 |
| 5 | Blau `#0000FF` | rechts | 250 | 4 |
| 6 | Magenta `#FF00FF` | rechts | 390 | 5 |

**Jeder Slot ist ein `button`** (290×120 px, radius=16, clip_corner=true, bg_opa=TRANSP):

- **Linker Tab** (60×100%, `clickable: true`): volle Slot-Farbe, Slot-Nummer zentriert  
  `on_click` → `script_schwenker_goto_slot->execute(N)` (N=1–6, Zielwinkel = (N-1)×60°)
- **Rechter Bereich** (230×100%, `clickable: false`): 50% Opacity, enthält:
  - Label "Timer:" oben links
  - Zeitanzeige `MM:SS` darunter (`font_timer`)
  - Play/Pause-Icon rechtsbündig (x=-17)

**Touch-Verhalten:**
- `on_short_click`: Start (Status 0→1) / Pause (Status 1→2) Toggle
- `on_long_press`: Reset — nur wenn Status=2 (pausiert) → Status=0, Anzeige=00:00

> `on_short_click` feuert bei Long Press **nicht** → kein Extra-Flag nötig

**Textfarben** (auf 50%-Hintergrund):
| Slot | Textfarbe |
|---|---|
| 1 R | `#880000` |
| 2 Y | `#666600` |
| 3 G | `#004400` |
| 4 C | `#004444` |
| 5 B | `#FFFFFF` |
| 6 M | `#880044` |

### Statusleiste
- `status_bar` unten, 1024×60 px (60 px Höhe), `#F0F0F0`
- **Links:** Uhrzeitanzeige `lbl_status_clock` (`font_clock`, `#333333`) — zeigt `HH:MM:SS` aus SNTP
- **Mitte:** Schneeflocken-Icon (`lbl_kompressor_icon`): grau = aus, blau = kühlt aktiv
- **Mitte-Rechts:** Becken-Temperatur (`lbl_temp_becken`, `font_clock`, Farbe `#2299DD` hellblau-cyan), x+80 vom Center, Beispiel: `12.3°C`
- **Rechts (vor Zahnrad):** `lbl_standby_icon` — Mond-Icon (U+F186, `font_icons`, `#8899BB` blau-grau, x=-72, `hidden: true`) — sichtbar wenn `bin_standby` aktiv
- **Rechts:** Button `btn_to_settings` (60×54 px, dunkelgrau `#444444`) mit Zahnrad-Icon (`\uF013`) → `lvgl.page.show: page_settings`
- Farbe des Schneeflocken-Icons wird via `climate.on_state` Lambda gesetzt

### Navigation
- Settings-Button ist Teil der Statusleiste (rechts)

---

## Page 2: Einstellungen (`page_settings`)

### Titel
- Text: "Einstellungen", Font: `font_title`, Farbe: `#003366`, oben mittig

### TabView (4 Tabs, `font_tab`, Hintergrund `#E0E0E0`)

Tab-Reihenfolge: **System · Schwenker · Licht · Bildschirm · Kühler · Test**

---

**Tab "System":**
- `row_overlays`: Label "Overlays:" + drei Buttons nebeneinander:
  - `btn_amg_live` (dunkelblau `#334466`) → öffnet `overlay_amg8833`
  - `btn_sensorphalanx_live` (grün `#336644`) → öffnet `overlay_sensorphalanx`
  - `btn_temp_messung_live` → öffnet `overlay_temp_messung`

---

**Tab "Schwenker":**
- Zeile: Label "Motor freigeben" + Button → Motor Mode 3 (aktiv, kein Haltestrom)
- Zeile: Label "Motor 0° setzen" + Button → `script_motor_set_zero`
- Zeile: Label "Motorstrom" + Slider `slider_motor_strom` (500–2000) → `sw_work_current_mA` (NVS-persistent)
- Zeile: Overlay-Button → `overlay_schwenker_settings` öffnen

---

**Tab "Licht":**
- Zeile: Label "Slot-Breite" + Slider `slider_slot_breite` (1–28 LEDs) + Wert-Label `lbl_slot_breite_val`
  - `on_value` → `global_leds_per_slot` setzen → WS2812-Ring-Effekt aktualisiert Slot-Breite motorpositionsbasiert

---

**Tab "Bildschirm":**
- Zeile: Label "Helligkeit" + Slider (`slider_helligkeit`, 0–100, Standard 80)
  - `on_value` → `light.control` auf `light_screen_background` mit `brightness: x/100.0`
  - Bei manueller Bedienung: Auto-Modus deaktivieren + Switch zurücksetzen
- Zeile: Switch `sw_brightness_auto` + Label "Auto (VEML7700)"
  - Ein: VEML7700-Lux steuert Helligkeit automatisch (Formel: 0 lux→ 50%, 100 lux → 100%)
  - Aus: manuelle Steuerung
- Farbtest-Quadrate (150×150 px): Rot / Grün / Blau nebeneinander zentriert

---

**Tab "Kühler":**
- Zeile: Label "Kühlung" + Switch `sw_kuehlung`
  - `on_value` → `climate_thermostat` Mode COOL / OFF
- Zeile: Label "Temperatur" + Slider `slider_temp` (0–50, entspricht 0.0–5.0 °C) + Wert-Label
  - `on_value` → `call.set_target_temperature(x / 10.0f)` + Label update
- Preset-Buttons: **1°C** / **1.5°C** / **5°C** (setzt Slider + Label + Climate)

---

**Tab "Test":**
- Zeile: Label "Turmpumpe" + Slider `slider_turmpumpe` (0–100)
  - `on_value` → `output.set_level: output_pumpe_dacA`
- Zeile: Label "Beckenpumpe" + Slider `slider_beckenpumpe` (0–100)
  - `on_value` → `output.set_level: output_pumpe_pwm1`

---

### Navigation
- Button "Zurück" unten links, `#444444` → `lvgl.page.show: page_main`

---

## Overlays (`lvgl_overlay.yaml`, `top_layer`)

### `overlay_schwenker_settings` — Schwenker-Einstellungen
- Vollflächig (100%×100%), weiß, `bg_opa: 97%`, initial `hidden: true`
- **Start/Stop-Button** `btn_schwenker_overlay_toggle` (TOP_LEFT 80×55, grau, U+F021): `on_press` → Schwenker start/stop
- **Exit-Button** `btn_schwenker_settings_exit` (TOP_RIGHT 80×55, rot `#CC3333`)
- **3 Arcs** (y=145, je 320×340 px), nebeneinander:
  - `arc_schwenkzeit` (blau, min=10 max=200, Einheit ×100ms → 1–20 s) → `lbl_arc_zeit_val`
  - `arc_acc` (orange, min=10 max=100, Einheit %) → `lbl_arc_acc_val` + `lbl_trapez_shape` (ASCII: `/\` bis `/___\`)
  - `arc_rpm` (grün, min=1 max=100) → `lbl_arc_rpm_val`
- **Trapez-Form ASCII** (kein Unicode – nicht in Roboto): `a≥80` → `/\`, `a≥55` → `/_\`, `a≥35` → `/___\`, sonst → `/_____\`
- **5 Preset-Buttons** `btn_preset[1..5]` (178×80 px, je ①–⑤ via `font_preset_num`):
  - `on_short_click`: lädt hp_ms + rpm + acc, setzt alle 3 Arcs + Labels
  - `on_long_press`: speichert aktuelle Globals in preset[N]_*, aktualisiert `lbl_preset[N]_vals`
- **Öffnen:** `script_schwenker_settings_open` (zentral, von Long-Press Hauptbutton + Einstellungen-Tab)
- **Start/Stop Hauptseite:** `btn_schwenker_main` → `on_short_click` (Long-Press öffnet nur Overlay, startet/stoppt **nicht**)

### `overlay_amg8833` — AMG8833 Live-Ansicht- Vollflächig (100%×100%), weiß, `bg_opa: 90%`, initial `hidden: true`
- `amg_grid_container` (496×496 px, leicht links): 8×8 Pixel-Zellen via `amg_create_grid()`
- Exit-Button `btn_amg_exit` (rot `#CC3333`, 80×80) rechts → `lvgl.widget.hide: overlay_amg8833`
- Daten werden per 500ms-Interval aus `amg_pixel_temps` aktualisiert

### `overlay_sensorphalanx` — Sensor-Phalanx- Vollflächig (100%×100%), weiß, `bg_opa: 93%`, initial `hidden: true`
- Titel "Sensor-Phalanx" (`font_title`, `#003366`)
- Exit-Button `btn_sensorphalanx_exit` (rot, oben rechts)
- 7 Sensor-Zeilen (zebra-gestreift), je 860×65 px mit farbigem Badge + Name + Beschreibung + Live-Wert:

| Badge | Farbe | Sensor | Beschreibung | Wert-Label |
|---|---|---|---|---|
| ToF | `#2255CC` | VL53L4CD | Time-of-Flight · 50–4000 mm | `distance_value_label` |
| T° | `#CC5500` | SHT4x | Umgebungstemperatur | `temp_value_label` |
| RH | `#008899` | SHT4x | Relative Luftfeuchte | `hum_value_label` |
| hPa | `#7744BB` | BMP581 | Barometrischer Druck | `pressure_value_label` |
| ALT | `#227733` | BMP581 | Höhe (hypsometrisch) | `height_value_label` |
| LUX | `#BB8800` | VEML7700 | Umgebungslicht | `light_value_label` |
| IR° | `#CC2200` | MLX90632 | IR Objekttemperatur | `mlx_value_label` |

---

### `overlay_temp_messung` — IR-Temperatur & TOF-Visualisierung

- Vollflächig (1024×600 px), `bg_color: #0D1117`, initial `hidden: true`
- Öffnet automatisch per `bin_objekt_vor_sensor` (`on_press`) wenn Objekt < Schwellwert erkannt
- Schließt automatisch: `bin_objekt_vor_sensor` hat `filters: delayed_off: 3s` → `on_release` → `script_close_temp_overlay`
- **`script_close_temp_overlay`** (mode: single): `lvgl.widget.hide: overlay_temp_messung`
- **Physik FOV** MLX90632: 50° → Halbwinkel 25° → `tan(25°)=0.46631` → `d_px = 2 × dist_mm × 0.46631 × 6.674`

**Z-Order (Deklarationsreihenfolge):**

1. `btn_temp_messung_exit` (80×60 px, TOP_RIGHT x:-10 y:10, `#CC3333`) → `script_close_temp_overlay`
2. `obj_dist_bar_container` (120×440 px, LEFT_MID x:40):
   - Labels: "100mm" oben (TOP_MID y:8), "45mm" CENTER y:-56 (grün = Sweet Spot), `lbl_dist_messung` BOTTOM_MID y:-4, "10mm" BOTTOM_MID y:-28
   - Optimal-Linie (120×3 px, CENTER y:-44, grün `#44AA44`)
   - `bar_distanz` (60×400 px, CENTER, min:10 max:100, init value:60) — **gespiegelter Wert**: Lambda `110 - d` (kein `inverted:`!)
3. `obj_ir_circle` (init 280×280 px, CENTER x:0 y:0, TRANSP bg, roter Rand 5px, radius:140):
   - `obj_cross_h` (init 280×5 px, CENTER, rot) — Fadenkreuz horizontal
   - `obj_cross_v` (init 5×280 px, CENTER, rot) — Fadenkreuz vertikal
   - Skaliert dynamisch per TOF via `lv_obj_set_size` + `lv_obj_set_style_radius` + `lv_obj_align`
4. `lbl_ts_temp` (CENTER x:0 y:0, `font_ts_xl`, weiß, `clickable: true`) → `script_close_temp_overlay`
   - Format: `%d,%d °C` mit manueller Vor/Nachkomma-Trennung (Komma statt Punkt)
5. `obj_emissivity_container` (200×440 px, RIGHT_MID x:-20, `#1A1E2C`):
   - Titel + `lbl_emissivity_val` (zeigt aktuellen ε-Wert)
   - 3 farbige Range-Balken: Glas (grün), Dose matt (orange), Alu (blau) mit Wertebereichen als Tick-Labels
   - `slider_emissivity` (28×340 px, LEFT_MID x:22, Bereich 1–100, init 90) → setzt `global_emissivity`
6. `obj_material_selector` (860×64 px, BOTTOM_MID y:-6, `#12162A`):
   - `btn_mat_glas` (x:44, `#226644`), `btn_mat_dose_matt` (x:138), `btn_mat_dose_glaenzend` (x:232)
   - 6 Farb-Swatches `btn_col_0`–`btn_col_5` (je 50×50 px, radius:25, x:383–673)
   - Emissivitätstabelle 3×6 in Lambdas → setzt `global_emissivity` + `global_mat_type` + `global_mat_color` + Slider + Label

**XML-Layout-Export:** `ui/overlay_temp_messung.xml` — für viewer.lvgl.io (pixelgenaues Layout-Editing)

### MCP4728 DAC — Turmpumpe
- `output_pumpe_dacA`, Kanal A, `vref: vdd`
- `power_down: normal`, `min_power: 0.2`, `max_power: 1.0`
- `zero_means_zero: true` → Slider 0 → exakt 0 V (kein Nachlaufen)
- Pumpe reagiert linear auf 1–3,5 V; unter 1 V aus

### Outputs
| ID | Typ | Pin | Zweck |
|---|---|---|---|
| `output_pumpe_dacA` | MCP4728 Kanal A | — | Turmpumpe (0–3,5 V) |
| `output_luefter_kompressor_pwm` | LEDC 25 kHz | `pin_pwm3` | Kompressor-Lüfter |
| `output_pumpe_pwm1` | LEDC 80 kHz | `pin_pwm1` | Beckenpumpe |
| `output_kompressor_relais` | GPIO | `pin_pwm4` | Kompressor Ein/Aus |

### I²C Devices
| ID | Adresse | Beschreibung |
|---|---|---|
| `i2c_amg8833` | 0x69 | AMG8833 8×8 IR-Matrix |

### Climate — `climate_thermostat`
- Platform: `thermostat`, Sensor: `sensor_temp_becken`
- Kühlen: Kompressor-Relais + Lüfter max + Beckenpumpe max
- Idle: Relais aus + Lüfter min + Beckenpumpe aus
- Presets: Eco (5 °C), Normal (1,5 °C)
- `on_control`: im OFF-Modus Beckenpumpe aus, sonst Standby-Level

---

## Sensor-Phalanx (`sensorphalanx.yaml`)

Alle Sensoren auf `i2c_id: i2c_bus` (fremdkonfiguriert in main_config).

| Sensor | Platform | Adresse | Beschreibung |
|---|---|---|---|
| VL53L4CD | `vl53l4cd` | 0x29 | Distanz, Short Mode, `${c_tof_update_interval}` |
| SHT4x | `sht4x` | auto | Temperatur + Luftfeuchtigkeit, 1s |
| BMP581 | `bmp581_i2c` | auto | Druck + Temperatur, 1s |
| `indoor_altitude` | template | — | Höhe aus BMP581, hypsometrisch |
| VEML7700 | `veml7700` | 0x10 | Umgebungslicht, 2s |
| `sensor_Temp_OBJECT` | template | — | MLX90632 IR-Berechnung (ε = `global_emissivity`), `${c_ir_update_interval}` |
| `i2c_mlx90632` | i2c_device | 0x3A | I²C-Device für MLX90632 |
| `bin_objekt_vor_sensor` | binary_sensor (template) | — | TOF < Schwellwert → öffnet/schließt `overlay_temp_messung` via `delayed_off: 3s` |
| `bin_standby` | binary_sensor (template) | — | TOF > `c_standby_tof_min_mm` UND `global_lvgl_is_idle` → zeigt `lbl_standby_icon` |
| interval 10s | — | — | Setzt `global_lvgl_is_idle` via `lvgl.is_idle: timeout: ${c_standby_lvgl_idle}` |
| `script_close_temp_overlay` | script (single) | — | `lvgl.widget.hide: overlay_temp_messung` |

---

## LVGL-Update-Konvention

**Labels werden per `lvgl.widget.refresh` aktualisiert**, nicht per `lvgl.label.update`.

- Der Sensor-Trigger enthält nur `- lvgl.widget.refresh: <label_id>`
- Das zugehörige Label-Widget in `lvgl_basis.yaml` / `lvgl_overlay.yaml` trägt das `text: !lambda`-Statement direkt
- So bleibt die Anzeigelogik zentral im LVGL-Block, der Sensor-Code bleibt schlank
- Ausnahme: Overlays / einmalige Aktionen, wo kein persistentes Widget existiert

---

## Bekannte LVGL-8-Einschränkungen

| Problem | Lösung |
|---|---|
| `transform_scale_y` nicht verfügbar (LVGL9) | nicht verwenden |
| `bg_opa` nur `%`-Suffix oder Keywords | `50%`, `COVER`, `TRANSP` |
| ESPHome setzt `LV_OBJ_FLAG_CLICKABLE` auf alle `obj` | `clickable: false` direkt im YAML setzen |
| `obj` empfängt keinen `on_click` zuverlässig | Widget-Typ `button` verwenden |
| `#` in C++ Lambda ist Präprozessor-Direktiv | immer `//` für Kommentare in lambdas |
| Sonderzeichen (→, ·, –) in Lambdas | nur in `ESP_LOGX`-Strings erlaubt, nicht in Rückgabewerten |

---

## Status

- [x] Font-Definitionen (title, normal, tab, small, icons, timer)
- [x] Timer-Globals (6 Slots) + AMG-Pixel-Global
- [x] Interval 500ms (alle 6 Slots + AMG-Overlay-Update)
- [x] Hauptseite Titel + Tank-Widget + 6 Farbslots
- [x] Statusleiste mit Kompressor-Icon (dynamische Farbe)
- [x] `lbl_standby_icon` (Mond U+F186, `font_icons`, blau-grau) in Statusleiste — sichtbar bei `bin_standby`
- [x] Einstellungen: Tab-Reihenfolge System · **Schwenker** · **Licht** · Bildschirm · Kühler · Test
- [x] Tab System: Overlay-Buttons (AMG8833 Live, Sensor-Phalanx, IR-Temp-Messung)
- [x] Tab Schwenker: "Motor freigeben" (Mode 3) + "Motor 0° setzen" (script_motor_set_zero) + Motorstrom-Slider + Overlay-Button
- [x] Tab Licht: `slider_slot_breite` (1–28 LEDs) → `global_leds_per_slot`
- [x] Tab Bildschirm: Helligkeit-Slider + Farbtest-Quadrate
- [x] Tab Kühler: Kühlung-Switch + Temperatur-Slider + Preset-Buttons
- [x] Tab Test: Turmpumpe-Slider + Beckenpumpe-Slider
- [x] Overlay AMG8833 Live (8×8 Grid, Exit-Button)
- [x] Overlay Sensor-Phalanx (7 Sensoren, Badge-Design)
- [x] Overlay `overlay_temp_messung`: TOF-Bar, FOV-Kreis, Temperaturanzeige (Komma-Format), Emissivitäts-Slider, Material/Farb-Auswahl
- [x] `bin_objekt_vor_sensor` + `bin_standby` in `sensorphalanx.yaml`
- [x] TOF-Bar gespiegelt (`110 - d`) — kein ungültiges `inverted:`
- [x] FOV-Kreis dynamisch skaliert via TOF (MLX90632 50° FOV, tan(25°)=0.46631, 6.674 px/mm)
- [x] Emissivitäts-System: `global_emissivity` + Slider + 3×6 Material/Farb-Auswahl
- [x] `sensor_Temp_OBJECT` liest `global_emissivity` statt hardcoded 0.90
- [x] XML-Export `ui/overlay_temp_messung.xml`
- [x] hardware.yaml: MCP4728 DAC, Lüfter, Pumpen, Relais, AMG8833
- [x] hardware.yaml: Climate Thermostat mit Kühler-Steuerung
- [x] sensorphalanx.yaml: VL53L1X, SHT4x, BMP581, VEML7700, MLX90632
- [x] `zero_means_zero: true` für Turmpumpe (kein Nachlaufen)
- [x] `lights.yaml`: WS2812-Ring (80 LEDs, `pin_led_h1`) mit Slot-Farb-Effekt (motorpositionsbasiert)
- [x] Standby-System: `bin_standby` (TOF + LVGL-Idle), `global_lvgl_is_idle`, Mond-Icon in Statusleiste
- [x] Substitutionen c_standby_delay / c_standby_lvgl_idle / c_standby_tof_min_mm / c_tof*/c_ir* in `display_7z_settings.yaml`
- [ ] Tank-Platzhalter durch echtes PNG ersetzen
- [x] `sensor_temp_becken` via DS18B20 I²C-Bridge (`1w_i2c_bridge`, ESP-IDF 5.x) aktiv
- [x] `schwenker.yaml`: Sinus-Pendel via F5 + "fernes Ziel" (MKS Servo42D)
- [x] `script_schwenker_goto_slot(slot)`: fährt Motor zu Slot-Position (Slot 1=0°…Slot 6=300°)
- [x] `on_boot priority:-200`: Motor fährt auf 0° (script_motor_set_zero)
- [x] `schwenker_ring` Hintergrundfarbe hellblau (#2299CC)
- [x] Slot-Tabs: 60px breit (war 50px), anklickbar → Schwenker-Navigation
- [x] `sw_work_current_mA` (NVS persistent): Strom wird beim Start und bei goto_slot geladen
- [x] Motorstrom-Slider (`slider_motor_strom`): speichert bei Release in NVS, on_boot sync
- [x] Motor Idle-Timeout: 10 s ohne Bewegung → FOC-Modus + 500 mA
- [x] `sw_motor_busy` + `sw_stop_pending`: verhindert Timeout während Bewegung bzw. Slot-Fahrt
- [x] Sanfter Stop: Halbzyklus wird fertig gefahren bevor Motor stoppt
- [x] Slot-Marker (ring_slot1-6_marker): `obj` → `button` + on_click → goto_slot(N)
- [x] goto_slot: stoppt Schwenker geordnet (F5+800ms) bevor Positionsfahrt
- [x] Wochentag-Fix: `t.day_of_week - 1` (war 1-basiert, Array 0-basiert)
- [x] arc_rpm: max_value 100, default 15 RPM (kein Upper-Clamp mehr)

---

## Schwenker (`schwenker.yaml`)

### Prinzip: F5 + "Fernes Ziel" + Sinus-Hüllkurve

> **⚠️ Wasserwiderstand / Strombegrenzung**
> Der Motor dreht im **gefüllten Wasserbecken** — das Wasser erzeugt erheblichen mechanischen Widerstand.
> - Der Motor kämpft bei jedem Richtungswechsel gegen Traegheitsmasse + Strömungswiderstand an.
> - Die **Arbeitsstromgrenze von 2000 mA** (0x83, 0x07, 0xD0) ist eine bewusste Schutzgrenze:
>   höhere Ströme würden den Motor überlasten und bei Blockade wäre ein Motorschaden möglich.
> - Geschwindigkeit und Beschleunigung müssen **wasserangepasst** klein bleiben:
>   - **Schwenken:** `sw_max_speed_rpm = 15`, `sw_acc = 200` (Sinus glättet Richtungswechsel)
>   - **Positionsfahrt:** `speed = 5 RPM`, `acc = 50` (langsam und weich im Wasser)
> - Eine Blockierung durch übermäßige Last → Motor bleibt stehen, 2 A Limit verhindert Schlimmeres.
>   Die Motortemperatur und das Verhalten im Wasser müssen bei der Inbetriebnahme beobachtet werden.

| Global-ID | Typ | Default | Bedeutung |
|---|---|---|---|
| `sw_aktiv` | bool | false | Schwenker läuft |
| `sw_richtung` | int | +1 | +1 = CW, -1 = CCW |
| `sw_phase_ms` | uint32_t | 0 | Fortschritt in aktueller Halbperiode |
| `sw_halbperiode_ms` | uint32_t | 3000 | Zeit pro Richtung (ms) |
| `sw_max_speed_rpm` | int | 100 | Spitzengeschwindigkeit |
| `sw_acc` | int | 100 | **10–100 %** = Rampenanteil der Halbperiode (acc+dec zusammen); 100 % = purer Sinus, 50 % = 25 % rauf + 50 % Plateau + 25 % runter |
| `sw_motor_ramp` | int | 20 | Motor-internes acc-Byte (1–254) für 50ms-Stufenglättung (NVS persistent) |
| `sw_work_current_mA` | int | 1000 | Arbeitsstrom in mA (NVS persistent, 500–2000) |
| `sw_motor_last_move_ms` | uint32_t | 0 | millis() beim letzten Stopp (0 = inaktiv) |
| `sw_motor_busy` | bool | false | Blockiert Idle-Timeout während Bewegung |
| `sw_stop_pending` | bool | false | Sanfter Stop angefordert – wartet auf Zyklusende |
| `preset[1..5]_hp_ms` | uint32_t | — | Gespeicherte Halbperiode je Preset |
| `preset[1..5]_rpm` | int | — | Gespeicherte RPM je Preset |
| `preset[1..5]_acc` | int | 100 | Gespeicherter Acc-% je Preset |

**Takt (50ms):**
- `speed = max_rpm * sin(π * phase_ms / T_half)` → 0→max→0 pro Halbperiode
- F5-Paket (verifiziert): `{ 0xF5, spd_H, spd_L, acc, pos_HH, pos_H, pos_L }`, absAxis = ±8.000.000
- Motor erreicht das Ziel nie (bei 15 RPM macht er ~200 Schritte/50ms vs. 8.000.000 Offset)
- Am Periodenende: `sw_richtung *= -1`, Phase zurück auf 0

**Scripts:**

| Script | Beschreibung |
|---|---|
| `script_schwenker_settings_open` | **Zentrale Sync-Funktion**: setzt alle 3 Arcs + Value-Labels + lbl_trapez_shape auf aktuelle Globals → öffnet overlay_schwenker_settings |
| `script_schwenker_start` | Motor-Init (Closed Loop 0x04 hardcoded, 64 Steps, `sw_work_current_mA`, idle min) → Sinus starten |
| `script_schwenker_stop` | Setzt `sw_stop_pending=true` + Buttons orange → Stop erfolgt am nächsten Richtungswechsel |
| `script_system_ein` | system_ein=true → Power-Button grün → Thermostat COOL |
| `script_system_aus` | system_ein=false → Power-Button rot → Schwenker stopp → Thermostat OFF → Pumpen aus |
| `script_schwenker_goto_slot` | Stoppt Schwenker geordnet (F5 speed=0 + 800ms), dann fährt Motor zu Slot-Position |

**Motor Idle-Timeout (50ms-Interval):**
- Nach jedem Stopp: `sw_motor_last_move_ms = millis()`
- 10 s ohne Bewegung (wenn !sw_aktiv && !sw_motor_busy): → Mode 5 (FOC) + Strom auf 500 mA
- `sw_motor_busy` wird während goto_slot und trim_slider aktiv gesetzt

**Sanfter Stop (`sw_stop_pending`):**
- Stop-Button setzt `sw_stop_pending=true`, Buttons orange
- 50ms-Interval prüft bei Richtungswechsel: falls true → F5 speed=0 + Buttons grau + `sw_aktiv=false`
- Verhindert abruptes Stoppen mitten im Halbzyklus

**Slot-Positionsnavigation (`script_schwenker_goto_slot`):**
- Parameter: `slot` (int, 1–6)
- Zielwinkel: `(slot - 1) × 60°` → Slot 1=0°, Slot 2=60°, …, Slot 6=300°
- Liest `sensor_motor_position` (14-bit, 0–16383 = 0–360°), berechnet kürzesten Relativweg (±180°)
- Ruft `script_motor_goto_relative_degree(5 RPM, 50 acc, rel_deg)` auf (wasserwiderstandsangepasst)
- Motor-Init vor Fahrt: Mode 4, 64 Steps, 2000 mA, idle min (wie Schwenken)
- Nach Fahrt: 7 s Wartezeit, dann Mode 5 (FOC) → kein Haltestrom

**Button:** `btn_schwenker_toggle` (Start/Stop Toggle)

---

## Update-Log

| Datum / Zeit (UTC) | Commit | Änderung |
|---|---|---|
| 2026-03-07 07:52 | `b6e12e6` | Initial commit – Projektstruktur angelegt |
| 2026-03-07 08:05 | `d600b37` | Projektstart: Grundstruktur lvgl_basis.yaml |
| 2026-03-07 08:12 | `5b73e94` | Helligkeit-Slider in Einstellungen |
| 2026-03-17 (session) | — | `schwenker.yaml` angelegt: Sinus-Pendel via F5, Globals, 50ms-Interval, Start/Stop-Button |
| 2026-03-07 08:23–09:25 | `cade272–65a0d82` | Iterative Aufbauphase: Display-Grundkonfiguration, Fonts, Seitenstruktur |
| 2026-03-07 10:32 | `3b1def5` | Hintergrundfarben angepasst |
| 2026-03-07 10:59–11:05 | `3ff4856–e01e12f` | Becken-Tank-Widget (Zylinder-Illusion) |
| 2026-03-07 11:27 | `b0f6ad7` | Ellipsen-Geometrie für Zylinder-Deckel/-Boden |
| 2026-03-07 11:33 | `7050ca4` | 6 Farbslots angelegt |
| 2026-03-07 11:42–12:00 | `fd17b5b–7bd9971` | Slot-Layout iterativ verfeinert (Split-Design, Farben) |
| 2026-03-07 12:14 | `c1e5269` | Timer-Logik (Start/Pause/Reset) implementiert |
| 2026-03-07 12:19–12:35 | `c0692d7–40efc98` | Timer-Anzeige, Icons, Interval |
| 2026-03-07 12:49 | `f23bd14` | Alle 6 Slots vollständig (6x!) |
| 2026-03-07 13:00–13:01 | `535bfeb–c4c0bd3` | Font-Definitionen erweitert (font_small, font_timer) |
| 2026-03-07 13:06 | `3263841` | Design-Feinschliff (Farben, Abstände) |
| 2026-03-10 11:00 | `d9832c3` | Turmpumpe-Slider im Test-Tab |
| 2026-03-10 11:43 | `39bda70` | Kompressor-Output + Relais-Konfiguration |
| 2026-03-10 11:52 | `0e0aaca` | Climate Thermostat: cool_action / idle_action |
| 2026-03-10 11:54 | `5874ade` | Beckenpumpe-Output (LEDC) hinzugefügt |
| 2026-03-10 11:57 | `790df74` | Kühler-Tab: Kühlung-Switch |
| 2026-03-10 12:08 | `0ed51ef` | Bugfix: Kühler-Tab Fehler behoben |
| 2026-03-10 12:46 | `339db75` | AMG8833 I²C-Device + Sensor-Lambda (8×8 Matrix) |
| 2026-03-10 12:55 | `2f2c464` | AMG8833 als Package included |
| 2026-03-10 13:00 | `7730f00` | I²C-Bus-ID auf `i2c_bus` vereinheitlicht |
| 2026-03-10 13:02 | `006ecac` | AMG8833 Overlay (amg_create_grid, Live-Ansicht) |
| 2026-03-10 13:59 | `5c9021b` | Einstellungen: Optik-Update, Tab-Reihenfolge System→Bildschirm, Overlays-Zeile im Test-Tab |
| 2026-03-10 14:15 | `a7a8ddf` | Sensor-Phalanx: sensorphalanx.yaml + overlay_sensorphalanx |
| 2026-03-10 14:15 | `59d9f5b` | Sensor-Phalanx Button in Overlays-Zeile eingefügt |
| 2026-03-10 14:20 | `9640e24` | Bugfix: sensorphalanx.yaml `#` → `//` in Lambda-Kommentar |
| 2026-03-10 14:26 | `d1a6640` | MCP4728 `power_down: gnd_1k` → `normal` (Pumpe lief nicht) |
| 2026-03-10 14:30 | `27f69d6` | MCP4728 min_power 0.3 → 0.2 angepasst |
| 2026-03-10 14:39 | `1f71ea5` | Pumpe + Design-Details |
| 2026-03-10 (session) | — | Overlays-Buttons von Test-Tab nach Einstellungen/System verschoben |
| 2026-03-10 (session) | — | Fonts: `·` (U+00B7) und `–` (U+2013) zu font_normal + font_small ergänzt |
| 2026-03-10 (session) | — | `zero_means_zero: true` für output_pumpe_dacA (kein Nachlaufen bei Slider=0) |
| 2026-03-10 (session) | — | Tank-Widget ersetzt durch runden Schwenker-Button (196×196px, grau/grün), schwenker_ring hellblau |
| 2026-03-10 (session) | — | System-Hauptschalter `btn_system_power` (80×80px, rot/grün, oben rechts) |
| 2026-03-10 (session) | — | scripts: system_ein/system_aus; font_icon_xl (120px FA U+F021); font_icons +U+F011 |
| 2026-03-10 (session) | — | Einstellungen: neuer Tab "Schwenker" (Pos. 2): "Motor freigeben" + "Motor 0° setzen" |
| 2026-03-10 (session) | — | Slot-Tabs: 50→60px, clickable+on_click → script_schwenker_goto_slot(N); Timer-Areas 240→230px |
| 2026-03-10 (session) | — | schwenker.yaml: script_schwenker_goto_slot (Slot 1=0°…6=300°, kürzester Weg) |
| 2026-03-10 (session) | — | hardware.yaml: on_boot priority:-200 → script_motor_set_zero (Motor auf 0°) |
| 2026-03-20 (session) | — | schwenker.yaml: `sw_work_current_mA` (NVS), Motorstrom-Slider speichert + on_boot sync |
| 2026-03-20 (session) | — | schwenker.yaml: Motor Idle-Timeout 10s → FOC + 500mA; `sw_motor_busy` + `sw_motor_last_move_ms` |
| 2026-03-20 (session) | — | schwenker.yaml: Sanfter Stop via `sw_stop_pending` – Halbzyklus fertig, dann stoppen |
| 2026-03-20 (session) | — | schwenker.yaml: script_schwenker_stop komplett überarbeitet (pending-Flag statt Sofortstopp) |
| 2026-03-20 (session) | — | lvgl_basis.yaml: Slot-Marker ring_slot1-6: obj → button + on_click → goto_slot(N) |
| 2026-03-20 (session) | — | schwenker.yaml: goto_slot stoppt Schwenker geordnet (F5+800ms) vor Positionsfahrt |
| 2026-03-20 (session) | — | schwenker.yaml: Speed-Clamp entfernt, max_value arc_rpm=100, default=15 RPM |
| 2026-03-20 (session) | — | lvgl_basis.yaml: Wochentag-Fix `t.day_of_week - 1` (1-basiert → 0-basiert) |
| 2026-03-20 (session) | — | `overlay_schwenker_settings` komplett redesignt: 3 Arcs (y=145), Trapez-ASCII, 5 Presets (①–⑤) |
| 2026-03-20 (session) | — | overlay_schwenker_test + btn_sw_motor_mode_toggle (Closed Loop/FOC) entfernt; Closed Loop hardcoded 0x04 |
| 2026-03-20 (session) | — | `btn_schwenker_main`: on_press → on_short_click (Long-Press startet/stoppt nicht mehr) |
| 2026-03-20 (session) | — | `script_schwenker_settings_open` neu: zentrale Sync-Funktion für Overlay-Öffnen |
| 2026-03-20 (session) | — | `sw_acc` Bedeutung geändert: 10–100 % Rampenanteil (war Motor-Rampenbyte); `sw_motor_ramp` = Motor-internes Byte |
| 2026-03-20 (session) | — | `font_arc_val` Glyphs: `%` ergänzt; `font_icons` Glyphs: U+F021 ergänzt |
| 2026-03-20 (session) | — | `font_preset_num` neu: Noto+Sans+Symbols size=28, nur ①②③④⑤ (Roboto hat diese Zeichen nicht) |
| 2026-03-20 (session) | — | Alle 9 YAML-Dateien: `# ── Komponenten ──` Übersicht am Anfang jeder Datei eingefügt |
| 2026-03-24 (session) | — | `display.yaml` → `display_7z_settings.yaml` umbenannt (Hardware-Substitutions: Pins, CAN, Konstanten) |
| 2026-03-24 (session) | — | `lights.yaml` neu: WS2812-Ring (`pin_led_h1`, 80 LEDs) mit `Slot Colors`-Effekt (6 Farben, motorpositionsbasiert) |
| 2026-03-26 (session) | — | `lvgl_basis.yaml`: Tab "Licht" – Zeile "Slot-Breite": Slider `slider_slot_breite` (1–28 LEDs) → `global_leds_per_slot`; Wert-Label `lbl_slot_breite_val` |
| 2026-03-27 (session) | — | `sensorphalanx.yaml`: Temperaturanzeige Komma-Format (`%d,%d °C`), `font_ts_xl` Glyphs mit `,` ergänzt |
| 2026-03-27 (session) | — | `lvgl_overlay.yaml`: `bar_distanz` gespiegelt (`110 - d`), kein ungültiges `inverted:`; Startwert 60 |
| 2026-03-27 (session) | — | `sensorphalanx.yaml`: `bin_standby` (TOF > 800mm & lvgl_is_idle, `delayed_on: 300s`, `delayed_off: 3s`) |
| 2026-03-27 (session) | — | `lvgl_basis.yaml`: `lbl_standby_icon` (Mond U+F186, 40px, `#8899BB`) in Statusleiste; `font_icons` +U+F186 |
| 2026-03-27 (session) | — | `sensorphalanx.yaml`: `bin_objekt_vor_sensor` NaN→`return false`; `delayed_off: 3s`-Filter |
| 2026-03-27 (session) | — | `lvgl_overlay.yaml`: `font_icon_ts` 120px → **80px**; Sweet Spot 50mm → 45mm |
| 2026-03-27 (session) | — | `lvgl_overlay.yaml`: Laser-Symbol entfernt; FOV-Kreis (`obj_ir_circle`) + Kreuz dynamisch via TOF skaliert |
| 2026-03-27 (session) | — | `script_close_temp_overlay` aufgeräumt: nur noch `lvgl.widget.hide: overlay_temp_messung` |
| 2026-03-27 (session) | — | `sensorphalanx.yaml`: Globals `global_emissivity` (0.9), `global_mat_type` (0), `global_mat_color` (3) |
| 2026-03-27 (session) | — | `lvgl_overlay.yaml`: `obj_emissivity_container` (Slider + Range-Balken) + `obj_material_selector` (3 Buttons + 6 Farb-Swatches) |
| 2026-03-27 (session) | — | `sensorphalanx.yaml`: `sensor_Temp_OBJECT` liest `global_emissivity` statt hardcoded 0.90 |
| 2026-03-27 (session) | — | `ui/overlay_temp_messung.xml` erstellt: LVGL XML-Layout-Export für viewer.lvgl.io |
| 2026-03-27 (session) | — | `display_7z_settings.yaml`: Substitutionen c_standby_*, c_tof_update_interval*, c_ir_update_interval* |


---

## Globale Variablen

Für 6 Timer-Slots (Index 0–5):

| ID | Typ | Bedeutung |
|---|---|---|
| `slot_start_ms` | `std::array<uint32_t, 6>` | millis() beim letzten Start |
| `slot_elapsed_ms` | `std::array<uint32_t, 6>` | akkumulierte Zeit in ms |
| `slot_status` | `std::array<int, 6>` | 0=gestoppt, 1=läuft, 2=pausiert |

**Interval:** 500 ms → aktualisiert alle 6 Timer-Labels wenn Status = 1.

---

## Page 1: Hauptseite (`page_main`)

### Titel
- Text: "Supercooler Drehwurm Kühler"
- Font: `font_title`, Farbe: `#003366`, oben mittig, y=20

### Tank-Widget (Platzhalter)
- **TODO: durch echtes PNG-Bild ersetzen**
- Position: zentriert, y=20, 360×360 px
- Aufbau aus LVGL `obj`-Widgets (Zylinder-Illusion):
  - `cyl_rim`: oben, grau, oval (Öffnung)
  - `cyl_body`: Mantelrechteck, grau
  - `water_surface`: hellblaue Ellipse (Wasseroberfläche)
  - `water_body`: blaues Rechteck (Wasserkörper)
  - `cyl_bottom`: dunkelgraue Ellipse (Boden)

### 6 Farbslots (Timer-Buttons)

Anordnung im Uhrzeigersinn nach Farbrad:

| Slot | Farbe | Seite | y-Position | Index |
|---|---|---|---|---|
| 1 | Rot `#FF0000` | links | 110 | 0 |
| 2 | Gelb `#FFFF00` | links | 250 | 1 |
| 3 | Grün `#00FF00` | links | 390 | 2 |
| 4 | Cyan `#00FFFF` | rechts | 110 | 3 |
| 5 | Blau `#0000FF` | rechts | 250 | 4 |
| 6 | Magenta `#FF00FF` | rechts | 390 | 5 |

**Jeder Slot ist ein `button`** (290×120 px, radius=16, clip_corner=true, bg_opa=TRANSP):

- **Linker Tab** (60×100%, `clickable: true`): volle Slot-Farbe, Slot-Nummer zentriert  
  `on_click` → `script_schwenker_goto_slot->execute(N)` (N=1–6, Zielwinkel = (N-1)×60°)
- **Rechter Bereich** (230×100%, `clickable: false`): 50% Opacity, enthält:
  - Label "Timer:" oben links
  - Zeitanzeige `MM:SS` darunter
  - Play/Pause-Icon rechtsbündig (x=-17)

**Touch-Verhalten:**
- `on_short_click`: Start (Status 0→1) / Pause (Status 1→2) Toggle
- `on_long_press`: Reset — nur wenn Status=2 (pausiert) → Status=0, Anzeige=00:00

> `on_short_click` feuert bei Long Press **nicht** → kein Extra-Flag nötig

**Textfarben** (auf 50%-Hintergrund):
| Slot | Textfarbe |
|---|---|
| 1 R | `#880000` |
| 2 Y | `#666600` |
| 3 G | `#004400` |
| 4 C | `#004444` |
| 5 B | `#FFFFFF` |
| 6 M | `#880044` |

### Navigation
- Button "Einstellungen" unten rechts → `lvgl.page.show: page_settings`

---

## Page 2: Einstellungen (`page_settings`)

### Titel
- Text: "Einstellungen", Font: `font_title`, Farbe: `#003366`, oben mittig

### TabView (3 Tabs, `font_tab`, Hintergrund `#E0E0E0`)

**Tab "Bildschirm":**
- Zeile: Label "Helligkeit" + Slider (`slider_helligkeit`, 0–100, Standard 80)
  - `on_value` → `light.control` auf `light_screen_background` mit `brightness: x/100.0`
  - Bei manueller Bedienung: Auto-Modus deaktivieren + Switch zurücksetzen
- Zeile: Switch `sw_brightness_auto` + Label "Auto (VEML7700)"
  - Ein: VEML7700-Lux steuert Helligkeit automatisch (Formel: 0 lux → 50%, 100 lux → 100%)
  - Aus: manuelle Steuerung
- Farbtest-Quadrate (150×150 px): Rot / Grün / Blau nebeneinander zentriert

**Tab "System":** Platzhalter

**Tab "Kühler":** Platzhalter

### Navigation
- Button "Zurück" unten links, `#444444` → `lvgl.page.show: page_main`

---

## Bekannte LVGL-8-Einschränkungen

| Problem | Lösung |
|---|---|
| `transform_scale_y` nicht verfügbar (LVGL9) | nicht verwenden |
| `bg_opa` nur `%`-Suffix oder Keywords | `50%`, `COVER`, `TRANSP` |
| ESPHome setzt `LV_OBJ_FLAG_CLICKABLE` auf alle `obj` | `clickable: false` direkt im YAML setzen |
| `obj` empfängt keinen `on_click` zuverlässig | Widget-Typ `button` verwenden |

---

## Status

- [x] Font-Definitionen
- [x] Timer-Globals (6 Slots)
- [x] Interval 500ms (alle 6 Slots)
- [x] Hauptseite Titel
- [x] Tank-Widget (Platzhalter)
- [x] 6 Farbslots im Farbrad-Layout
- [x] Split-Design: voller Farbtab + 50%-Timer-Bereich
- [x] Touch: `on_short_click` / `on_long_press`
- [x] Helligkeit-Slider → `light_screen_background`
- [x] Farbtest-Quadrate in Einstellungen
- [ ] Tank-Platzhalter durch echtes PNG ersetzen
- [ ] Einstellungen Tab "System" füllen
- [ ] Einstellungen Tab "Kühler" füllen
- [x] `sensor_temp_becken` via DS18B20 I²C-Bridge (`1w_i2c_bridge`, ESP-IDF 5.x) aktiv

---

## Changelog

| Datum | Änderung | Datei |
|---|---|---|
| 2026-03-15 | 1-Wire: DS2484 (I2C-Bridge) entfernt → Nativer GPIO-Bus auf GPIO45 (`pin_1w`) | `hardware.yaml` |
| 2026-03-15 | DS18B20 `0xae00000fba143528` als `sensor_temp_becken` eingebunden, Template entfernt | `hardware.yaml` |
| 2026-03-15 | DS18B20 `resolution: 9` (94ms statt 750ms) + `update_interval: 10s` → LVGL-Flickern behoben | `hardware.yaml` |
| 2026-03-15 | DS2484 wieder eingebaut (`active_pullup: true`), GPIO-Bus entfernt – echter Fix für LVGL-Flackern | `hardware.yaml` |
| 2026-03-15 | 1-Wire / DS2484 / DS18B20 aufgegeben – SHT30 I²C als `sensor_temp_becken` eingebaut | `hardware.yaml` |
| 2026-03-15 | Becken-Temperatur-Label `lbl_temp_becken` in Statusleiste (rechts neben Schneeflocke, Farbe `#2299DD`) | `lvgl_basis.yaml` |
| 2026-03-15 | `lbl_temp_becken`: Font auf `font_temp` (hat `°C`), Update via `lvgl.widget.refresh` + Lambda im Widget | `hardware.yaml`, `lvgl_basis.yaml` |
| 2026-03-15 | TCA9548A-Multiplexer (0x70) eingebaut; sensorphalanx-Sensoren auf `i2c_mux_ch0`, SHT30 auf Haupt-`i2c_bus` | `hardware.yaml`, `sensorphalanx.yaml` |
| 2026-03-15 | `mcp4728` fehlende `i2c_id: i2c_bus` ergänzt | `hardware.yaml` |
| 2026-03-15 | `c_pumpe_standby_perc` → `c_pumpe_umwaelzung_ein_perc: 30` in Substitutions | `display.yaml` |
| 2026-03-15 | Pump-Slider (`row_turmpumpe`, `row_umwaelzpumpe`) von Tab "Test" → Tab "System" (y:110/200, nach row_overlays) | `lvgl_basis.yaml` |
| 2026-03-15 | `on_control`: `c_pumpe_standby_perc` → `c_pumpe_umwaelzung_ein_perc`; `on_state`: `slider_umwaelzpumpe` synchronisiert Thermostat-Modus | `hardware.yaml` |
| 2026-03-15 | DS18B20-Bridge: I²C-Slave entfernt → BLE BTHome v2 Advertising (NimBLE, ESP32-C3 v3.x) | `ds18b20_i2c_bridge.ino` |
| 2026-03-15 | `ble.yaml` neu: `esp32_ble_tracker` (passiv); `esp32_hosted` extern, nicht in dieser Datei | `ble.yaml` |
| 2026-03-15 | BLE MAC ermittelt (`dc:da:0c:a1:89:8e`); `sensor_temp_becken` via BTHome v2 aktiviert | `ble.yaml` |
| 2026-03-15 | I²C-Bridge `temp_bridge` (0x48) + Template-Sensor aus `hardware.yaml` entfernt | `hardware.yaml` |
| 2026-03-17 | TCA9548A-Mux entfernt; alle sensorphalanx-Sensoren direkt auf `i2c_bus` umgestellt | `sensorphalanx.yaml` |
| 2026-03-17 | I²C-Bridge (`1w_i2c_bridge`, ESP-IDF 5.x) wieder aktiv; `sensor_temp_becken` liest `temp_bridge` (0x48, 3s); `ble.yaml`-BLE-Sensor abgelöst | `hardware.yaml` |
| 2026-03-18 | Timer-Kontrast: Alle 6 Slot-Timer-Bereiche: `font_small` → Roboto@700 (fett), `font_timer` 35→38px, `bg_opa` 50%→40%; Textfarben Slots 1–4 → schwarz, Slot 6 → weiß | `lvgl_basis.yaml` |
| 2026-03-18 | `btn_beckenfuellung`: Runder Button 80×80px, Position tangential 45° rechts unten am Ring (`align: CENTER, x:156, y:176`); Icon `\uF773` (fa-water) zu `font_icons`-Glyph-Liste ergänzt | `lvgl_basis.yaml` |
| 2026-03-18 | `script_pumpe_a_kurz/lang`: Slider `slider_turmpumpe` wird am Ende auf 15 gesetzt | `schwenker.yaml` |
| 2026-03-18 | Umwälzpumpe: `on_state`-Slider-Reset entfernt; Slider-Sync nur noch bei `cool_action` (→30%) und `idle_action`/`on_control` (→0%); manuelle Slider-Änderung bleibt erhalten | `hardware.yaml` |
| 2026-03-18 | `script_pumpe_a_lang`: 100%-Spülphase 7s → 9s | `schwenker.yaml` |
| 2026-03-18 | `script_schwenker_goto_slot`: Motor-Modus fix auf Closed Loop (`0x04`), unabhängig von `sw_motor_mode` | `schwenker.yaml` |
| 2026-03-18 | `script_schwenker_goto_slot`: acc 20→10; acc=0/255-Guard (sofortiger Ruck) in `script_motor_goto_relative_degree`; sanfter Stop via F5 speed=0 acc=10 + 500ms Delay vor FOC | `schwenker.yaml`, `motorcontrol_can-bus.yaml` |
| 2026-03-18 | Compile-Fehler behoben: Kommentar-Text `(25% → 15% …)` befand sich außerhalb Lambda-Kommentar in `script_pumpe_a_kurz` | `schwenker.yaml` |
| 2026-03-18 | App-Titel umbenannt auf "SCK Schwippschwenker"; `font_title`-Glyphs um `K` ergänzt | `lvgl_basis.yaml` |
