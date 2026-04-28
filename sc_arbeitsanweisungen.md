# SC Drehwurm – Permanente KI-Arbeitsanweisungen

> Dieses Dokument enthält verbindliche Regeln und Muster für die Arbeit an diesem Projekt.
> **Vor jedem Eingriff in `lights.yaml` zwingend lesen.**
> Technische Hintergründe → `sc_projektinfo.md`.

---

## 1. addressable_lambda — Strikte Zugriffsregeln

`addressable_lambda` läuft in einem **eigenen RTOS-Task** (nicht im ESPHome-Main-Task).
Direktzugriff auf `GlobalsComponent` via `id(xyz)` führt zu **Crash / Watchdog-Reset**.

### Erlaubt:
| Zugriff | Warum sicher |
|---|---|
| `id(sensor_xyz).state` | `float`, atomar (plain assignment) |
| `bin_xyz->state` | `bool`, plain (kein GlobalsComponent-Wrapper) |
| `${substitution_name}` | Compile-Zeit-Konstante, kein Laufzeitzugriff |
| `millis()` | Thread-safe |

### Verboten:
| Zugriff | Problem |
|---|---|
| `id(global_xyz)` mit `GlobalsComponent` | Nicht thread-safe → Crash |
| `id(global_xyz).value()` | Selbes Problem |

### Workaround: Sensor-Wrapper (bestehend im Projekt)
Wenn ein Global-Wert in `addressable_lambda` benötigt wird, wird er als `template`-Sensor
gespiegelt und von der Lambda per `.state` gelesen:

```yaml
# In lvgl_basis.yaml / schwenker.yaml:
sensor:
  - platform: template
    id: sensor_leds_per_slot     # Wrapper für global_leds_per_slot
    lambda: return id(global_leds_per_slot);
    update_interval: 100ms

# In lights.yaml (addressable_lambda):
float lps = id(sensor_leds_per_slot).state;   # ✓ sicher
# id(global_leds_per_slot)                    # ✗ CRASH
```

---

## 2. ESPHome YAML — Struktur-Regeln

- **Ein Block pro Typ pro Datei**: `globals:`, `script:`, `interval:`, `sensor:` usw. dürfen
  pro YAML-Datei **nur einmal** als Top-Level-Key vorkommen.
  Mehrere `script:`-Blöcke → ESPHome: „Duplicate key", Package wird nicht geladen.
- **Alle Einträge desselben Typs** in einen einzigen Block schreiben.
- Gilt auch für `packages:` (nur ein Block pro Datei).
- Jede YAML-Datei hat am Anfang einen `# ── Komponenten ──` Kommentarblock mit allen IDs
  → **nach Änderungen aktualisieren**.

---

## 3. LVGL 9 (ESPHome 2026.4) — Widget-Properties

> Upgrade auf **LVGL 9.5** mit ESPHome 2026.4 abgeschlossen, kompiliert fehlerfrei.

| Falsch | Richtig |
|---|---|
| `flags: hidden` | `hidden: true` (direkte Property) |
| Lambda: kein direktes API | `lv_obj_add_flag(id(x), LV_OBJ_FLAG_HIDDEN)` |
| `flex_flow:` / `flex_align_*:` | ESPHome-YAML unterstützt kein Flex → `align: LEFT_MID` + `x:` |
| `bg_opa: TRANSP` | `bg_opa: 0%` |

---

## 4. LED-Effekte in lights.yaml

### Effekt-Architektur (Slot Colors)

Alle Effekte lesen ausschließlich über sichere Kanäle:
- `id(sensor_motor_position).state` → Motor 14-bit (0–16383 = 0°–360°)
- `id(sensor_leds_per_slot).state` → aktuelle Slot-Breite (1–28)
- `id(bin_slotN_blink).state` → Blink-Zustand je Slot (N=1–6)
- Substitutionen `${c_led_slot_N_r/g/b}` → Compile-Zeit Slot-Farben

### Vollring-Effekt (Mix / Vollring-Modus)

Zwei häufige Bugs bei Vollring-Implementierungen — bereits gefixt in „Slot Colors Mix":

**Bug 1: Richtungs-Inversion**
```c
// FALSCH: pos läuft immer CW, dir=1 bedeutet aber CCW
// RICHTIG:
if (dir == 1) pos = total - pos;
```

**Bug 2: Phasen-Offset (Segment-Zentrierung)**
```c
// Spot-Effekt: origin = (motor_pos / 16383.0f) * total
// Vollring-Effekt: Segment 0 startet bei pos=0, Zentrum bei seg/2
// → 14 LED Offset bei total=172, seg=28
// RICHTIG:
float origin_ring = origin - seg * 0.5f;
```

### Spot-Modus (lps < lps_max)

Helligkeit-Abstufung je nach Slot-Breite:
- lps ≥ 5: 33% / 66% / 100% / 66% / 33% (5 LEDs Breite)
- lps ≥ 3: 50% / 100% / 50%
- lps < 3: 100% (flach)

---

## 5. Motor-CAN — Polling-Setup (100 Hz)

Das 0x30-Positionspaket läuft in einem **separaten 10ms-Interval-Block** (nie zusammen mit der 50ms-Regelschleife):

```yaml
interval:
  - interval: 10ms
    then:
      - lambda: |
          // Nur 0x30 CAN-Abfrage hier (100 Hz Motorposition)
          ...

  - interval: 50ms
    then:
      - lambda: |
          // Sinus-Regelschleife, Idle-Timeout, etc.
          ...
```

**Regel:** Zwei `interval:`-Einträge können im selben `interval:`-Block stehen (eine Liste),
aber **nur ein `interval:` Top-Level-Block** pro Datei (YAML-Regel §2).

---

## 6. Motor Idle-Timeout

Nach 10 s Stillstand:
1. Mode 5 (FOC)
2. `script_motor_set_work_current_mA(100)` (0x83)
3. `script_motor_set_idle_current_perc(10)` (0x9B, param=0 → ~10 mA)

**Grund:** FOC mit 500 mA Arbeitsstrom führt zu Oszillation (Motor versucht unveränderlich
Position zu halten → Schwingung im Wasser). 100 mA + 10 % Idle = praktisch stromlos.

`script_motor_set_idle_current_perc(perc)`:
- 0x9B-Byte: `round(perc/10 - 1)`, Clamp auf gültigen Bereich 10–90 %
- 10 % → Byte 0x00 (niedrigster Wert)

---

## 7. sw_stop_pending — Reset bei Start nicht vergessen

`script_schwenker_start` muss **explizit** `id(sw_stop_pending) = false;` setzen.
Fehlt das Reset → nach `goto_slot` (das `sw_stop_pending=false` nicht immer räumt)
läuft der Schwenker einen Halbzyklus und stoppt dann sofort (Ein-Schuss-Bug).

---

## 8. Schnell-Checkliste vor lights.yaml Änderung

- [ ] Zugreife ich auf `id(global_xyz)`? → **NEIN**, nur `sensor.state` oder `bin->state`
- [ ] Neue Substitution nötig? → In `display_7z_settings.yaml` eintragen
- [ ] Vollring-Effekt mit Richtung? → `if (dir==1) pos = total - pos;` einbauen
- [ ] Vollring-Effekt mit Phasenbezug? → `origin_ring = origin - seg*0.5f` nutzen
- [ ] `update_interval: 10ms` gesetzt? (100 Hz Standard für alle Slot-Effekte)
- [ ] `# ── Komponenten ──` Kommentarblock in `lights.yaml` aktualisiert?
- [ ] Changelog + Update-Log in `sc_projektinfo.md` ergänzt?
