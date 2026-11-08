/* In-game legal documents summary and personal data consent version. */

#define LEGAL_DOCS_VERSION 2
#define LEGAL_OPERATOR_RU "Дима Густенёв"
#define LEGAL_OPERATOR_EN "Dima Gustenyov"
/* Contact endpoint for personal data requests. */
#define LEGAL_CONTACT "github.com/andreibioazum-cell/C-Larp/issues"
#define LEGAL_PAGES 9
#define LEGAL_LINES 10
#define LEGAL_PAGE_CONSENT 5
#define LEGAL_PAGE_RIGHTS 6

static double docs_open;
/* Account screen mode (account.inc), 0 - closed; it sits over the documents. */
static double acct_open;

static const char *const legal_ru[LEGAL_PAGES][LEGAL_LINES + 1] = {
    {"Правовая информация", "Игра «Cubic Battle 4». Возраст: 12+", "Правообладатель и оператор данных:",
     LEGAL_OPERATOR_RU, "Обращения: " LEGAL_CONTACT, "", "Игра бесплатна, покупок за деньги нет;",
     "предоставляется «как есть», без гарантий.", "Пользуясь игрой, вы принимаете",
     "Пользовательское соглашение (стр. 2-3).", ""},
    {"Пользовательское соглашение", "1. Возрастная категория игры: 12+.", "   До 14 лет играйте с разрешения",
     "   родителей (законных представителей).", "2. В чате и в нике запрещены мат,",
     "   оскорбления, угрозы, травля, реклама,", "   ссылки, порнография, пропаганда насилия,",
     "   наркотиков, самоубийства, экстремизма", "   и всё, что запрещено законами РФ.",
     "3. Читы, взлом и обход защиты запрещены.", ""},
    {"Пользовательское соглашение (2)", "4. За нарушения аккаунт блокируется,",
     "   сообщения и ник могут быть удалены.", "5. Кубки, леденцы и персонажи - игровые",
     "   условности, не имущество и не деньги.", "6. За свои сообщения отвечает их автор.",
     "7. Игра может меняться или закрыться;", "   ответственность правообладателя",
     "   ограничена в пределах, допустимых законом.", "8. Применяется право Российской Федерации.",
     "9. Новая редакция действует с публикации."},
    {"Персональные данные", "Что обрабатываем: ник, пароль (сервис",
     "входа хранит только его хеш), прогресс,", "настройки, промокод, сообщения чата,",
     "отметку о согласии.", "Не собираем: имя, телефон, почту,", "геолокацию, контакты, номер устройства.",
     "Нет рекламы, аналитики и трекеров.", "Цели: аккаунт, сохранение прогресса,",
     "таблица лидеров, онлайн-бои и чат.", "Основание: ваше согласие (ст. 6, 9 152-ФЗ)."},
    {"Хранение и защита", "Данные передаются только по HTTPS и", "хранятся в Google Firebase; серверы могут",
     "находиться за пределами России", "(трансграничная передача, ст. 12 152-ФЗ).",
     "Срок: пока существует аккаунт; чат", "хранит лишь последние сообщения комнаты.",
     "Данные не продаются и не передаются", "третьим лицам, кроме случаев по закону.",
     "Пароль, PIN родителя и режим возраста", "хранятся только на устройстве."},
    {"Согласие на обработку данных", "Отмечая галочку при входе, вы свободно,",
     "своей волей и в своём интересе даёте", "оператору согласие на обработку данных",
     "со стр. 4: сбор, запись, хранение,", "уточнение, использование, передачу",
     "сервису Firebase, удаление - для целей", "со стр. 4, до отзыва согласия.",
     "До 14 лет согласие даёт родитель.", "", ""},
    {"Ваши права", "Вы вправе узнать, какие данные о вас", "хранятся, потребовать исправить,",
     "заблокировать или удалить их и в любой", "момент отозвать согласие (ст. 14, 21).",
     "Отзыв согласия - это удаление аккаунта:", "кнопка ниже стирает аккаунт, профиль",
     "в облаке и данные на этом устройстве.", "Ответ на обращение - до 10 рабочих дней.",
     "Жалобу можно подать в Роскомнадзор.", ""},
    {"Лицензия и авторские права", "Кроме сторонних компонентов (стр. 9),", "код, графика, музыка, тексты и персонажи",
     "принадлежат правообладателю.", "Копировать, распространять, продавать игру,", "её код и материалы, а также",
     "декомпилировать их и обходить защиту", "запрещено. Текстурпаки делаете вы и", "отвечаете за них сами:",
     "в паках нельзя распространять материалы игры.", "Сторонние лицензии указаны отдельно."},
    {"Музыка диско: автор и лицензия", "Disco Medusae - Kevin MacLeod", "Автор: Kevin MacLeod (incompetech.com)",
     "Лицензия: Creative Commons Attribution 4.0", "https://creativecommons.org/licenses/by/4.0/",
     "Источник: incompetech.com/music/royalty-free/", "index.html?isrc=USUAN1500041",
     "Изменения: перекодировано в MP3 32 kbps,", "стерео, 22050 Гц; темп и высота не менялись.",
     "Это опубликованная запись, не генерация.", "Полный текст: assets/audio/licenses/."},
};

