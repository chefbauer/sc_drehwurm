#pragma once
#include "lvgl.h"
#include <cstdio>
#include <cmath>

// AMG8833 Zell-Objekte (8x8 Grid, lazy-created beim ersten Overlay-Aufruf)
static lv_obj_t* amg_cells[64] = {};

// Thermisches Farbspektrum (Iron/Rainbow): 0°C = schwarz-blau, 30°C = weiß
// Palette: schwarz → blau → cyan → grün → gelb → orange → rot → weiß
inline lv_color_t amg_temp_color(float temp) {
    float t = temp / 30.0f;
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

// Grid-Zellen programmatisch erstellen (einmalig beim ersten Aufruf)
inline void amg_create_grid(lv_obj_t* parent, int cell_size) {
    for (int i = 0; i < 64; i++) {
        if (amg_cells[i] != nullptr) continue;
        int row = i / 8;
        int col = i % 8;
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

// Zellen mit neuen Temperaturwerten aktualisieren
inline void amg_update_cells(const float* temps) {
    char buf[8];
    for (int i = 0; i < 64; i++) {
        if (amg_cells[i] == nullptr) continue;
        float t = (std::isnan(temps[i])) ? 0.0f : temps[i];
        lv_obj_set_style_bg_color(amg_cells[i], amg_temp_color(t), 0);
        lv_obj_t* lbl = lv_obj_get_child(amg_cells[i], 0);
        if (lbl) {
            snprintf(buf, sizeof(buf), "%.1f", t);
            lv_label_set_text(lbl, buf);
        }
    }
}
