#include "s3e_host_internal.h"

#include <math.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

enum {
    SDL_INIT_JOYSTICK = 0x00000200u,
    SDL_INIT_GAMECONTROLLER = 0x00002000u,
};

enum {
    SDL_BUTTON_A = 0,
    SDL_BUTTON_B = 1,
    SDL_BUTTON_X = 2,
    SDL_BUTTON_Y = 3,
    SDL_BUTTON_BACK = 4,
    SDL_BUTTON_START = 6,
    SDL_BUTTON_LEFTSTICK = 7,
    SDL_BUTTON_RIGHTSTICK = 8,
    SDL_BUTTON_LEFTSHOULDER = 9,
    SDL_BUTTON_RIGHTSHOULDER = 10,
    SDL_BUTTON_DPAD_UP = 11,
    SDL_BUTTON_DPAD_DOWN = 12,
    SDL_BUTTON_DPAD_LEFT = 13,
    SDL_BUTTON_DPAD_RIGHT = 14,
};

enum {
    SDL_AXIS_LEFTX = 0,
    SDL_AXIS_LEFTY = 1,
    SDL_AXIS_RIGHTX = 2,
    SDL_AXIS_RIGHTY = 3,
    SDL_AXIS_TRIGGERLEFT = 4,
    SDL_AXIS_TRIGGERRIGHT = 5,
};

enum {
    SDL_HAT_UP = 0x01,
    SDL_HAT_RIGHT = 0x02,
    SDL_HAT_DOWN = 0x04,
    SDL_HAT_LEFT = 0x08,
};

enum {
    POINTER_STATE_UP = 0,
    POINTER_STATE_DOWN = 1,
    POINTER_STATE_PRESSED = 2,
    POINTER_STATE_RELEASED = 4,
};

enum {
    KEY_STATE_DOWN = 1,
    KEY_STATE_PRESSED = 2,
    KEY_STATE_RELEASED = 4,
};

/* NFS Shift (NextOS): teclas s3e que o proprio jogo escuta, direto do ICF
   embutido do modulo ([game] KeyControl* — heranca do Xperia Play):
     MenuUp=10 MenuDown=12 MenuLeft/SteerLeft=9 MenuRight/SteerRight=11
     MenuSelect=8(Space) GearUp=39(Q) GearDown=23(A) Brake=48(Z)
     Drift=46(X) Nitro=41(S) SwitchCamera=44(V) Pause=38(P)
   Aceleracao e' automatica (padrao mobile); nao ha tecla de acelerar. */
enum {
    SHIFT_KEY_ESC = 1,
    SHIFT_KEY_MENU_SELECT = 8,
    SHIFT_KEY_LEFT = 9,
    SHIFT_KEY_UP = 10,
    SHIFT_KEY_RIGHT = 11,
    SHIFT_KEY_DOWN = 12,
    SHIFT_KEY_GEAR_DOWN = 23,
    SHIFT_KEY_PAUSE = 38,
    SHIFT_KEY_GEAR_UP = 39,
    SHIFT_KEY_NITRO = 41,
    SHIFT_KEY_CAMERA = 44,
    SHIFT_KEY_DRIFT = 46,
    SHIFT_KEY_BRAKE = 48,
    S3E_KEY_ABS_GAME_A = 200,
    S3E_KEY_ABS_GAME_B = 201,
    S3E_KEY_ABS_GAME_C = 202,
    S3E_KEY_ABS_GAME_D = 203,
    S3E_KEY_ABS_UP = 204,
    S3E_KEY_ABS_DOWN = 205,
    S3E_KEY_ABS_LEFT = 206,
    S3E_KEY_ABS_RIGHT = 207,
    S3E_KEY_ABS_OK = 208,
    S3E_KEY_ABS_ASK = 209,
    S3E_KEY_ABS_BSK = 210,
};

enum {
    KEYBOARD_KEY_COUNT = 256,
    TOUCHPAD_COUNT = 2,
    AXIS_DEADZONE = 9000,
    XPERIA_AXIS_DEADZONE = 6000,
    TRIGGER_THRESHOLD = 16384,
};

enum {
    S3E_TOUCHPAD_RELEASED = 0,
    S3E_TOUCHPAD_PRESSED = 1,
};

struct key_map {
    const uint32_t *keys;
    size_t key_count;
};

struct sdl_input_api {
    int (*InitSubSystem)(uint32_t flags);
    void (*QuitSubSystem)(uint32_t flags);
    int (*GameControllerAddMapping)(const char *mapping);
    int (*NumJoysticks)(void);
    int (*IsGameController)(int joystick_index);
    void *(*GameControllerOpen)(int joystick_index);
    void (*GameControllerClose)(void *gamecontroller);
    void *(*GameControllerGetJoystick)(void *gamecontroller);
    int (*JoystickNumHats)(void *joystick);
    uint8_t (*JoystickGetHat)(void *joystick, int hat);
    void (*GameControllerUpdate)(void);
    int16_t (*GameControllerGetAxis)(void *gamecontroller, int axis);
    uint8_t (*GameControllerGetButton)(void *gamecontroller, int button);
    const char *(*GetError)(void);
};

static const uint32_t KEY_SELECT[] = {SHIFT_KEY_MENU_SELECT};
static const uint32_t KEY_BACK[] = {SHIFT_KEY_ESC};
static const uint32_t KEY_DRIFT[] = {SHIFT_KEY_DRIFT};
static const uint32_t KEY_CAMERA[] = {SHIFT_KEY_CAMERA};
static const uint32_t KEY_GEAR_DOWN[] = {SHIFT_KEY_GEAR_DOWN};
static const uint32_t KEY_GEAR_UP[] = {SHIFT_KEY_GEAR_UP};
static const uint32_t KEY_BRAKE[] = {SHIFT_KEY_BRAKE};
static const uint32_t KEY_NITRO[] = {SHIFT_KEY_NITRO};
static const uint32_t KEY_PAUSE[] = {SHIFT_KEY_PAUSE};
static const uint32_t KEY_ARROW_UP[] = {SHIFT_KEY_UP};
static const uint32_t KEY_ARROW_DOWN[] = {SHIFT_KEY_DOWN};
static const uint32_t KEY_ARROW_LEFT[] = {SHIFT_KEY_LEFT};
static const uint32_t KEY_ARROW_RIGHT[] = {SHIFT_KEY_RIGHT};

#define KEY_MAP(keys) {keys, ARRAY_SIZE(keys)}