static const char *const legal_en[LEGAL_PAGES][LEGAL_LINES + 1] = {
    {"Legal information", "\"Cubic Battle 4\". Age rating: 12+", "Rights holder and data operator:",
     LEGAL_OPERATOR_EN, "Requests: " LEGAL_CONTACT, "", "The game is free, nothing is sold for money;",
     "it is provided \"as is\", without warranty.", "By playing you accept the",
     "Terms of Use (pages 2-3).", ""},
    {"Terms of Use", "1. Age rating: 12+.", "   Under 14, play with the permission of",
     "   your parents (legal guardians).", "2. Chat and nicks must not contain swearing,",
     "   insults, threats, bullying, ads, links,", "   pornography, promotion of violence,",
     "   drugs, suicide, extremism or anything", "   prohibited by the laws of Russia.",
     "3. Cheats, hacking and bypassing are banned.", ""},
    {"Terms of Use (2)", "4. Violations lead to an account ban;", "   messages and nicks may be removed.",
     "5. Cups, candies and characters are game", "   items, not property and not money.",
     "6. Authors are responsible for their messages.", "7. The game may change or shut down;",
     "   the rights holder's liability is limited", "   to the extent permitted by law.",
     "8. The law of the Russian Federation applies.", "9. A new edition applies once published."},
    {"Personal data", "We process: nick, password (the sign-in", "service keeps only its hash), progress,",
     "settings, promo code, chat messages,", "the consent record.", "We do not collect: name, phone, email,",
     "location, contacts, device identifiers.", "No ads, no analytics, no trackers.",
     "Purposes: account, saving progress,", "leaderboard, online battles and chat.",
     "Legal basis: your consent (152-FZ, art. 6, 9)."},
    {"Storage and security", "Data travels over HTTPS only and is", "stored in Google Firebase; servers may",
     "be located outside Russia", "(cross-border transfer, 152-FZ art. 12).",
     "Kept while the account exists; the chat", "keeps only the latest room messages.",
     "Data is never sold or shared with third", "parties, except where the law requires.",
     "The password, parent PIN and age mode", "are kept on this device only."},
    {"Consent to data processing", "By ticking the box at sign-in you freely,",
     "of your own will and in your interest give", "the operator consent to process the data",
     "from page 4: collect, record, store,", "update, use, transfer to the Firebase",
     "service and delete it - for the purposes", "on page 4, until you withdraw consent.",
     "Under 14, a parent gives the consent.", "", ""},
    {"Your rights", "You may learn what data about you is", "stored, demand that it is corrected,",
     "blocked or deleted, and withdraw your", "consent at any time (152-FZ art. 14, 21).",
     "Withdrawing consent deletes the account:", "the button below erases the account, the",
     "cloud profile and data on this device.", "Requests are answered within 10 working days.",
     "You may complain to Roskomnadzor.", ""},
    {"License and copyright", "Except third-party components (page 9),", "code, graphics, music, texts and characters",
     "belong to the rights holder.", "Copying, distributing or selling the game,", "its code and materials, decompiling them",
     "or bypassing the protection is prohibited.", "Texture packs are made by you and are", "your responsibility:",
     "packs may not redistribute the game materials.", "Third-party licenses are listed separately."},
    {"Disco music: author and license", "Disco Medusae - Kevin MacLeod", "Author: Kevin MacLeod (incompetech.com)",
     "License: Creative Commons Attribution 4.0", "https://creativecommons.org/licenses/by/4.0/",
     "Source: incompetech.com/music/royalty-free/", "index.html?isrc=USUAN1500041",
     "Changes: re-encoded as MP3 32 kbps,", "stereo, 22050 Hz; no tempo or pitch changes.",
     "A published recording, not generated music.", "Full license: assets/audio/licenses/."},
};

