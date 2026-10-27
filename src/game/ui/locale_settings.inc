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

static const char *tr_winter_toggle(void) {
    if (language == 1) {
        if (winter_theme == 1) {
            return "Зимняя тема: ВКЛ";
        }
        return "Зимняя тема: ВЫКЛ";
    }
    if (winter_theme == 1) {
        return "Winter theme: ON";
    }
    return "Winter theme: OFF";
}

static const char *tr_christmas_banner(void) {
    if (language == 1) {
        return "С Новым годом!";
    }
    return "Merry Christmas!";
}

/* Three themes in one button: ordinary, New Year (grey while unavailable),
 * and night. The label always says which one is active right now. */
static const char *tr_theme_label(void) {
    if (language == 1) {
        if (theme_mode == 1) {
            return winter_theme_on() == 1 ? "Тема: Новогодняя" : "Тема: Новогодняя (недоступна)";
        }
        if (theme_mode == 2) {
            return "Тема: Ночная";
        }
        return "Тема: Обычная";
    }
    if (theme_mode == 1) {
        return winter_theme_on() == 1 ? "Theme: New Year" : "Theme: New Year (locked)";
    }
    if (theme_mode == 2) {
        return "Theme: Night";
    }
    return "Theme: Ordinary";
}