static const struct key_map KEYMAP_SELECT = KEY_MAP(KEY_SELECT);
static const struct key_map KEYMAP_BACK = KEY_MAP(KEY_BACK);
static const struct key_map KEYMAP_DRIFT = KEY_MAP(KEY_DRIFT);
static const struct key_map KEYMAP_CAMERA = KEY_MAP(KEY_CAMERA);
static const struct key_map KEYMAP_GEAR_DOWN = KEY_MAP(KEY_GEAR_DOWN);
static const struct key_map KEYMAP_GEAR_UP = KEY_MAP(KEY_GEAR_UP);
static const struct key_map KEYMAP_BRAKE = KEY_MAP(KEY_BRAKE);
static const struct key_map KEYMAP_NITRO = KEY_MAP(KEY_NITRO);
static const struct key_map KEYMAP_PAUSE = KEY_MAP(KEY_PAUSE);
static const struct key_map KEYMAP_ARROW_UP = KEY_MAP(KEY_ARROW_UP);
static const struct key_map KEYMAP_ARROW_DOWN = KEY_MAP(KEY_ARROW_DOWN);
static const struct key_map KEYMAP_ARROW_LEFT = KEY_MAP(KEY_ARROW_LEFT);
static const struct key_map KEYMAP_ARROW_RIGHT = KEY_MAP(KEY_ARROW_RIGHT);

#undef KEY_MAP

static void *g_sdl2;
static struct sdl_input_api g_sdl;
static void *g_controller;
static void *g_joystick;
static int g_sdl_tried;
static int g_input_pumping;
static int g_keyboard_update_active;
static int g_prev_select;
static int g_prev_a;
static uint64_t g_input_last_ms;
static uint8_t g_hat_mask;

static uint8_t g_keyboard_state[KEYBOARD_KEY_COUNT];

static int g_touchpad_active[TOUCHPAD_COUNT];
static int32_t g_touchpad_x[TOUCHPAD_COUNT];
static int32_t g_touchpad_y[TOUCHPAD_COUNT];
static uint8_t g_touchpad_state[TOUCHPAD_COUNT];

static int sdl_load_symbol(void **slot, const char *name) {
    *slot = dlsym(g_sdl2, name);
    return *slot != NULL;
}

static void sdl_load_optional_symbol(void **slot, const char *name) {
    *slot = dlsym(g_sdl2, name);
}

static void input_open(void) {
    if (g_sdl_tried) {
        return;
    }
    g_sdl_tried = 1;

    const char *names[] = {"libSDL2-2.0.so.0", "libSDL2.so", NULL};
    g_sdl2 = open_first(names);
    if (!g_sdl2) {
        return;
    }

    int ok = 1;
    ok &= sdl_load_symbol((void **)&g_sdl.InitSubSystem, "SDL_InitSubSystem");
    ok &= sdl_load_symbol((void **)&g_sdl.QuitSubSystem, "SDL_QuitSubSystem");
    ok &= sdl_load_symbol((void **)&g_sdl.GameControllerAddMapping, "SDL_GameControllerAddMapping");
    ok &= sdl_load_symbol((void **)&g_sdl.NumJoysticks, "SDL_NumJoysticks");
    ok &= sdl_load_symbol((void **)&g_sdl.IsGameController, "SDL_IsGameController");
    ok &= sdl_load_symbol((void **)&g_sdl.GameControllerOpen, "SDL_GameControllerOpen");
    ok &= sdl_load_symbol((void **)&g_sdl.GameControllerClose, "SDL_GameControllerClose");
    sdl_load_optional_symbol((void **)&g_sdl.GameControllerGetJoystick,
                             "SDL_GameControllerGetJoystick");
    sdl_load_optional_symbol((void **)&g_sdl.JoystickNumHats, "SDL_JoystickNumHats");
    sdl_load_optional_symbol((void **)&g_sdl.JoystickGetHat, "SDL_JoystickGetHat");
    ok &= sdl_load_symbol((void **)&g_sdl.GameControllerUpdate, "SDL_GameControllerUpdate");
    ok &= sdl_load_symbol((void **)&g_sdl.GameControllerGetAxis, "SDL_GameControllerGetAxis");
    ok &= sdl_load_symbol((void **)&g_sdl.GameControllerGetButton, "SDL_GameControllerGetButton");
    sdl_load_optional_symbol((void **)&g_sdl.GetError, "SDL_GetError");
    if (!ok || g_sdl.InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        if (!ok) {
            fprintf(stderr, "[input] SDL2 controller symbols unavailable\n");
        } else if (g_sdl.GetError) {
            const char *error = g_sdl.GetError();
            fprintf(stderr, "[input] SDL_InitSubSystem failed: %s\n",
                    error ? error : "unknown");
        }
        return;
    }

    const char *mapping = getenv("SDL_GAMECONTROLLERCONFIG");
    if (mapping && mapping[0]) {
        g_sdl.GameControllerAddMapping(mapping);
    }

    int count = g_sdl.NumJoysticks();
    int selected_index = -1;

    /* SIMS3_PAD_INDEX escolhe QUAL controle abrir. Existe para a prova
       automatizada de controle: o clone uinput nasce ao lado do pad fisico, e
       sem poder escolher o loader abriria o fisico e o teste nao provaria nada.
       Sem a variavel, o comportamento e' o de sempre (o primeiro que servir). */
    int desejado = -1;
    {
        const char *e = getenv("SIMS3_PAD_INDEX");
        if (e && e[0]) {
            desejado = atoi(e);
        }
    }
    for (int i = 0; i < count; ++i) {
        if (g_sdl.IsGameController(i)) {
            if (desejado >= 0 && i != desejado) {
                continue;
            }
            selected_index = i;
            break;
        }
    }
    fprintf(stderr, "[input] %d joystick(s); abrindo indice %d%s\n", count, selected_index,
            desejado >= 0 ? " (SIMS3_PAD_INDEX)" : "");

    if (selected_index >= 0) {
        g_controller = g_sdl.GameControllerOpen(selected_index);
        if (g_controller) {
            g_joystick =
                g_sdl.GameControllerGetJoystick ? g_sdl.GameControllerGetJoystick(g_controller) :
                                                  NULL;
        }
    }
}

static uint8_t input_current_hat_mask(void) {
    uint8_t mask = 0;
    if (!g_joystick || !g_sdl.JoystickNumHats || !g_sdl.JoystickGetHat) {
        return 0;
    }

    int count = g_sdl.JoystickNumHats(g_joystick);
    for (int i = 0; i < count; ++i) {
        mask |= g_sdl.JoystickGetHat(g_joystick, i);
    }
    return mask;
}

static int input_button(int button) {
    if (!g_controller || !g_sdl.GameControllerGetButton || button < 0) {
        return 0;
    }
    return g_sdl.GameControllerGetButton(g_controller, button) != 0;
}

static int32_t input_axis_raw(int axis) {
    if (!g_controller || !g_sdl.GameControllerGetAxis) {
        return 0;
    }
    return g_sdl.GameControllerGetAxis(g_controller, axis);
}

static int input_trigger(int axis) {
    return input_axis_raw(axis) > TRIGGER_THRESHOLD;
}

static int input_hat(uint8_t mask) {
    return (g_hat_mask & mask) != 0;
}

