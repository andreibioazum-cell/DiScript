static void load_textures(void) {
    grass_tex_ok = png_load(GRASS);
    ordinary_tex_ok = png_load(ORDINARY_TEX);
    ordinary_punch_tex_ok = png_load(PUNCH_TEX);
    azum_tex_ok = png_load(AZUM_TEX);
    azum_punch_tex_ok = png_load(AZUM_PUNCH_TEX);
    azum_zombie_tex_ok = png_load(AZUM_ZOMBIE_TEX);
    azum_zombie_punch_tex_ok = png_load(AZUM_ZOMBIE_PUNCH_TEX);
    santa_tex_ok = png_load(SANTA_TEX);
    santa_punch_tex_ok = png_load(SANTA_PUNCH_TEX);
    ebuc_tex_ok = png_load(EBUC_TEX);
    ebuc_punch_tex_ok = png_load(EBUC_PUNCH_TEX);
    ebuc_elf_tex_ok = png_load(EBUC_ELF_TEX);
    ebuc_elf_punch_tex_ok = png_load(EBUC_ELF_PUNCH_TEX);
    png_load(DESPENSER_TEX);
    png_load(EBUC_ELF_DESPENSER_TEX);
    candy_tex_ok = png_load(CANDY_TEX);
    png_load(EVENT_COLUMNS_TEX);
    png_load(SNOWFLAKE_TEX);
    tex_reload_t = 0;
    tex_reload_left = 60;
}

static double flag_live(double flag, const char *name) {
    if (flag != 1) {
        return flag;
    }
    if (tex_ready(name) == 1) {
        return 1;
    }
    return 0;
}

static void recheck_texture_flags(void) {
    grass_tex_ok = flag_live(grass_tex_ok, GRASS);
    ordinary_tex_ok = flag_live(ordinary_tex_ok, ORDINARY_TEX);
    ordinary_punch_tex_ok = flag_live(ordinary_punch_tex_ok, PUNCH_TEX);
    azum_tex_ok = flag_live(azum_tex_ok, AZUM_TEX);
    azum_punch_tex_ok = flag_live(azum_punch_tex_ok, AZUM_PUNCH_TEX);
    azum_zombie_tex_ok = flag_live(azum_zombie_tex_ok, AZUM_ZOMBIE_TEX);
    azum_zombie_punch_tex_ok = flag_live(azum_zombie_punch_tex_ok, AZUM_ZOMBIE_PUNCH_TEX);
    santa_tex_ok = flag_live(santa_tex_ok, SANTA_TEX);
    santa_punch_tex_ok = flag_live(santa_punch_tex_ok, SANTA_PUNCH_TEX);
    ebuc_tex_ok = flag_live(ebuc_tex_ok, EBUC_TEX);
    ebuc_punch_tex_ok = flag_live(ebuc_punch_tex_ok, EBUC_PUNCH_TEX);
    ebuc_elf_tex_ok = flag_live(ebuc_elf_tex_ok, EBUC_ELF_TEX);
    ebuc_elf_punch_tex_ok = flag_live(ebuc_elf_punch_tex_ok, EBUC_ELF_PUNCH_TEX);
    candy_tex_ok = flag_live(candy_tex_ok, CANDY_TEX);
    if (winter_theme == 1) {
        snow_tex_ok = flag_live(snow_tex_ok, SNOW_TEX);
    }
}

static double textures_all_ok(void) {
    if (grass_tex_ok == 0 || ordinary_tex_ok == 0 || santa_tex_ok == 0 || ebuc_tex_ok == 0) {
        return 0;
    }
    if (ordinary_punch_tex_ok == 0 || azum_tex_ok == 0 || azum_punch_tex_ok == 0) {
        return 0;
    }
    if (azum_zombie_tex_ok == 0 || azum_zombie_punch_tex_ok == 0) {
        return 0;
    }
    if (santa_punch_tex_ok == 0 || ebuc_punch_tex_ok == 0 || candy_tex_ok == 0) {
        return 0;
    }
    if (winter_theme == 1 && snow_tex_ok == 0) {
        return 0;
    }
    return 1;
}

static void reload_textures(void) {
    if (tex_reload_t > 0) {
        tex_reload_t = tex_reload_t - dt;
        return;
    }
    recheck_texture_flags();
    if (tex_reload_left <= 0 && textures_all_ok() == 1) {
        tex_reload_t = tex_reload_slow;
        return;
    }
    if (tex_reload_left > 0) {
        tex_reload_t = 0.5;
        tex_reload_left = tex_reload_left - 1;
    } else {
        tex_reload_t = tex_reload_slow;
    }
    if (grass_tex_ok == 0) {
        grass_tex_ok = png_load(GRASS);
    }
    if (ordinary_tex_ok == 0) {
        ordinary_tex_ok = png_load(ORDINARY_TEX);
    }
    if (santa_tex_ok == 0) {
        santa_tex_ok = png_load(SANTA_TEX);
    }
    if (ebuc_tex_ok == 0) {
        ebuc_tex_ok = png_load(EBUC_TEX);
    }
    if (ordinary_punch_tex_ok == 0) {
        ordinary_punch_tex_ok = png_load(PUNCH_TEX);
    }
    if (azum_tex_ok == 0) {
        azum_tex_ok = png_load(AZUM_TEX);
    }
    if (azum_punch_tex_ok == 0) {
        azum_punch_tex_ok = png_load(AZUM_PUNCH_TEX);
    }
    if (azum_zombie_tex_ok == 0) {
        azum_zombie_tex_ok = png_load(AZUM_ZOMBIE_TEX);
    }
    if (azum_zombie_punch_tex_ok == 0) {
        azum_zombie_punch_tex_ok = png_load(AZUM_ZOMBIE_PUNCH_TEX);
    }
    if (santa_punch_tex_ok == 0) {
        santa_punch_tex_ok = png_load(SANTA_PUNCH_TEX);
    }
    if (ebuc_punch_tex_ok == 0) {
        ebuc_punch_tex_ok = png_load(EBUC_PUNCH_TEX);
    }
    if (ebuc_elf_tex_ok == 0) {
        ebuc_elf_tex_ok = png_load(EBUC_ELF_TEX);
    }
    if (ebuc_elf_punch_tex_ok == 0) {
        ebuc_elf_punch_tex_ok = png_load(EBUC_ELF_PUNCH_TEX);
    }
    if (tex_ready(DESPENSER_TEX) == 0) {
        png_load(DESPENSER_TEX);
    }
    if (tex_ready(EBUC_ELF_DESPENSER_TEX) == 0) {
        png_load(EBUC_ELF_DESPENSER_TEX);
    }
    if (candy_tex_ok == 0) {
        candy_tex_ok = png_load(CANDY_TEX);
    }
    if (winter_theme == 1 && snow_tex_ok == 0) {
        snow_tex_ok = png_load(SNOW_TEX);
    }
    if (textures_all_ok() == 1) {
        tex_reload_left = 0;
    }
}

static void apply_winter_theme(void) {
    if (winter_theme == 1) {
        snow_tex_ok = png_load(SNOW_TEX);
        return;
    }
    snow_tex_ok = 0;
}