static const char *legal_line(double page, double line) {
    int p = (int)page, l = (int)line;
    if (p < 0 || p >= LEGAL_PAGES || l < 0 || l > LEGAL_LINES) {
        return "";
    }
    return language == 1 ? legal_ru[p][l] : legal_en[p][l];
}

static const char *tr_legal(void) {
    if (language == 1) {
        return "Правовая информация";
    }
    return "Legal information";
}

static const char *tr_legal_prev(void) {
    if (language == 1) {
        return "Назад";
    }
    return "Previous";
}

static const char *tr_legal_next(void) {
    if (language == 1) {
        return "Далее";
    }
    return "Next";
}

static const char *tr_legal_close(void) {
    if (language == 1) {
        return "Закрыть";
    }
    return "Close";
}

static const char *tr_legal_docs(void) {
    if (language == 1) {
        return "Документы";
    }
    return "Documents";
}

static const char *tr_consent_given(void) {
    if (language == 1) {
        return "Ваше согласие: дано";
    }
    return "Your consent: given";
}

static const char *tr_consent_missing(void) {
    if (language == 1) {
        return "Ваше согласие: не дано";
    }
    return "Your consent: not given";
}

static const char *tr_consent_box(void) {
    if (language == 1) {
        return "Даю согласие на обработку персональных данных";
    }
    return "I consent to the processing of my personal data";
}

static const char *tr_consent_more(void) {
    if (language == 1) {
        return "Соглашение и политика данных";
    }
    return "Terms and data policy";
}

static const char *tr_consent_needed(void) {
    if (language == 1) {
        return "Отметьте согласие на обработку данных";
    }
    return "Tick the data processing consent box";
}

static const char *tr_consent_agree(void) {
    if (language == 1) {
        return "Даю согласие";
    }
    return "I consent";
}

static const char *tr_consent_refuse(void) {
    if (language == 1) {
        return "Отказаться и выйти";
    }
    return "Refuse and sign out";
}

static const char *tr_delete_account(void) {
    if (language == 1) {
        return "Удалить аккаунт и данные";
    }
    return "Delete account and data";
}

static const char *tr_delete_busy(void) {
    if (language == 1) {
        return "Удаляем...";
    }
    return "Deleting...";
}

static const char *tr_delete_done(void) {
    if (language == 1) {
        return "Аккаунт и данные удалены";
    }
    return "Account and data deleted";
}

static const char *tr_delete_need_login(void) {
    if (language == 1) {
        return "Чтобы удалить аккаунт, сначала войдите в него.";
    }
    return "Sign in first to delete your account.";
}

static const char *tr_age_mark(void) { return "12+"; }

static const char *tr_bad_nick_rules(void) {
    if (language == 1) {
        return "Ник нарушает правила игры";
    }
    return "This nick breaks the game rules";
}