static int input_dpad_up(void) {
    return input_button(SDL_BUTTON_DPAD_UP) || input_hat(SDL_HAT_UP);
}

static int input_dpad_down(void) {
    return input_button(SDL_BUTTON_DPAD_DOWN) || input_hat(SDL_HAT_DOWN);
}

static int input_dpad_left(void) {
    return input_button(SDL_BUTTON_DPAD_LEFT) || input_hat(SDL_HAT_LEFT);
}

static int input_dpad_right(void) {
    return input_button(SDL_BUTTON_DPAD_RIGHT) || input_hat(SDL_HAT_RIGHT);
}

static int32_t input_axis_deadzone(int axis, int32_t deadzone) {
    int32_t value = input_axis_raw(axis);
    return value < -deadzone || value > deadzone ? value : 0;
}

static int32_t input_axis(int axis) {
    return input_axis_deadzone(axis, AXIS_DEADZONE);
}

static int32_t window_width(void) {
    return g_native_window.width > 0 ? (int32_t)g_native_window.width : 640;
}

static int32_t window_height(void) {
    return g_native_window.height > 0 ? (int32_t)g_native_window.height : 480;
}

static int32_t clamp_value(int32_t value, int32_t upper_exclusive) {
    if (value < 0) {
        return 0;
    }
    if (value >= upper_exclusive) {
        return upper_exclusive - 1;
    }
    return value;
}

static int32_t clamp_pointer_x(int32_t x) {
    return clamp_value(x, window_width());
}

static int32_t clamp_pointer_y(int32_t y) {
    return clamp_value(y, window_height());
}

static void pointer_dispatch(uint32_t id, void *event) {
    if (id >= ARRAY_SIZE(g_pointer_callbacks)) {
        return;
    }
    struct callback_slot *slot = &g_pointer_callbacks[id];
    if (!slot->callback) {
        return;
    }
    ((s3e_callback_fn)(uintptr_t)slot->callback)(event, slot->user_data);
}

static void touchpad_dispatch(uint32_t id, void *event) {
    if (id >= ARRAY_SIZE(g_touchpad_callbacks)) {
        return;
    }
    struct callback_slot *slot = &g_touchpad_callbacks[id];
    if (!slot->callback) {
        return;
    }
    ((s3e_callback_fn)(uintptr_t)slot->callback)(event, slot->user_data);
}

/* Pump SEM despacho: o present pode manter o estado SDL fresco, mas NAO pode
   consumir as bordas do ponteiro. A implementacao antiga chamava input_pump()
   com callbacks bloqueados: A/R2 mudavam g_pointer_down aqui e, quando o jogo
   chegava a s3ePointerUpdate, a borda ja tinha sumido. Atualizamos somente o
   dispositivo; o estado Marmalade e seus callbacks pertencem ao update nativo. */
void input_pump_no_dispatch(void) {
    input_open();
    if (g_controller && g_sdl.GameControllerUpdate) {
        g_sdl.GameControllerUpdate();
        g_hat_mask = input_current_hat_mask();
    }
}

static unsigned g_ptr_update_calls, g_ptr_getstate_calls, g_ptr_getxy_calls;

void s3e_pointer_poll_stats(void) {
    fprintf(stderr, "[ptr] polls: Update=%u GetState=%u GetX/Y=%u\n", g_ptr_update_calls,
            g_ptr_getstate_calls, g_ptr_getxy_calls);
    fflush(stderr);
}

static void pointer_set_down(int down) {
    if (down) {
        g_pointer_states[0] = g_pointer_down ? POINTER_STATE_DOWN : POINTER_STATE_PRESSED;
        g_pointer_down = 1;
    } else {
        g_pointer_states[0] = g_pointer_down ? POINTER_STATE_RELEASED : POINTER_STATE_UP;
        g_pointer_down = 0;
    }
}

static void pointer_clear_transitions(void) {
    for (size_t i = 0; i < sizeof(g_pointer_states); ++i) {
        if (g_pointer_states[i] == POINTER_STATE_PRESSED) {
            g_pointer_states[i] = POINTER_STATE_DOWN;
        } else if (g_pointer_states[i] == POINTER_STATE_RELEASED) {
            g_pointer_states[i] = POINTER_STATE_UP;
        }
    }
    for (size_t i = 0; i < ARRAY_SIZE(g_touchpad_state); ++i) {
        if (g_touchpad_state[i] == POINTER_STATE_PRESSED) {
            g_touchpad_state[i] = POINTER_STATE_DOWN;
        } else if (g_touchpad_state[i] == POINTER_STATE_RELEASED) {
            g_touchpad_state[i] = POINTER_STATE_UP;
        }
    }
}

static void pointer_dispatch_button(uint32_t button, int32_t pressed) {
    if (getenv("SIMS3_POINTER_LOG")) {
        fprintf(stderr, "[ptr] botao=%u apertado=%d em (%d,%d) cb0=%p cb2=%p\n", button, pressed,
                g_pointer_x, g_pointer_y, g_pointer_callbacks[0].callback,
                g_pointer_callbacks[2].callback);
        fflush(stderr);
    }
    struct s3e_pointer_button_event event = {
        .button = (int32_t)button,
        .pressed = pressed,
        .x = g_pointer_x,
        .y = g_pointer_y,
    };
    struct s3e_pointer_touch_event touch_event = {
        .touch_id = (int32_t)button,
        .pressed = pressed,
        .x = g_pointer_x,
        .y = g_pointer_y,
    };
    pointer_dispatch(0, &event);
    pointer_dispatch(2, &touch_event);
}

static void pointer_dispatch_motion(void) {
    struct s3e_pointer_motion_event event = {
        .x = g_pointer_x,
        .y = g_pointer_y,
    };
    struct s3e_pointer_touch_motion_event touch_event = {
        .touch_id = 0,
        .x = g_pointer_x,
        .y = g_pointer_y,
    };
    pointer_dispatch(1, &event);
    pointer_dispatch(3, &touch_event);
}

static void input_release_pointer(void) {
    if (g_pointer_down) {
        pointer_set_down(0);
        pointer_dispatch_button(0, 0);
    }
}

static void keyboard_dispatch_event(uint32_t key, int32_t pressed) {
    struct s3e_keyboard_event event = {
        .key = (int32_t)key,
        .pressed = pressed ? 1 : 0,
    };

    for (size_t i = 0; i < ARRAY_SIZE(g_keyboard_callbacks); ++i) {
        struct keyboard_callback_slot *slot = &g_keyboard_callbacks[i];
        if (slot->callback) {
            ((s3e_callback_fn)(uintptr_t)slot->callback)(&event, slot->user_data);
        }
    }
}

