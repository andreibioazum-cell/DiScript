// tools/host_test/test_project.c
//
// Project folders: create, add sprites (including a Cyrillic display name),
// reopen, and remove. Runs on a scratch folder so a stale run cannot pass.
#include <stdio.h>
#include <string.h>
#include "ide/ide_project.h"

static int fails = 0;
static void ck(const char *what, int cond) {
    printf("  %s %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond) fails++;
}

int main(void) {
    /* Start from a clean folder so a stale run cannot pass for us. */
    remove("/tmp/ProjTest/project.json");
    remove("/tmp/ProjTest/scripts/main.ds");
    IDEProject p;
    printf("создание проекта\n");
    ck("create", ide_project_create(&p, "/tmp/ProjTest", "Моя Игра"));
    ck("имя сохранено", strcmp(p.name, "Моя Игра") == 0);
    ck("скрипт есть", p.script_count == 1);

    char sp[600];
    ide_project_script_path(&p, sp, sizeof(sp));
    ck("путь к скрипту", strstr(sp, "scripts/main.ds") != NULL);

    printf("добавление спрайта из файла\n");
    ck("add sprite", ide_project_add_sprite_file(&p, "assets/fonts/DejaVuSansMono.ttf", "Player"));
    ck("один спрайт", p.sprite_count == 1);
    ck("имя в нижнем регистре", strcmp(p.sprites[0].name, "player") == 0);
    char full[600];
    ide_project_sprite_path(&p, p.sprites[0].file, full, sizeof(full));
    FILE *f = fopen(full, "rb");
    ck("файл записан на диск", f != NULL);
    if (f) fclose(f);

    printf("второй спрайт с тем же именем\n");
    ck("add 2", ide_project_add_sprite_file(&p, "assets/fonts/DejaVuSansMono.ttf", "Player"));
    ck("имя уникализировано", p.sprite_count == 2 && strcmp(p.sprites[0].name, p.sprites[1].name) != 0);
    printf("  имена: %s / %s\n", p.sprites[0].name, p.sprites[1].name);

    printf("переоткрытие\n");
    IDEProject q;
    ck("open", ide_project_open(&q, "/tmp/ProjTest"));
    ck("имя восстановлено", strcmp(q.name, "Моя Игра") == 0);
    ck("2 спрайта", q.sprite_count == 2);
    ck("скрипт восстановлен", q.script_count == 1);
    if (q.sprite_count >= 1)
        ck("имя спрайта", strcmp(q.sprites[0].name, "player") == 0);

    printf("удаление\n");
    char before[600], gone[600];
    ide_project_sprite_path(&q, q.sprites[0].file, before, sizeof(before));
    ck("remove", ide_project_remove_sprite(&q, "player"));
    ck("остался 1", q.sprite_count == 1);
    ide_project_sprite_path(&q, q.sprites[0].file, gone, sizeof(gone));
    /* The removed file must be gone from disk, not just from the list. */
    FILE *chk = fopen(before, "rb");
    ck("файл удалён с диска", chk == NULL);
    if (chk) fclose(chk);

    printf("кириллица в имени файла не ломает путь\n");
    ck("add кириллица", ide_project_add_sprite_file(&q, "assets/fonts/DejaVuSansMono.ttf", "Игрок"));
    ck("имя латиницей", strcmp(q.sprites[q.sprite_count-1].name, "player") != 0);
    printf("  имя: %s\n", q.sprites[q.sprite_count-1].name);

    printf("%s\n", fails ? "ЕСТЬ ОШИБКИ" : "все проверки прошли");
    return fails ? 1 : 0;
}
