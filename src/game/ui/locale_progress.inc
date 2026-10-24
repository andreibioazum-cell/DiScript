static const char *tr_super_codecracker(void) {
    if (language == 1) {
        return "Разгон";
    }
    return "Overclock";
}

static const char *tr_super(void) {
    if (language == 1) {
        return "Снежинка";
    }
    return "Snowflake";
}

static const char *tr_super_ebuc(void) {
    if (language == 1) {
        return "Ульта";
    }
    return "Ult";
}

static const char *tr_super_azum(void) {
    if (language == 1) {
        return "Рывок";
    }
    return "Dash";
}

static const char *tr_super_astra(void) {
    if (language == 1) {
        return "Захват";
    }
    return "Grab";
}

static const char *tr_class_selected(void) {
    if (language == 1) {
        return "Выбран";
    }
    return "Selected";
}

static const char *tr_class_pick(void) {
    if (language == 1) {
        return "Выбрать";
    }
    return "Select";
}

static const char *tr_levels(void) {
    if (language == 1) {
        return "Уровни";
    }
    return "Levels";
}

static const char *tr_level(void) {
    if (language == 1) {
        return "Уровень";
    }
    return "Level";
}

static const char *tr_level_status_open(void) {
    if (language == 1) {
        return "Открыт";
    }
    return "Opened";
}

static const char *tr_level_status_locked(void) {
    if (language == 1) {
        return "Закрыт";
    }
    return "Locked";
}

static const char *tr_level_unlock_cost(double cost) {
    const char *out = "";
    if (language == 1) {
        out = ds_concat(ds_concat(ds_concat("Открыть · ", ds_num_to_string((double)(cost))), " "), tr_cups_unit());
    } else {
        out = ds_concat(ds_concat(ds_concat("Unlock · ", ds_num_to_string((double)(cost))), " "), tr_cups_unit());
    }
    return out;
}

static const char *tr_class_level_effect(double cls, double n) {
    if (cls == CLASS_CODECRACKER) {
        /* His levels are his own, not the ordinary cube's: a bite that hits
         * harder, an overclock that comes back sooner and thicker overclock
         * armour. */
        if (n == 1) {
            return language == 1 ? "Укус: урон +10%" : "Bite: damage +10%";
        }
        if (n == 2) {
            return language == 1 ? "Ульта: перезарядка 3 с" : "Ult: cooldown 3 s";
        }
        if (n == 3) {
            return language == 1 ? "Ульта: поглощение урона 60%" : "Ult: damage absorb 60%";
        }
    }
    if (cls == 1 && n == 1) {
        if (language == 1) {
            return "Вторая жизнь: урон -10%";
        }
        return "Second life: damage -10%";
    }
    if (cls == 1 && n == 2) {
        if (language == 1) {
            return "Скорость +3%";
        }
        return "Speed +3%";
    }
    if (cls == 1 && n == 3) {
        if (language == 1) {
            return "Атака лечит +0.1 HP";
        }
        return "Attack heals +0.1 HP";
    }
    if (cls == 2 && n == 1) {
        if (language == 1) {
            return "Тик яда +0.15 урона";
        }
        return "Poison tick +0.15 damage";
    }
    if (cls == 2 && n == 2) {
        if (language == 1) {
            return "Тик яда +0.15 урона (x2)";
        }
        return "Poison tick +0.15 damage (x2)";
    }
    if (cls == 2 && n == 3) {
        if (language == 1) {
            return "Яд стакается, снежинка наносит урон";
        }
        return "Poison stacks, snowflake deals damage";
    }
    if (cls == 3 && n == 1) {
        if (language == 1) {
            return "Турель +2 HP";
        }
        return "Turret +2 HP";
    }
    if (cls == 3 && n == 2) {
        if (language == 1) {
            return "Спавн турели лечит +0.5 HP";
        }
        return "Placing a turret heals +0.5 HP";
    }
    if (cls == 3 && n == 3) {
        if (language == 1) {
            return "Луч турели лечит +0.1 HP/сек";
        }
        return "Turret beam heals +0.1 HP/sec";
    }
    if (cls == 4 && n == 1) {
        if (language == 1) {
            return "Захват: откат -2 с";
        }
        return "Grab: cooldown -2 s";
    }
    if (cls == 4 && n == 2) {
        if (language == 1) {
            return "Зона захвата +15%";
        }
        return "Grab zone +15%";
    }
    if (cls == 4 && n == 3) {
        if (language == 1) {
            return "Бросок дальше +40%, оглушение +0.3 с";
        }
        return "Throw +40% further, stun +0.3 s";
    }
    if (cls == 0 && n == 1) {
        if (language == 1) {
            return "Урон атаки +5%";
        }
        return "Attack damage +5%";
    }
    if (cls == 0 && n == 2) {
        if (language == 1) {
            return "Скорость +2%";
        }
        return "Speed +2%";
    }
    if (language == 1) {
        return "Атака лечит +0.05 HP";
    }
    return "Attack heals +0.05 HP";
}