static void keyboard_set_key(uint32_t key, int down, int dispatch_callback) {
    if (key >= KEYBOARD_KEY_COUNT) {
        return;
    }

    int was_down = (g_keyboard_state[key] & KEY_STATE_DOWN) != 0;
    if (was_down == down) {
        return;
    }

    if (down) {
        g_keyboard_state[key] &= (uint8_t)~KEY_STATE_RELEASED;
        g_keyboard_state[key] |= KEY_STATE_DOWN | KEY_STATE_PRESSED;
    } else {
        g_keyboard_state[key] &= (uint8_t)~KEY_STATE_DOWN;
        g_keyboard_state[key] &= (uint8_t)~KEY_STATE_PRESSED;
        g_keyboard_state[key] |= KEY_STATE_RELEASED;
    }
    if (dispatch_callback) {
        keyboard_dispatch_event(key, down);
    }
}

static void keyboard_set_keys(const uint32_t *keys, size_t count, int down, int dispatch_callback) {
    for (size_t i = 0; i < count; ++i) {
        keyboard_set_key(keys[i], down, dispatch_callback);
    }
}

static void keyboard_clear_transitions(void) {
    for (size_t key = 0; key < ARRAY_SIZE(g_keyboard_state); ++key) {
        g_keyboard_state[key] &= (uint8_t)~(KEY_STATE_PRESSED | KEY_STATE_RELEASED);
    }
}

static uint32_t keyboard_abs_target(uint32_t key) {
    switch (key) {
    case S3E_KEY_ABS_GAME_A:
        return SHIFT_KEY_MENU_SELECT;
    case S3E_KEY_ABS_GAME_B:
        return SHIFT_KEY_ESC;
    case S3E_KEY_ABS_GAME_C:
        return SHIFT_KEY_DRIFT;
    case S3E_KEY_ABS_GAME_D:
        return SHIFT_KEY_CAMERA;
    case S3E_KEY_ABS_UP:
        return SHIFT_KEY_UP;
    case S3E_KEY_ABS_DOWN:
        return SHIFT_KEY_DOWN;
    case S3E_KEY_ABS_LEFT:
        return SHIFT_KEY_LEFT;
    case S3E_KEY_ABS_RIGHT:
        return SHIFT_KEY_RIGHT;
    case S3E_KEY_ABS_OK:
        return SHIFT_KEY_MENU_SELECT;
    case S3E_KEY_ABS_ASK:
        return SHIFT_KEY_PAUSE;
    case S3E_KEY_ABS_BSK:
        return SHIFT_KEY_ESC;
    default:
        return key;
    }
}

static void game_action_apply(int physical_down, const struct key_map *keys) {
    keyboard_set_keys(keys->keys, keys->key_count, physical_down, 1);
}

static void keyboard_release_all(void) {
    for (uint32_t key = 0; key < KEYBOARD_KEY_COUNT; ++key) {
        if (g_keyboard_state[key] & KEY_STATE_DOWN) {
            keyboard_set_key(key, 0, 1);
        }
    }
}

static void touchpad_dispatch_button(uint32_t id, int32_t pressed, int32_t x, int32_t y) {
    struct s3e_touchpad_button_event event = {
        .id = (int32_t)id,
        .pressed = pressed,
        .x = x,
        .y = y,
    };
    touchpad_dispatch(0, &event);
}

static void __attribute__((unused)) touchpad_dispatch_motion(uint32_t id, int32_t x, int32_t y) {
    struct s3e_touchpad_motion_event event = {
        .id = (int32_t)id,
        .x = x,
        .y = y,
    };
    touchpad_dispatch(1, &event);
}

static void touchpad_release(uint32_t id) {
    if (id >= TOUCHPAD_COUNT || !g_touchpad_active[id]) {
        return;
    }
    g_touchpad_active[id] = 0;
    g_touchpad_state[id] = POINTER_STATE_RELEASED;
    touchpad_dispatch_button(id, S3E_TOUCHPAD_RELEASED, g_touchpad_x[id], g_touchpad_y[id]);
}

static void touchpad_release_all(void) {
    for (uint32_t id = 0; id < TOUCHPAD_COUNT; ++id) {
        touchpad_release(id);
    }
}

/* O toque roteirizado (fila/SIMS3_TAP/SIMS3_SWIPE) tambem segura o dedo. Sem
   saber disso, a cura abaixo soltaria o dedo no meio de um toque injetado. */
static int injecao_segura_o_dedo(void);

