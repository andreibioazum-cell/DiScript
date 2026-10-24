static const char *tr_shop(void) {
    if (language == 1) {
        return "Магазин";
    }
    return "Shop";
}

static const char *tr_candies(void) {
    if (language == 1) {
        return "Леденцы";
    }
    return "Candies";
}

static const char *tr_class_ordinary(void) {
    if (language == 1) {
        return "Обычный";
    }
    return "Ordinary";
}

static const char *tr_class_azum(void) {
    if (azum_buff_active == 1) {
        return "@z&m";
    }
    if (language == 1) {
        return "Азум";
    }
    return "Azum";
}

static const char *tr_class_ordinary_desc(void) {
    return "I was the first";
}

static const char *tr_class_azum_desc(void) {
    if (azum_buff_active == 1) {
        return language == 1 ? "Суббота-17" : "Saturday-17";
    }
    return "Lived once, buried twice...";
}

static const char *tr_class_santa(void) {
    if (language == 1) {
        return "Дед Мороз";
    }
    return "Santa";
}

static const char *tr_class_santa_desc(void) {
    return "It's 12:00 AM!";
}

static const char *tr_class_astra(void) {
    if (language == 1) {
        return "Астра";
    }
    return "Astra";
}

static const char *tr_class_astra_desc(void) {
    return "I love you, Azum...";
}

static const char *tr_class_codecracker(void) { return "CodeCracker"; }

static const char *tr_class_codecracker_desc(void) { return "MOL Edits..."; }

static const char *tr_class_ebuc(void) {
    if (language == 1) {
        return "буК";
    }
    return "ebuC";
}

static const char *tr_class_ebuc_desc(void) {
    return "Tr4pp3d";
}