static const char *tr_cups_unit(void) {
    if (language == 1) {
        return "кубков";
    }
    return "cups";
}

static const char *tr_candies_unit(void) {
    if (language == 1) {
        return "конфет";
    }
    return "candies";
}

static const char *tr_level_locked(void) {
    if (language == 1) {
        return "Купите предыдущий уровень";
    }
    return "Buy the previous level";
}

static const char *tr_level_need_cups(void) {
    if (language == 1) {
        return "Не хватает кубков для уровня";
    }
    return "Not enough cups for this level";
}

static const char *tr_cups(void) {
    if (language == 1) {
        return "Кубки";
    }
    return "Cups";
}

static const char *tr_buy_for(double cost, const char *currency) {
    const char *out = "";
    if (language == 1) {
        out = ds_concat(ds_concat(ds_concat("", ds_num_to_string((double)(cost))), " "), currency);
    } else {
        out = ds_concat(ds_concat(ds_concat("", ds_num_to_string((double)(cost))), " "), currency);
    }
    return out;
}

static const char *tr_class_need_cups(void) {
    if (language == 1) {
        return "Не хватает кубков";
    }
    return "Not enough cups";
}

static const char *tr_class_need_candies(void) {
    if (language == 1) {
        return "Не хватает леденцов";
    }
    return "Not enough candies";
}

static const char *tr_skins(void) {
    if (language == 1) {
        return "Скины";
    }
    return "Skins";
}

static const char *tr_skin_normal(void) {
    if (language == 1) {
        return "Обычный";
    }
    return "Normal";
}

static const char *tr_skin_zombie(void) {
    if (language == 1) {
        return "Зомби";
    }
    return "Zombie";
}

static const char *tr_skin_elf(void) {
    if (language == 1) {
        return "Эльф";
    }
    return "Elf";
}

static const char *tr_skin_elf_hint(void) {
    if (language == 1) {
        return "Подарок буКа";
    }
    return "ebuC's gift";
}

static const char *tr_skin_legends_hint(void) {
    if (language == 1) {
        return "Легенды не умирают...";
    }
    return "Legends never die...";
}

static const char *tr_skins_need_legends(void) {
    if (language == 1) {
        return "Скин «Зомби»: достижение «Легенды не умирают...»";
    }
    return "Zombie skin: \"Legends never die...\" achievement";
}

static const char *tr_cups_reward(double amount) {
    const char *out = "";
    if (language == 1) {
        out = ds_concat(ds_concat("+", ds_num_to_string((double)(amount))), " кубков");
    } else {
        out = ds_concat(ds_concat("+", ds_num_to_string((double)(amount))), " Cups");
    }
    return out;
}

/* Shown instead of the reward line when the battle was played with any cheat
 * switched on, even if it was switched straight back off. */
static const char *tr_cups_cheated(void) {
    if (language == 1) {
        return "Читы: кубки не начисляются";
    }
    return "Cheats used: no Cups";
}

/* Shown instead of the deduction line when a lost battle was played with any
 * admin panel switch enabled: cups are neither awarded nor taken away. */
static const char *tr_cups_admin(void) {
    if (language == 1) {
        return "Читы: кубки не снимаются";
    }
    return "Cheats used: no Cups lost";
}

static const char *tr_cups_lose(double amount) {
    const char *out = "";
    if (language == 1) {
        out = ds_concat(ds_concat("", ds_num_to_string((double)(amount))), " кубков");
    } else {
        out = ds_concat(ds_concat("", ds_num_to_string((double)(amount))), " Cups");
    }
    return out;
}

static const char *tr_banned(void) {
    if (language == 1) {
        return "Вы забанены";
    }
    return "You are banned";
}

static const char *tr_promo(void) {
    if (language == 1) {
        return "Промокод";
    }
    return "Promo code";
}

static const char *tr_promo_title(void) {
    if (language == 1) {
        return "Промокод";
    }
    return "Promo code";
}

static const char *tr_promo_need_login(void) {
    if (language == 1) {
        return "Войди, чтобы получить свой код";
    }
    return "Log in to get your own code";
}

static const char *tr_promo_enter_ph(void) {
    if (language == 1) {
        return "Введи промокод...";
    }
    return "Enter a promo code...";
}

static const char *tr_promo_submit(void) {
    if (language == 1) {
        return "Активировать";
    }
    return "Redeem";
}

static const char *tr_promo_need_code(void) {
    if (language == 1) {
        return "Введи код";
    }
    return "Enter a code";
}

static const char *tr_promo_bad(void) {
    if (language == 1) {
        return "Неверный код";
    }
    return "Invalid code";
}

static const char *tr_promo_used_msg(void) {
    if (language == 1) {
        return "Промокод уже активирован";
    }
    return "Promo code already redeemed";
}

static const char *tr_promo_success_2027(void) {
    if (language == 1) {
        return ds_concat(ds_concat("+", ds_num_to_string((double)(promo_2027_cups))), " кубков!");
    }
    return ds_concat(ds_concat("+", ds_num_to_string((double)(promo_2027_cups))), " cups!");
}