static void input_update_cursor(uint64_t dt) {
    int a = input_button(SDL_BUTTON_A);
    int r2 = input_trigger(SDL_AXIS_TRIGGERRIGHT);
    /* 🚨 BUG DE CAMPO (02/09): "clico sem querer e a seta fica arrastando
       infinito, nao volta a clicar".
       A soltura dependia da BORDA do botao (`a != g_prev_a`). Borda e' estado
       que se PERDE: `input_update_game_keys()` sobrescreve `g_prev_a` com o
       valor atual do botao, entao basta o modo alternar (SELECT) com o A
       apertado para o loader passar a achar que ja' viu a soltura. O UP nunca
       e' despachado, o jogo fica com o dedo encostado e tudo vira arrasto —
       para sempre, porque nao existe segunda borda.
       Agora a decisao e' contra o ESTADO REAL do dedo (`g_pointer_down`), nao
       contra uma copia. Assim uma borda perdida se cura no proximo pump: se o
       botao esta solto e o dedo ainda esta encostado, ele SOBE. */
    {
        /* Recibo do estado FISICO dos dois botoes que este bug envolve. Sem ele
           a prova automatizada nao consegue dizer QUAL botao do clone uinput e'
           o A e qual e' o ATRAS. */
        static int prev_a = -1, prev_r2 = -1, prev_back = -1;
        int back = input_button(SDL_BUTTON_BACK);
        if (getenv("SIMS3_INPUT_LOG") &&
            (a != prev_a || r2 != prev_r2 || back != prev_back)) {
            fprintf(stderr, "[input] A=%d R2=%d ATRAS=%d dedo=%d cursor=%d\n", a, r2, back,
                    g_pointer_down, g_cursor_active);
            fflush(stderr);
        }
        prev_a = a;
        prev_r2 = r2;
        prev_back = back;
    }
    int quer_apertado = a || r2 || injecao_segura_o_dedo();
    if (quer_apertado != g_pointer_down) {
        pointer_set_down(quer_apertado);
        pointer_dispatch_button(0, quer_apertado ? 1 : 0);
    }
    g_prev_a = a;

    /* Sims 3 e' touch-first e nao importa joystick diretamente: ambos os
       analogicos dirigem a mesma seta. Aplicamos deadzone RADIAL, curva
       progressiva e amortecimento por tempo, para a ponta ficar precisa sem
       sacrificar velocidade. Se os dois sticks forem usados, vence o de maior
       deflexao; direcoes opostas nao se anulam por acidente. */
    int32_t lx = input_axis_raw(SDL_AXIS_LEFTX);
    int32_t ly = input_axis_raw(SDL_AXIS_LEFTY);
    int32_t rx = input_axis_raw(SDL_AXIS_RIGHTX);
    int32_t ry = input_axis_raw(SDL_AXIS_RIGHTY);
    float lxf = (float)lx / 32767.0f;
    float lyf = (float)ly / 32767.0f;
    float rxf = (float)rx / 32767.0f;
    float ryf = (float)ry / 32767.0f;
    float lmag = sqrtf(lxf * lxf + lyf * lyf);
    float rmag = sqrtf(rxf * rxf + ryf * ryf);
    int use_right = rmag > lmag;
    float x = use_right ? rxf : lxf;
    float y = use_right ? ryf : lyf;
    float mag = use_right ? rmag : lmag;
    const float deadzone = 0.24f;
    float response = 0.0f;
    if (mag > deadzone) {
        if (mag > 1.0f) mag = 1.0f;
        response = (mag - deadzone) / (1.0f - deadzone);
        response = response * response * (3.0f - 2.0f * response);
        x = x / (use_right ? rmag : lmag) * response;
        y = y / (use_right ? rmag : lmag) * response;
    } else {
        x = 0.0f;
        y = 0.0f;
    }

    static float velocity_x, velocity_y;
    static float remainder_x, remainder_y;
    float smoothing = (float)dt / 45.0f;
    if (smoothing > 1.0f) smoothing = 1.0f;
    float speed = (float)window_width() * 0.85f;
    if (speed < 650.0f) speed = 650.0f;
    if (speed > 1200.0f) speed = 1200.0f;
    velocity_x += (x * speed - velocity_x) * smoothing;
    velocity_y += (y * speed - velocity_y) * smoothing;
    if (x == 0.0f && fabsf(velocity_x) < 1.0f) velocity_x = 0.0f;
    if (y == 0.0f && fabsf(velocity_y) < 1.0f) velocity_y = 0.0f;

    float move_x = velocity_x * (float)dt / 1000.0f + remainder_x;
    float move_y = velocity_y * (float)dt / 1000.0f + remainder_y;
    int32_t step_x = (int32_t)move_x;
    int32_t step_y = (int32_t)move_y;
    remainder_x = move_x - (float)step_x;
    remainder_y = move_y - (float)step_y;
    if (!step_x && !step_y) return;

    int32_t old_x = g_pointer_x;
    int32_t old_y = g_pointer_y;
    g_pointer_x = clamp_pointer_x(g_pointer_x + step_x);
    g_pointer_y = clamp_pointer_y(g_pointer_y + step_y);
    if (g_pointer_x != old_x || g_pointer_y != old_y) {
        /* Em touchscreen nao existe hover: movimento so e' arrasto enquanto o
           dedo esta encostado. Despachar cada passo da seta solta fazia a lista
           de saves guardar estado de scroll e rejeitar o proximo New Game. O
           evento de botao ja carrega x/y e resolve o alvo sem movimento previo. */
        if (g_pointer_down) {
            pointer_dispatch_motion();
        }
        if (getenv("SIMS3_INPUT_LOG")) {
            static uint64_t last_cursor_log;
            uint64_t now = monotonic_ms();
            if (now - last_cursor_log >= 250) {
                fprintf(stderr, "[input] cursor=%d,%d via %s raw L=%d,%d R=%d,%d\n",
                        g_pointer_x, g_pointer_y,
                        use_right ? "analogico-direito" : "analogico-esquerdo", lx, ly, rx, ry);
                fflush(stderr);
                last_cursor_log = now;
            }
        }
    }
}

static void input_update_game_keys(void) {
    /* Sincroniza a copia do A ao entrar no modo de teclas. Isto APAGA a borda
       que o modo de ponteiro usava — era metade do bug do dedo preso. Continua
       aqui de proposito (o modo de teclas precisa da copia sincronizada), mas
       agora e' inofensivo: input_update_cursor decide pelo estado real do dedo. */
    g_prev_a = input_button(SDL_BUTTON_A);

    /* NFS Shift (NextOS): mapeamento Xbox-padrao dos nossos ports de corrida
       (igual NFS Hot Pursuit: nitro=R2, freio=L2, START=pausa). Aceleracao e'
       automatica no jogo. Setas = menu + esterco digital (KeySteer* do ICF). */
    game_action_apply(input_button(SDL_BUTTON_A), &KEYMAP_SELECT);
    game_action_apply(input_button(SDL_BUTTON_B), &KEYMAP_BACK);
    game_action_apply(input_button(SDL_BUTTON_X), &KEYMAP_DRIFT);
    game_action_apply(input_button(SDL_BUTTON_Y), &KEYMAP_CAMERA);
    game_action_apply(input_button(SDL_BUTTON_LEFTSHOULDER), &KEYMAP_GEAR_DOWN);
    game_action_apply(input_button(SDL_BUTTON_RIGHTSHOULDER), &KEYMAP_GEAR_UP);
    game_action_apply(input_trigger(SDL_AXIS_TRIGGERLEFT), &KEYMAP_BRAKE);
    game_action_apply(input_trigger(SDL_AXIS_TRIGGERRIGHT), &KEYMAP_NITRO);
    game_action_apply(input_button(SDL_BUTTON_START), &KEYMAP_PAUSE);

    /* Setas: dpad OU stick esquerdo -> s3eKeyLeft/Up/Right/Down (9/10/11/12).
       O proprio jogo le essas teclas p/ navegar menu e estercar (Xperia Play). */
    int32_t lx = input_axis(SDL_AXIS_LEFTX);
    int32_t ly = input_axis(SDL_AXIS_LEFTY);
    game_action_apply(input_dpad_left() || lx < 0, &KEYMAP_ARROW_LEFT);
    game_action_apply(input_dpad_right() || lx > 0, &KEYMAP_ARROW_RIGHT);
    game_action_apply(input_dpad_up() || ly < 0, &KEYMAP_ARROW_UP);
    game_action_apply(input_dpad_down() || ly > 0, &KEYMAP_ARROW_DOWN);
}

static void input_update_game_touchpads(void) {
    /* NFS Shift nao registra os touchpads Xperia (extensao ausente nos imports
       do modulo) — nada a alimentar aqui. */
}

/* Toque roteirizado (SIMS3_TAP="ms,x,y,segurar_ms;...").
   Instrumento de PROVA, nao de release: dirige o jogo sem dedo humano num
   device sem tela de toque. Duas regras da casa estao embutidas:
   - o toque e' contado por TEMPO (relogio monotonico), nunca por "um evento
     por pump" — em tela lenta um pump pode demorar centenas de ms;
   - o dedo POUSA primeiro (so movimento) e so no quadro seguinte APERTA:
     movimento e aperto no mesmo pump nao acionam os botoes deste jogo, que
     resolve o widget sob o cursor no evento de movimento. Movimento tambem
     NAO e' reemitido enquanto segura, senao vira arrasto e cancela o clique. */
