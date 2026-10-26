

static const char *tr_level_need_class(void) {
    if (language == 1) {
        return "Сначала купите персонажа";
    }
    return "Buy the character first";
}

/* Old rule sets used to overcharge for levels and hand out characters that
 * were never bought. Such a price never reaches the wallet now. */
static const char *tr_level_price_guard(void) {
    if (language == 1) {
        return "Старые правила: цена уровня не совпадает, покупка отменена";
    }
    return "Old rules: the level price is wrong, purchase cancelled";
}

static const char *tr_class_price_guard(void) {
    if (language == 1) {
        return "Старые правила: цену персонажа проверить нельзя";
    }
    return "Old rules: the character price cannot be checked";
}
