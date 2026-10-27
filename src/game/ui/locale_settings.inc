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

static const char *tr_theme_label(void) {
    if (language == 1) {
        return new_theme == 1 ? "Тема: Новая" : "Тема: Старая";
    }
    return new_theme == 1 ? "Theme: New" : "Theme: Old";
}