#define TAP_STEPS 48
#define TAP_POUSO_MS 150
static struct { long at_ms, hold_ms; int x, y; int state; } g_tap[TAP_STEPS];
static int g_tap_n = -1;
static uint64_t g_tap_t0;

/* Arrasto roteirizado: SIMS3_SWIPE="ms,x1,y1,x2,y2,dur_ms". Segura e desliza
   emitindo movimento interpolado POR TEMPO (o jogo pede "swipe" para girar o
   Sim e usa arrasto para a camera da cidade). Um unico arrasto por corrida. */
static struct { long at_ms, dur_ms; int x1, y1, x2, y2; int state; } g_swipe;
static int g_swipe_on = -1;
static uint64_t g_swipe_t0;

static void swipe_script_pump(long now) {
    if (g_swipe_t0) now = (long)(monotonic_ms() - g_swipe_t0);
    if (g_swipe_on < 0) {
        const char *spec = getenv("SIMS3_SWIPE");
        g_swipe_on = 0;
        if (spec && sscanf(spec, "%ld,%d,%d,%d,%d,%ld", &g_swipe.at_ms, &g_swipe.x1,
                           &g_swipe.y1, &g_swipe.x2, &g_swipe.y2, &g_swipe.dur_ms) == 6) {
            g_swipe_on = 1;
            g_swipe.state = 0;
            fprintf(stderr, "[swipe] roteiro (%d,%d)->(%d,%d) em %ld ms\n", g_swipe.x1,
                    g_swipe.y1, g_swipe.x2, g_swipe.y2, g_swipe.dur_ms);
            fflush(stderr);
        }
    }
    if (g_swipe_on != 1) return;
    if (g_swipe.state == 0 && now >= g_swipe.at_ms) {
        g_pointer_x = g_swipe.x1;
        g_pointer_y = g_swipe.y1;
        pointer_dispatch_motion();
        g_swipe.state = 1;
    } else if (g_swipe.state == 1 && now >= g_swipe.at_ms + TAP_POUSO_MS) {
        pointer_set_down(1);
        pointer_dispatch_button(0, 1);
        g_swipe.state = 2;
        fprintf(stderr, "[swipe] %ld ms: comeca\n", now);
        fflush(stderr);
    } else if (g_swipe.state == 2) {
        long t = now - (g_swipe.at_ms + TAP_POUSO_MS);
        if (t >= g_swipe.dur_ms) {
            g_pointer_x = g_swipe.x2;
            g_pointer_y = g_swipe.y2;
            pointer_dispatch_motion();
            pointer_set_down(0);
            pointer_dispatch_button(0, 0);
            g_swipe.state = 3;
            fprintf(stderr, "[swipe] %ld ms: termina\n", now);
            fflush(stderr);
        } else {
            g_pointer_x = g_swipe.x1 + (int)((g_swipe.x2 - g_swipe.x1) * t / g_swipe.dur_ms);
            g_pointer_y = g_swipe.y1 + (int)((g_swipe.y2 - g_swipe.y1) * t / g_swipe.dur_ms);
            pointer_dispatch_motion();
        }
    }
}

/* Fila de comandos em arquivo (SIMS3_TAP_FIFO=/tmp/sims3tap).
   O roteiro por relogio de parede nao serve para dirigir este jogo: o tempo de
   cada tela varia entre corridas e um toque em instante fixo ora acerta a tela
   certa ora acerta outra (ja trocou o idioma para frances sem querer). Com a
   fila da para FECHAR O LAÇO de fora: capturar /dev/fb0, ver em que tela esta,
   escrever o toque e capturar de novo.
   Uma linha por comando:  "x,y,segurar_ms"  ou  "s x1,y1,x2,y2,dur_ms".
   O arquivo e' esvaziado depois de lido. */
static uint32_t g_key_inject_code = 0;
static uint64_t g_key_inject_until = 0;
static void key_inject_pump(void) {
    if (g_key_inject_until && monotonic_ms() >= g_key_inject_until) {
        keyboard_set_key(g_key_inject_code, 0, 1);
        g_key_inject_until = 0;
    }
}

static void tap_fifo_pump(void) {
    const char *path = getenv("SIMS3_TAP_FIFO");
    if (!path || !path[0]) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        int x1, y1, x2, y2;
        long d;
        if (line[0] == 's' &&
            sscanf(line + 1, "%d,%d,%d,%d,%ld", &x1, &y1, &x2, &y2, &d) == 5) {
            g_swipe.at_ms = 0;
            g_swipe.x1 = x1; g_swipe.y1 = y1; g_swipe.x2 = x2; g_swipe.y2 = y2;
            g_swipe.dur_ms = d;
            g_swipe.state = 0;
            g_swipe_on = 1;
            g_swipe_t0 = monotonic_ms();
            fprintf(stderr, "[fila] arrasto (%d,%d)->(%d,%d)\n", x1, y1, x2, y2);
        } else if (line[0] == 'k' && sscanf(line + 1, "%d,%ld", &x1, &d) == 2) {
            /* Canal de entrada que nunca tinha sido testado neste port: o jogo
               importa s3eKeyboard e o payload traz assets/softkey. Sem uma forma
               de mandar TECLA, "o CAS nao responde" so' provava que ele nao
               responde a TOQUE. Uma tecla por comando, segurada e solta pelo
               pump como um toque. */
            g_key_inject_code = (uint32_t)x1;
            g_key_inject_until = monotonic_ms() + (uint64_t)(d > 0 ? d : 200);
            keyboard_set_key((uint32_t)x1, 1, 1);
            fprintf(stderr, "[fila] tecla %d segurando %ld ms\n", x1, d);
        } else if (sscanf(line, "%d,%d,%ld", &x1, &y1, &d) == 3 && g_tap_n < TAP_STEPS) {
            int i = g_tap_n++;
            g_tap[i].at_ms = (long)(monotonic_ms() - g_tap_t0);
            g_tap[i].x = x1; g_tap[i].y = y1; g_tap[i].hold_ms = d;
            g_tap[i].state = 0;
            fprintf(stderr, "[fila] toque (%d,%d) segurando %ld ms\n", x1, y1, d);
        }
    }
    fclose(f);
    f = fopen(path, "w");
    if (f) fclose(f);
    fflush(stderr);
}

