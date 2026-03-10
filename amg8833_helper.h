#pragma once
#include "lvgl.h"
#include <cstdio>
#include <cmath>

// AMG8833 Zell-Objekte (8x8 Grid, lazy-created beim ersten Overlay-Aufruf)
static lv_obj_t* amg_cells[64] = {};

// Farbinterpolation: 0°C = hellblau (#ADD8E6), ≥20°C = hellrot (#FF8080)
inline lv_color_t amg_temp_color(float temp) {
    float t = temp / 20.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    uint8_t r = (uint8_t)(173 + t * (255 - 173));
    uint8_t g = (uint8_t)(216 - t * (216 - 128));
    uint8_t b = (uint8_t)(230 - t * (230 - 128));
    return lv_color_make(r, g, b);
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
