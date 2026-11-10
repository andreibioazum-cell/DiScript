static const char *tr_hitboxes_toggle(void) {
    if (language == 1) {
        if (show_hitboxes == 1) {
            return "Хитбоксы: ВКЛ";
        }
        return "Хитбоксы: ВЫКЛ";
    }
    if (show_hitboxes == 1) {
        return "Hitboxes: ON";
    }
    return "Hitboxes: OFF";
}

static const char *tr_christmas_banner(void) {
    if (language == 1) {
        return "С Новым годом!";
    }
    return "Merry Christmas!";
}

/* Two themes in one button on the Play screen: ordinary and New Year (grey
 * while unavailable). The label always says which one is active. */
static const char *tr_theme_label(void) {
    double mode = active_theme_mode();
    if (language == 1) {
        if (mode == 1) {
            return winter_theme_on() == 1 ? "Тема: Новогодняя" : "Тема: Новогодняя (недоступна)";
        }
        return "Тема: Обычная";
    }
    if (mode == 1) {
        return winter_theme_on() == 1 ? "Theme: New Year" : "Theme: New Year (locked)";
    }
    return "Theme: Ordinary";
}


static const char *tr_graphics_mode_name(double mode) {
    if (mode == 1) {
        return "MSAA";
    }
    if (mode == 2) {
        return "SMAA";
    }
    return language == 1 ? "Пикселизация" : "Pixelation";
}

static const char *tr_graphics_mode_label(void) {
    if (language == 1) {
        if (graphics_mode == 1) return "Графика: MSAA";
        if (graphics_mode == 2) return "Графика: SMAA";
        return "Графика: Пикселизация";
    }
    if (graphics_mode == 1) return "Graphics: MSAA";
    if (graphics_mode == 2) return "Graphics: SMAA";
    return "Graphics: Pixelation";
}