static void tap_script_pump(void) {
    if (g_tap_n < 0) {
        const char *spec = getenv("SIMS3_TAP");
        g_tap_n = 0;
        g_tap_t0 = monotonic_ms();
        for (const char *p = spec; p && *p && g_tap_n < TAP_STEPS;) {
            long at, hold; int x, y;
            if (sscanf(p, "%ld,%d,%d,%ld", &at, &x, &y, &hold) == 4) {
                g_tap[g_tap_n].at_ms = at;
                g_tap[g_tap_n].x = x;
                g_tap[g_tap_n].y = y;
                g_tap[g_tap_n].hold_ms = hold;
                g_tap[g_tap_n].state = 0;
                g_tap_n++;
            }
            p = strchr(p, ';');
            if (p) p++;
        }
        if (g_tap_n) {
            fprintf(stderr, "[tap] roteiro com %d toque(s)\n", g_tap_n);
            fflush(stderr);
        }
    }
    long now = (long)(monotonic_ms() - g_tap_t0);
    tap_fifo_pump();
    key_inject_pump();
    swipe_script_pump(now);
    /* at_ms NEGATIVO = "tantos ms DEPOIS da ancora" (ver s3e_file.c). Enquanto
       a ancora nao abriu, esses passos simplesmente nao existem. */
    uint64_t anc = s3e_file_anchor_ms();
    for (int i = 0; i < g_tap_n; i++) {
        if (g_tap[i].at_ms < 0) {
            if (!anc) continue;
            long rel = (long)(anc - g_tap_t0) - g_tap[i].at_ms;
            if (g_tap[i].state == 0 && now >= rel) g_tap[i].at_ms = rel;
        }
    }
    if (!g_tap_n) return;
    for (int i = 0; i < g_tap_n; i++) {
        if (g_tap[i].state == 0 && now >= g_tap[i].at_ms) {
            g_pointer_x = g_tap[i].x;
            g_pointer_y = g_tap[i].y;
            /* SIMS3_TAP_NOMOVE=1: posiciona sem emitir movimento. Serve para
               testar widgets dentro de container rolavel, que podem tomar o
               movimento anterior ao aperto como inicio de arrasto. */
            if (!getenv("SIMS3_TAP_NOMOVE")) pointer_dispatch_motion();
            g_tap[i].state = 1;
        } else if (g_tap[i].state == 1 && now >= g_tap[i].at_ms + TAP_POUSO_MS) {
            pointer_set_down(1);
            pointer_dispatch_button(0, 1);
            g_tap[i].state = 2;
            fprintf(stderr, "[tap] %ld ms: DESCE em (%d,%d)\n", now, g_tap[i].x, g_tap[i].y);
            fflush(stderr);
        } else if (g_tap[i].state == 2 &&
                   now >= g_tap[i].at_ms + TAP_POUSO_MS + g_tap[i].hold_ms) {
            pointer_set_down(0);
            pointer_dispatch_button(0, 0);
            g_tap[i].state = 3;
            fprintf(stderr, "[tap] %ld ms: SOLTA em (%d,%d)\n", now, g_tap[i].x, g_tap[i].y);
            fflush(stderr);
        }
    }
}

/* Verdadeiro enquanto um toque ou arrasto ROTEIRIZADO esta com o dedo encostado
   (estado 2 das duas maquinas). */
static int injecao_segura_o_dedo(void) {
    if (g_swipe_on == 1 && g_swipe.state == 2) {
        return 1;
    }
    for (int i = 0; i < g_tap_n && i < TAP_STEPS; i++) {
        if (g_tap[i].state == 2) {
            return 1;
        }
    }
    return 0;
}

void input_pump(void) {
    if (g_input_pumping) {
        return;
    }
    g_input_pumping = 1;
    tap_script_pump();

    input_open();
    if (!g_controller) {
        goto out;
    }
    g_sdl.GameControllerUpdate();
    g_hat_mask = input_current_hat_mask();

    /* NextOS: Select+Start = sair do port (hotkey padrao dos nossos ports).
       O launcher restaura o free_scale ao ver o processo terminar. */
    if (input_button(SDL_BUTTON_BACK) && input_button(SDL_BUTTON_START)) {
        _exit(0);
    }

    uint64_t now = monotonic_ms();
    if (!g_input_last_ms) {
        g_input_last_ms = now;
    }
    uint64_t dt = now - g_input_last_ms;
    g_input_last_ms = now;
    if (dt > 50) {
        dt = 50;
    }

    /* 🚨 BUG DE CAMPO (02/09): "clico em ATRAS sem querer e a seta nao volta a
       clicar, preciso voltar."
       O botao Back/Select alternava para um modo de TECLAS herdado do NFS Shift
       — e nesse modo o ponteiro simplesmente para. Para o Sims 3 esse modo nao
       serve para nada: o mapa e' de jogo de corrida (DRIFT, NITRO, GEAR_UP) e
       foi MEDIDO no device que nenhum dos 11 codigos de tecla faz efeito. Ou
       seja, o botao so' desligava o unico controle que funciona. E' armadilha,
       nao recurso: fica desligado por padrao.
       SIMS3_MODO_TECLAS=1 devolve o alternador para experimento. O atalho da
       casa Select+Start (sair) segue intacto — ele e' tratado acima. */
    {
        static int alternador = -1;
        if (alternador < 0) {
            const char *e = getenv("SIMS3_MODO_TECLAS");
            alternador = e && e[0] == '1';
        }
        int select = input_button(SDL_BUTTON_BACK);
        if (alternador && select && !g_prev_select) {
            if (g_cursor_active) {
                input_release_pointer();
            } else {
                touchpad_release_all();
                keyboard_release_all();
            }
            g_cursor_active = !g_cursor_active;
        }
        g_prev_select = select;
    }

    if (g_cursor_active) {
        touchpad_release_all();
        if (g_keyboard_update_active) {
            keyboard_release_all();
        }
        input_update_cursor(dt);
    } else {
        input_release_pointer();
        input_update_game_touchpads();
        if (g_keyboard_update_active) {
            input_update_game_keys();
        }
    }

out:
    g_input_pumping = 0;
}

void input_shutdown(void) {
    input_release_pointer();
    touchpad_release_all();
    keyboard_release_all();
    if (g_controller && g_sdl.GameControllerClose) {
        g_sdl.GameControllerClose(g_controller);
        g_controller = NULL;
        g_joystick = NULL;
    }
    if (g_sdl2) {
        if (g_sdl.QuitSubSystem) {
            g_sdl.QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
        }
        dlclose(g_sdl2);
        g_sdl2 = NULL;
    }
}

int32_t s3eKeyboardRegister(uint32_t id, void *callback, void *user_data) {
    struct keyboard_callback_slot *free_slot = NULL;
    for (size_t i = 0; i < ARRAY_SIZE(g_keyboard_callbacks); ++i) {
        struct keyboard_callback_slot *slot = &g_keyboard_callbacks[i];
        if (slot->callback == callback && slot->id == id) {
            slot->user_data = user_data;
            return 0;
        }
        if (!slot->callback && !free_slot) {
            free_slot = slot;
        }
    }
    if (free_slot) {
        free_slot->id = id;
        free_slot->callback = callback;
        free_slot->user_data = user_data;
    }
    return 0;
}

