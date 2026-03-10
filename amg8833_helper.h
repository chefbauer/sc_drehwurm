#pragma once
#include "lvgl.h"
#include <cstdio>
#include <cmath>

// AMG8833 Zell-Objekte (8x8 Grid, lazy-created beim ersten Overlay-Aufruf)
static lv_obj_t* amg_cells[64] = {};
static bool      amg_cold_mode  = false;
static float     amg_last_temps[64] = {};

// ── Palette "Normal" (Iron/Rainbow): 15°C = schwarz, 40°C = weiß ────────────
inline lv_color_t amg_temp_color_normal(float temp) {
    float t = (temp - 15.0f) / 25.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    struct Stop { float pos; uint8_t r, g, b; };
    static const Stop palette[8] = {
        {0.000f,   0,   0,   0},  // schwarz
        {0.143f,   0,   0, 220},  // blau
        {0.286f,   0, 210, 220},  // cyan
        {0.429f,   0, 210,   0},  // grün
        {0.571f, 255, 255,   0},  // gelb
        {0.714f, 255, 120,   0},  // orange
        {0.857f, 255,   0,   0},  // rot
        {1.000f, 255, 255, 255},  // weiß
    };
    for (int i = 0; i < 7; i++) {
        if (t <= palette[i + 1].pos) {
            float seg = (t - palette[i].pos) / (palette[i + 1].pos - palette[i].pos);
            uint8_t r = (uint8_t)(palette[i].r + seg * (palette[i + 1].r - palette[i].r));
            uint8_t g = (uint8_t)(palette[i].g + seg * (palette[i + 1].g - palette[i].g));
            uint8_t b = (uint8_t)(palette[i].b + seg * (palette[i + 1].b - palette[i].b));
            return lv_color_make(r, g, b);
        }
    }
    return lv_color_make(255, 255, 255);
}

// ── Palette "Kalt": 0°C = dunkelblau, 5°C = cyan, 20°C = rot ────────────────
// Optimiert für Eiswasser-Erkennung — 0° und 5° sind deutlich unterscheidbar
inline lv_color_t amg_temp_color_kalt(float temp) {
    float t = temp / 20.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    struct Stop { float pos; uint8_t r, g, b; };
    static const Stop palette[6] = {
        {0.000f,   0,   0, 180},  // dunkelblau  (0°C  – Eis)
        {0.100f,   0,  80, 255},  // blau        (2°C)
        {0.250f,   0, 230, 230},  // cyan        (5°C  – klar unterscheidbar)
        {0.500f,   0, 200,   0},  // grün        (10°C)
        {0.750f, 255, 220,   0},  // gelb        (15°C)
        {1.000f, 255,  40,   0},  // rot         (20°C)
    };
    for (int i = 0; i < 5; i++) {
        if (t <= palette[i + 1].pos) {
            float seg = (t - palette[i].pos) / (palette[i + 1].pos - palette[i].pos);
            uint8_t r = (uint8_t)(palette[i].r + seg * (palette[i + 1].r - palette[i].r));
            uint8_t g = (uint8_t)(palette[i].g + seg * (palette[i + 1].g - palette[i].g));
            uint8_t b = (uint8_t)(palette[i].b + seg * (palette[i + 1].b - palette[i].b));
            return lv_color_make(r, g, b);
        }
    }
    return lv_color_make(255, 40, 0);
}

inline lv_color_t amg_temp_color(float temp) {
    return amg_cold_mode ? amg_temp_color_kalt(temp) : amg_temp_color_normal(temp);
}

// Grid-Zellen programmatisch erstellen (einmalig beim ersten Aufruf)
inline void amg_create_grid(lv_obj_t* parent, int cell_size) {
    for (int i = 0; i < 64; i++) {
        if (amg_cells[i] != nullptr) continue;
        int row = i / 8;
        int col = 7 - (i % 8);  // X-Spiegel: AMG8833 Pixel 0 = oben rechts
        lv_obj_t* cell = lv_obj_create(parent);
        lv_obj_set_size(cell, cell_size - 2, cell_size - 2);
        lv_obj_set_pos(cell, col * cell_size, row * cell_size);
        lv_obj_set_style_bg_color(cell, lv_color_hex(0xADD8E6), 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cell, 1, 0);
        lv_obj_set_style_border_color(cell, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_radius(cell, 2, 0);
        lv_obj_set_style_pad_all(cell, 0, 0);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* lbl = lv_label_create(cell);
        lv_label_set_text(lbl, "0.0");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_font(lbl, lv_font_default(), 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        amg_cells[i] = cell;
    }
}

// Zellen neu zeichnen – wird auch nach Moduswechsel aufgerufen
inline void amg_refresh_cells() {
    char buf[8];
    for (int i = 0; i < 64; i++) {
        if (amg_cells[i] == nullptr) continue;
        float t = amg_last_temps[i];
        lv_obj_set_style_bg_color(amg_cells[i], amg_temp_color(t), 0);
        lv_obj_t* lbl = lv_obj_get_child(amg_cells[i], 0);
        if (lbl) {
            snprintf(buf, sizeof(buf), "%.1f", t);
            lv_label_set_text(lbl, buf);
        }
    }
}

// Zellen mit neuen Temperaturwerten aktualisieren
inline void amg_update_cells(const float* temps) {
    for (int i = 0; i < 64; i++)
        amg_last_temps[i] = (std::isnan(temps[i])) ? 0.0f : temps[i];
    amg_refresh_cells();
}
