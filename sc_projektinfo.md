# SC Drehwurm – Projektinfo

---

> **KI-Arbeitsanweisung:** Diese Datei ist die zentrale Projektdokumentation.
> **Nach jeder Änderung an YAML-Dateien** diese Projektinfo automatisch aktualisieren –
> ohne dass der Benutzer es explizit fordert. Relevante Daten: Pin-Belegungen,
> Plattform-/Bus-Entscheidungen, Sensor-IDs, neue Komponenten, geänderte Logik,
> offene TODOs. Änderungen im Abschnitt **Changelog** dokumentieren.

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
| 1-Wire Bus | DS18B20 an ESP32-C3 BLE-Bridge → BTHome v2 |

---

## Becken-Temperatursensor

**Sensor:** DS18B20 via BLE-Bridge (ESP32-C3, BTHome v2)

**MAC:** `dc:da:0c:a1:89:8e`  
**Sensor-ID:** `sensor_temp_becken` in `ble.yaml`  
**ROM-Code:** `28:B2:FE:B9:0F:00:00:2E`

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

**Bus:** Nativer ESP32 1-Wire auf `${pin_1w}` (GPIO45), ESPHome `platform: gpio`.

| Rolle | Sensor | ID | Status |
|---|---|---|---|
| Becken-Temperatur | DS18B20 | `0xae00000fba143528` | aktiv, `update_interval: 10s` |

**Sensor-Einbindung (hardware.yaml):** `sensor_temp_becken` läuft als echter `dallas_temp`-Sensor; Thermostat verwendet ihn direkt.

---

## Fonts

**Regel für große Fonts (Größe ≥ 60):** Glyphs auf die tatsächlich verwendeten Zeichen beschränken – vollständige Zeichensätze erzeugen hier zu große Binaries.  
Bei Änderungen von Texten mit `font_title` muss die Glyph-Liste manuell aktualisiert werden.

**Fonts < 60:** Vollständiger Standardzeichensatz (`a–z`, `A–Z`, `0–9`, Sonderzeichen) ist OK und bevorzugt.

| ID | Datei | Größe | Verwendung |
|---|---|---|---|
| `font_title` | Roboto 700 (gfonts) | 60 | Seitentitel |
| `font_normal` | Roboto 400 (gfonts) | 28 | Labels, Buttons, allgemein |
| `font_tab` | Roboto 400 (gfonts) | 30 | Tab-Beschriftungen |
| `font_small` | Roboto 400 (gfonts) | 18 | Beschriftungen, Overlay-Hilfstexte |
| `font_icons` | Font Awesome Solid 6.5.0 (CDN) | 40 | Play/Pause/Schneeflocke/Zahnrad (U+F04B, U+F04C, U+F2DC, U+F013) |
| `font_timer` | 5x7-dot-matrix.ttf (lokal) | 35 | Timer-Anzeige MM:SS |
| `font_clock` | Roboto 400 (gfonts) | 22 | Uhrzeit HH:MM:SS in Statusleiste |

**Sonderzeichen in Glyphs** (alle vier Text-Fonts): `äöüÄÖÜß°·–`  
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

**Interval:** 500 ms → aktualisiert alle 6 Timer-Labels wenn Status = 1.  
Aktualisiert auch das AMG8833-Overlay wenn es sichtbar ist.

---

## Page 1: Hauptseite (`page_main`)

### Titel
- Text: "SC Schwippschwenker"
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

- **Linker Tab** (50×100%, `clickable: false`): volle Slot-Farbe, Slot-Nummer zentriert
- **Rechter Bereich** (240×100%, `clickable: false`): 50% Opacity, enthält:
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
- **Rechts:** Button `btn_to_settings` (60×54 px, dunkelgrau `#444444`) mit Zahnrad-Icon (`\uF013`) → `lvgl.page.show: page_settings`
- Farbe des Schneeflocken-Icons wird via `climate.on_state` Lambda gesetzt

### Navigation
- Settings-Button ist Teil der Statusleiste (rechts)

---

## Page 2: Einstellungen (`page_settings`)

### Titel
- Text: "Einstellungen", Font: `font_title`, Farbe: `#003366`, oben mittig

### TabView (4 Tabs, `font_tab`, Hintergrund `#E0E0E0`)

Tab-Reihenfolge: **System · Bildschirm · Kühler · Test**

---

**Tab "System":**
- `row_overlays`: Label "Overlays:" + zwei Buttons nebeneinander:
  - `btn_amg_live` (dunkelblau `#334466`) → öffnet `overlay_amg8833`
  - `btn_sensorphalanx_live` (grün `#336644`) → öffnet `overlay_sensorphalanx`

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