int32_t s3eKeyboardUnRegister(uint32_t id, void *callback) {
    for (size_t i = 0; i < ARRAY_SIZE(g_keyboard_callbacks); ++i) {
        struct keyboard_callback_slot *slot = &g_keyboard_callbacks[i];
        if (slot->callback && slot->id == id && (!callback || callback == slot->callback)) {
            slot->callback = NULL;
            slot->user_data = NULL;
        }
    }
    return 0;
}

int32_t s3eKeyboardUpdate(void) {
    keyboard_clear_transitions();
    g_keyboard_update_active = 1;
    input_pump();
    if (g_cursor_active || !g_controller) {
        keyboard_release_all();
    }
    g_keyboard_update_active = 0;
    dispatch_due_timers();
    return 0;
}

int32_t s3eKeyboardGetState(uint32_t key) {
    uint32_t target = keyboard_abs_target(key);
    return target < KEYBOARD_KEY_COUNT ? g_keyboard_state[target] : 0;
}

int32_t s3eKeyboardAnyKey(void) {
    for (size_t i = 0; i < ARRAY_SIZE(g_keyboard_state); ++i) {
        if (g_keyboard_state[i] & (KEY_STATE_DOWN | KEY_STATE_PRESSED)) {
            return 1;
        }
    }
    return 0;
}

int32_t s3eKeyboardGetInt(uint32_t key) {
    switch (key) {
    case 0:
    case 2:
        return 1;
    case 1:
    case 4:
    case 5:
    case 6:
        return 0;
    default:
        return -1;
    }
}

int32_t s3eKeyboardSetInt(uint32_t key, int32_t value) {
    (void)key;
    (void)value;
    return 0;
}

const char *s3eKeyboardGetDisplayName(uint32_t key) {
    switch (key) {
    case SHIFT_KEY_MENU_SELECT:
        return "Select";
    case SHIFT_KEY_ESC:
        return "Back";
    case SHIFT_KEY_LEFT:
        return "Left";
    case SHIFT_KEY_UP:
        return "Up";
    case SHIFT_KEY_RIGHT:
        return "Right";
    case SHIFT_KEY_DOWN:
        return "Down";
    case SHIFT_KEY_GEAR_DOWN:
        return "GearDown";
    case SHIFT_KEY_GEAR_UP:
        return "GearUp";
    case SHIFT_KEY_BRAKE:
        return "Brake";
    case SHIFT_KEY_NITRO:
        return "Nitro";
    case SHIFT_KEY_DRIFT:
        return "Drift";
    case SHIFT_KEY_CAMERA:
        return "Camera";
    case SHIFT_KEY_PAUSE:
        return "Pause";
    case S3E_KEY_ABS_GAME_A:
        return "KeyAbsGameA";
    case S3E_KEY_ABS_GAME_B:
        return "KeyAbsGameB";
    case S3E_KEY_ABS_GAME_C:
        return "KeyAbsGameC";
    case S3E_KEY_ABS_GAME_D:
        return "KeyAbsGameD";
    case S3E_KEY_ABS_UP:
        return "KeyAbsUp";
    case S3E_KEY_ABS_DOWN:
        return "KeyAbsDown";
    case S3E_KEY_ABS_LEFT:
        return "KeyAbsLeft";
    case S3E_KEY_ABS_RIGHT:
        return "KeyAbsRight";
    case S3E_KEY_ABS_OK:
        return "KeyAbsOk";
    case S3E_KEY_ABS_ASK:
        return "KeyAbsASK";
    case S3E_KEY_ABS_BSK:
        return "KeyAbsBSK";
    default:
        return "";
    }
}

void s3eKeyboardClearState(void) {
    memset(g_keyboard_state, 0, sizeof(g_keyboard_state));
}

int32_t s3ePointerRegister(uint32_t id, void *callback, void *user_data) {
    if (getenv("SIMS3_POINTER_LOG")) {
        fprintf(stderr, "[ptr] Register(id=%u, cb=%p)\n", id, callback);
        fflush(stderr);
    }
    if (id < ARRAY_SIZE(g_pointer_callbacks)) {
        g_pointer_callbacks[id].callback = callback;
        g_pointer_callbacks[id].user_data = user_data;
    }
    return 0;
}

int32_t s3ePointerUnRegister(uint32_t id, void *callback) {
    if (getenv("SIMS3_POINTER_LOG")) {
        fprintf(stderr, "[ptr] UnRegister(id=%u)\n", id);
        fflush(stderr);
    }
    if (id < ARRAY_SIZE(g_pointer_callbacks) &&
        (!callback || callback == g_pointer_callbacks[id].callback)) {
        g_pointer_callbacks[id].callback = NULL;
        g_pointer_callbacks[id].user_data = NULL;
    }
    return 0;
}

int32_t s3ePointerUpdate(void) {
    g_ptr_update_calls++;
    pointer_clear_transitions();
    input_pump();
    dispatch_due_timers();
    return 0;
}

int32_t s3ePointerGetInt(uint32_t key) {
    input_pump();
    switch (key) {
    case 0:
        return 1;
    case 1:
        return g_pointer_x;
    case 2:
        return g_pointer_y;
    default:
        return 0;
    }
}

int32_t s3ePointerSetInt(uint32_t key, int32_t value) {
    (void)key;
    (void)value;
    return 0;
}

int32_t s3ePointerGetState(uint32_t button) {
    g_ptr_getstate_calls++;
    input_pump();
    return button < sizeof(g_pointer_states) ? g_pointer_states[button] : 0;
}

int32_t s3ePointerGetX(void) {
    g_ptr_getxy_calls++;
    input_pump();
    return g_pointer_x;
}

int32_t s3ePointerGetY(void) {
    input_pump();
    return g_pointer_y;
}

int32_t s3ePointerGetTouchState(uint32_t touch_id) {
    input_pump();
    if (g_cursor_active) {
        return touch_id == 0 ? g_pointer_states[0] : 0;
    }
    return 0;
}

int32_t s3ePointerGetTouchX(uint32_t touch_id) {
    input_pump();
    if (g_cursor_active) {
        return touch_id == 0 ? g_pointer_x : 0;
    }
    return 0;
}

int32_t s3ePointerGetTouchY(uint32_t touch_id) {
    input_pump();
    if (g_cursor_active) {
        return touch_id == 0 ? g_pointer_y : 0;
    }
    return 0;
}

int32_t s3ePointerGetPressure(uint32_t button) {
    input_pump();
    return button == 0 && g_pointer_down ? 1 : 0;
}

int32_t s3ePointerGetTouchPressure(uint32_t touch_id) {
    input_pump();
    if (g_cursor_active) {
        return touch_id == 0 && g_pointer_down ? 1 : 0;
    }
    return 0;
}

int32_t s3ePointerGetError(void) {
    return 0;
}

const char *s3ePointerGetErrorString(void) {
    return "S3E_POINTER_ERR_NONE";
}
