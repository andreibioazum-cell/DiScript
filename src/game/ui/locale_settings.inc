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

/* Three themes in one button on the Play screen: ordinary, New Year (grey
 * while unavailable) and Halloween. The label always says which one is active. */
static const char *tr_theme_label(void) {
    double mode = active_theme_mode();
    if (language == 1) {
        if (mode == 1) {
            return winter_theme_on() == 1 ? "Тема: Новогодняя" : "Тема: Новогодняя (недоступна)";
        }
        if (mode == 2) {
            return "Тема: Хэллоуин";
        }
        return "Тема: Обычная";
    }
    if (mode == 1) {
        return winter_theme_on() == 1 ? "Theme: New Year" : "Theme: New Year (locked)";
    }
    if (mode == 2) {
        return "Theme: Halloween";
    }
    return "Theme: Ordinary";
}
