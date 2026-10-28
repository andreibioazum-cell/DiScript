static const char *tr_top(void) {
    if (language == 1) {
        return "Топ";
    }
    return "Top";
}

/* Exit notification strings. */
static const char *tr_bye_sorry(void) {
    if (language == 1) {
        return "Нам жаль что вы вышли, удачи!";
    }
    return "Sorry that you left, good luck!";
}

static const char *tr_bye_cubes(void) {
    if (language == 1) {
        return "Пока, кубы будут вас ждать!";
    }
    return "Bye, the cubes will be waiting for you!";
}

static const char *tr_bye_nick_pre(void) {
    if (language == 1) {
        return "Пока, ";
    }
    return "Bye, ";
}

static const char *tr_bye_nick_post(void) {
    if (language == 1) {
        return ", удачи!";
    }
    return ", good luck!";
}

/* Admin panel titles. */
static const char *tr_admin_title(void) {
    return "CONTENT CREATOR PANEL";
}

static const char *tr_admin_ban_protected(void) {
    if (language == 1) {
        return "Администраторов нельзя банить";
    }
    return "Admin accounts cannot be banned";
}

static const char *tr_skins_note_for(double cls) {
    if (cls == CLASS_EBUC) {
        if (language == 1) {
            return "Скин «Эльф»: достижение «Подарок буКа»";
        }
        return "Elf skin: \"ebuC's gift\" achievement";
    }
    if (cls == CLASS_ASTRA) {
        return language == 1 ? "Скин бесплатный при покупке Астры" : "Free skin with Astra";
    }
    return tr_skins_need_legends();
}

