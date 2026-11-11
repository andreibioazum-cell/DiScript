static Player *player_new(void) {
    Player *self = (Player *)calloc(1, sizeof(*self));
    if (!self) {
        ds_runtime_error("out of memory: Player");
        return NULL;
    }
    self->x = 0;
    self->y = 0;
    self->size = 30;
    self->angle = 0;
    self->hp = 20;
    self->max_hp = 20;
    return self;
}

static void player_free(Player *self) {
    free(self);
}

static Enemy *enemy_new(void) {
    Enemy *self = (Enemy *)calloc(1, sizeof(*self));
    if (!self) {
        ds_runtime_error("out of memory: Enemy");
        return NULL;
    }
    self->x = 0;
    self->y = 0;
    self->size = 25;
    self->hp = 20;
    self->max_hp = 20;
    self->angle = 0;
    self->state = 0;
    self->state_time = 0;
    self->cooldown = 0;
    self->hit = 0;
    self->strafe = 1;
    self->dodge_cd = 0;
    self->think = 0;
    self->px = 0;
    self->py = 0;
    self->pvx = 0;
    self->pvy = 0;
    self->seen = 0;
    self->flank = 0;
    self->mvx = 0;
    self->mvy = 0;
    self->svx = 0;
    self->svy = 0;
    self->freeze = 0;
    self->freeze_slow = 1;
    self->poison = 0;
    self->poison_tick = 0;
    self->poison_damage = 0;
    self->stun = 0;
    self->react = 0;
    self->speed_var = 1;
    self->aim_err = 0;
    self->hesit = 0;
    return self;
}

static void enemy_free(Enemy *self) {
    free(self);
}

static Punch *punch_new(void) {
    Punch *self = (Punch *)calloc(1, sizeof(*self));
    if (!self) {
        ds_runtime_error("out of memory: Punch");
        return NULL;
    }
    self->x = 0;
    self->y = 0;
    self->dx = 0;
    self->dy = 0;
    self->active = 0;
    self->shot = 0;
    self->hit = 0;
    return self;
}

static void punch_free(Punch *self) {
    free(self);
}

static Gift *gift_new(void) {
    Gift *self = (Gift *)calloc(1, sizeof(*self));
    if (!self) {
        ds_runtime_error("out of memory: Gift");
        return NULL;
    }
    self->x = 0;
    self->y = 0;
    self->dx = 0;
    self->dy = 0;
    self->active = 0;
    self->t = 0;
    self->shot = 0;
    self->pierce = 0;
    return self;
}

static void gift_free(Gift *self) {
    free(self);
}