### `overlay_amg8833` — AMG8833 Live-Ansicht
- Vollflächig (100%×100%), weiß, `bg_opa: 90%`, initial `hidden: true`
- `amg_grid_container` (496×496 px, leicht links): 8×8 Pixel-Zellen via `amg_create_grid()`
- Exit-Button `btn_amg_exit` (rot `#CC3333`, 80×80) rechts → `lvgl.widget.hide: overlay_amg8833`
- Daten werden per 500ms-Interval aus `amg_pixel_temps` aktualisiert

### `overlay_sensorphalanx` — Sensor-Phalanx
- Vollflächig (100%×100%), weiß, `bg_opa: 93%`, initial `hidden: true`
- Titel "Sensor-Phalanx" (`font_title`, `#003366`)
- Exit-Button `btn_sensorphalanx_exit` (rot, oben rechts)
- 7 Sensor-Zeilen (zebra-gestreift), je 860×65 px mit farbigem Badge + Name + Beschreibung + Live-Wert:

| Badge | Farbe | Sensor | Beschreibung | Wert-Label |
|---|---|---|---|---|
| ToF | `#2255CC` | VL53L1X | Time-of-Flight · 50–4000 mm | `distance_value_label` |
| T° | `#CC5500` | SHT4x | Umgebungstemperatur | `temp_value_label` |
| RH | `#008899` | SHT4x | Relative Luftfeuchte | `hum_value_label` |
| hPa | `#7744BB` | BMP581 | Barometrischer Druck | `pressure_value_label` |
| ALT | `#227733` | BMP581 | Höhe (hypsometrisch) | `height_value_label` |
| LUX | `#BB8800` | VEML7700 | Umgebungslicht | `light_value_label` |
| IR° | `#CC2200` | MLX90632 | IR Objekttemperatur | `mlx_value_label` |

---

## Hardware (`hardware.yaml`)

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
| VL53L1X | `vl53l1x` | 0x29 | Distanz, Short Mode, 500ms |
| SHT4x | `sht4x` | auto | Temperatur + Luftfeuchtigkeit, 1s |
| BMP581 | `bmp581_i2c` | auto | Druck + Temperatur, 1s |
| `indoor_altitude` | template | — | Höhe aus BMP581, hypsometrisch |
| VEML7700 | `veml7700` | 0x10 | Umgebungslicht, 2s |
| `sensor_Temp_OBJECT` | template | — | MLX90632 IR-Berechnung (ε=0.90), 1s |
| `i2c_mlx90632` | i2c_device | 0x3A | I²C-Device für MLX90632 |

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
- [x] Einstellungen: Tab-Reihenfolge System · Bildschirm · Kühler · Test
- [x] Tab System: Overlay-Buttons (AMG8833 Live, Sensor-Phalanx)
- [x] Tab Bildschirm: Helligkeit-Slider + Farbtest-Quadrate
- [x] Tab Kühler: Kühlung-Switch + Temperatur-Slider + Preset-Buttons
- [x] Tab Test: Turmpumpe-Slider + Beckenpumpe-Slider
- [x] Overlay AMG8833 Live (8×8 Grid, Exit-Button)
- [x] Overlay Sensor-Phalanx (7 Sensoren, Badge-Design)
- [x] hardware.yaml: MCP4728 DAC, Lüfter, Pumpen, Relais, AMG8833
- [x] hardware.yaml: Climate Thermostat mit Kühler-Steuerung
- [x] sensorphalanx.yaml: VL53L1X, SHT4x, BMP581, VEML7700, MLX90632
- [x] `zero_means_zero: true` für Turmpumpe (kein Nachlaufen)
- [ ] Tank-Platzhalter durch echtes PNG ersetzen
- [ ] `sensor_temp_becken` durch echten Sensor ersetzen

---

## Update-Log

| Datum / Zeit (UTC) | Commit | Änderung |
|---|---|---|
| 2026-03-07 07:52 | `b6e12e6` | Initial commit – Projektstruktur angelegt |
| 2026-03-07 08:05 | `d600b37` | Projektstart: Grundstruktur lvgl_basis.yaml |
| 2026-03-07 08:12 | `5b73e94` | Helligkeit-Slider in Einstellungen |
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

- **Linker Tab** (50×100%, `clickable: false`): volle Slot-Farbe, Slot-Nummer zentriert
- **Rechter Bereich** (240×100%, `clickable: false`): 50% Opacity, enthält:
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
- [x] `sensor_temp_becken` von Template auf echten DS18B20-Sensor umstellen (ID ausstehend)

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
