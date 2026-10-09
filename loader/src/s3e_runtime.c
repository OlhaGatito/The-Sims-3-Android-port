#include "s3e_host_internal.h"

static int g_device_quit_requested;

static uint64_t timer_elapsed_ms(void) {
    uint64_t now = monotonic_us();
    if (!g_host_start_us || now < g_host_start_us) {
        g_host_start_us = now;
    }
    return (now - g_host_start_us) / 1000u;
}

void *s3eMallocBase(uint32_t size, const char *file, int line) {
    (void)file;
    (void)line;
    if (g_user_mem_mgr_set && g_user_mem_mgr.alloc &&
        g_user_mem_mgr.alloc != (void *)(uintptr_t)&s3eMallocBase && !g_in_user_mem_mgr) {
        typedef void *(*alloc_fn)(uint32_t size);
        g_in_user_mem_mgr = 1;
        void *ptr = ((alloc_fn)(uintptr_t)g_user_mem_mgr.alloc)(size);
        g_in_user_mem_mgr = 0;
        if (!ptr) {
            /* The Sims 3 (NextOS): alocacao NEGADA pelo gerenciador do PROPRIO
               jogo. Isso nao aparece como erro em lugar nenhum — o jogo segue
               com um ponteiro nulo e morre longe daqui (ex.: mapa de hash com
               vetor de baldes NULO na saida da tela de idioma). Logar sempre. */
            static unsigned failures;
            if (failures++ < 32) {
                fprintf(stderr, "[mem] gerenciador do jogo NEGOU %u bytes (falha #%u)\n", size,
                        failures);
                fflush(stderr);
            }
        }
        return ptr;
    }
    return calloc(1, size ? size : 1);
}

void *s3eReallocBase(void *ptr, uint32_t size, const char *file, int line) {
    (void)file;
    (void)line;
    if (g_user_mem_mgr_set && g_user_mem_mgr.realloc &&
        g_user_mem_mgr.realloc != (void *)(uintptr_t)&s3eReallocBase && !g_in_user_mem_mgr) {
        typedef void *(*realloc_fn)(void *ptr, uint32_t size);
        g_in_user_mem_mgr = 1;
        void *new_ptr = ((realloc_fn)(uintptr_t)g_user_mem_mgr.realloc)(ptr, size);
        g_in_user_mem_mgr = 0;
        return new_ptr;
    }
    if (!ptr) {
        return calloc(1, size ? size : 1);
    }
    return realloc(ptr, size ? size : 1);
}

void s3eFreeBase(void *ptr) {
    if (g_user_mem_mgr_set && g_user_mem_mgr.free &&
        g_user_mem_mgr.free != (void *)(uintptr_t)&s3eFreeBase && !g_in_user_mem_mgr) {
        typedef void (*free_fn)(void *ptr);
        g_in_user_mem_mgr = 1;
        ((free_fn)(uintptr_t)g_user_mem_mgr.free)(ptr);
        g_in_user_mem_mgr = 0;
        return;
    }
    free(ptr);
}

/* Contadores do laco do jogo: s3eTimerGetMs sai exatamente 1x por quadro, o que
   ja' descarta carregador fatiado por orcamento de tempo (esse consultaria o
   relogio muitas vezes por quadro). Contamos tambem UST e os yields para saber
   o que o quadro do Create-A-Sim faz de diferente do quadro do menu. */
static unsigned long g_timer_ust_calls = 0;
static unsigned long g_timer_set = 0;
static unsigned long g_timer_fired = 0;
static unsigned long g_timer_cancel = 0;
unsigned long s3e_timer_set_count(void) {
    return g_timer_set;
}
unsigned long s3e_timer_fired_count(void) {
    return g_timer_fired;
}
unsigned long s3e_timer_cancel_count(void) {
    return g_timer_cancel;
}
static unsigned long g_yield_calls = 0;
static unsigned long g_yield_evt_calls = 0;
unsigned long s3e_timer_ust_calls(void) {
    return g_timer_ust_calls;
}
unsigned long s3e_yield_calls(void) {
    return g_yield_calls;
}
unsigned long s3e_yield_evt_calls(void) {
    return g_yield_evt_calls;
}
uint64_t s3eTimerGetUST(void) {
    g_timer_ust_calls++;
    return timer_elapsed_ms();
}

/* Sonda do relogio (Sims 3/NextOS): a cena do Create-A-Sim fica BYTE A BYTE
   identica enquanto o render faz 97 draws por quadro e a entrada e' pompada.
   Isso separa duas causas muito diferentes: render sem logica (o jogo nem pede
   as horas) ou logica travada (pede, mas o dt nao anda). Contamos as chamadas e
   o valor devolvido — sem isso a diferenca e' invisivel. */
static unsigned long g_timer_ms_calls = 0;
unsigned long s3e_timer_ms_calls(void) {
    return g_timer_ms_calls;
}
uint64_t s3eTimerGetMs(void) {
    g_timer_ms_calls++;
    uint64_t ms = timer_elapsed_ms();
    if (getenv("SIMS3_CLOCK_LOG") && (g_timer_ms_calls % 300u) == 1u) {
        fprintf(stderr, "[clock] GetMs chamada #%lu -> %llu ms\n", g_timer_ms_calls,
                (unsigned long long)ms);
        fflush(stderr);
    }
    return ms;
}

int32_t s3eTimerGetInt(uint32_t key) {
    return key == 0 ? 1 : -1;
}

void dispatch_due_timers(void) {
    uint64_t now = monotonic_ms();
    struct timer_event *ready = NULL;
    pthread_mutex_lock(&g_timer_mutex);
    struct timer_event **link = &g_timers;
    while (*link) {
        struct timer_event *timer = *link;
        if (timer->due_ms <= now) {
            *link = timer->next;
            timer->next = ready;
            ready = timer;
        } else {
            link = &timer->next;
        }
    }
    pthread_mutex_unlock(&g_timer_mutex);
    while (ready) {
        struct timer_event *next = ready->next;
        if (ready->callback) {
            g_timer_fired++;
            ((s3e_callback_fn)(uintptr_t)ready->callback)(NULL, ready->user_data);
        }
        free(ready);
        ready = next;
    }
}

static void wait_with_timers(uint32_t ms) {
    uint64_t deadline = monotonic_ms() + ms;
    while (1) {
        dispatch_due_timers();
        uint64_t now = monotonic_ms();
        if (now >= deadline) {
            break;
        }
        uint32_t step = (uint32_t)(deadline - now);
        if (step > 10) {
            step = 10;
        }
        sleep_ms(step);
    }
}

uint32_t s3eTimerSetTimer(uint32_t period_ms, void *callback, void *user_data) {
    g_timer_set++;
    struct timer_event *timer = calloc(1, sizeof(*timer));
    if (!timer) {
        return 0;
    }
    uint32_t id = g_next_timer_id++;
    if (g_next_timer_id == 0) {
        g_next_timer_id = 1;
    }
    timer->id = id;
    timer->due_ms = monotonic_ms() + period_ms;
    timer->callback = callback;
    timer->user_data = user_data;
    pthread_mutex_lock(&g_timer_mutex);
    timer->next = g_timers;
    g_timers = timer;
    pthread_mutex_unlock(&g_timer_mutex);
    return id;
}

int32_t s3eTimerCancelTimer(uint32_t id) {
    g_timer_cancel++;
    int found = 0;
    pthread_mutex_lock(&g_timer_mutex);
    struct timer_event **link = &g_timers;
    while (*link) {
        struct timer_event *timer = *link;
        if (timer->id == id) {
            *link = timer->next;
            free(timer);
            found = 1;
            break;
        }
        link = &timer->next;
    }
    pthread_mutex_unlock(&g_timer_mutex);
    return found ? 0 : -1;
}

uint64_t s3eTimerGetUTC(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

int64_t s3eTimerGetLocaltimeOffset(const uint64_t *utc_ms) {
    time_t now = utc_ms ? (time_t)(*utc_ms / 1000u) : time(NULL);
    struct tm local_tm;
    struct tm utc_tm;
    localtime_r(&now, &local_tm);
    gmtime_r(&now, &utc_tm);
    time_t local = mktime(&local_tm);
    time_t utc = mktime(&utc_tm);
    return (int64_t)difftime(local, utc) * 1000;
}

int32_t s3eDeviceRegister(uint32_t id, void *callback, void *user_data);
int32_t s3eDeviceUnRegister(uint32_t id, void *callback);

uint64_t s3eDeviceYield(int32_t ms) {
    g_yield_calls++;
    input_pump();
    if (ms == INT32_MIN) {
        dispatch_due_timers();
        sleep_ms(1);
    } else if (ms > 0) {
        wait_with_timers((uint32_t)ms);
    } else {
        dispatch_due_timers();
    }
    return timer_elapsed_ms();
}

uint64_t s3eDeviceYieldUntilEvent(int32_t ms) {
    g_yield_evt_calls++;
    return s3eDeviceYield(ms ? ms : INT32_MIN);
}

int32_t s3eDeviceCheckQuitRequest(void) {
    input_pump();
    dispatch_due_timers();
    return g_device_quit_requested;
}

int32_t s3eDeviceCheckPauseRequest(void) {
    return 0;
}

// Used to determine the device type and what controller binding set to use
int32_t s3eDeviceGetInt(uint32_t key) {
    switch (key) {
    case 0:
        return 0x12; // XperiaPlayBO
    case 10:
        return 0x00080101; // BOPlayerControlsWin
    default:
        return 0; // falls back to BOPlayerControlsWin?
    }
}

// This doesn't affect the active binding set,
// but does affect things like hud controller hints
const char *s3eDeviceGetString(uint32_t key) {
    const char *ret;
    switch (key) {
    case 0:
        ret = "Android";
        break;
    case 2:
        ret = "R800i";
        break;
    case 8:
        ret = "ARM7A";
        break;
    case 0x0d:
        ret = "4.1.2";
        break;
    /* NFS Shift (NextOS): 🚫 SEMPRE INGLES. O jogo resolve o idioma da UI pela
       locale do device — e' a chave 0x1f (confirmado por log: e' a UNICA chave
       de string que ele consulta; devolver "" o fazia cair em alemao). Devolvemos
       "en" em 0x1f e nas demais chaves plausiveis de locale p/ nunca cair em
       alemao/japones. */
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x16:
    case 0x1f:
        ret = "en";
        break;
    case 0x14:
        ret = "en_US";
        break;
    case 0x15:
        ret = "Sony Ericsson Xperia Play";
        break;
    default:
        ret = "";
        break;
    }
    if (getenv("SIMS3_LOC_LOG")) {
        fprintf(stderr, "[s3e] DeviceGetString(key=0x%x) -> \"%s\"\n", key, ret);
    }
    return ret;
}

int32_t s3eDeviceSetInt(uint32_t key, int32_t value) {
    (void)key;
    (void)value;
    return 0;
}

int32_t s3eDeviceBacklightOn(void) {
    return 0;
}

int32_t s3eDeviceRequestQuit(void) {
    g_device_quit_requested = 1;
    return 0;
}

int32_t s3eDeviceAbort(void) {
    s3e_dump_module_stack("abort");
    fprintf(stderr, "[s3e] *** s3eDeviceAbort() — o jogo abortou ***\n");
    fflush(stderr);
    _Exit(1);
}

int32_t s3eDeviceExit(void) {
    fprintf(stderr, "[s3e] s3eDeviceExit()\n");
    fflush(stderr);
    _Exit(0);
}

/* The Sims 3 (NextOS): o loader base DESCARTAVA todo o canal de debug. Este
   jogo fala por ele antes de abortar (s3eDeviceAbort) — sem isto, o abort chega
   sem NENHUMA mensagem e o motivo fica invisivel. Regra da casa: log vazio nao
   e' ausencia de erro, e' erro nao lido. */
/* The Sims 3 (NextOS): pseudo-backtrace do MODULO. O modulo nao tem simbolos
   nem unwind info utilizavel pelo host, entao varremos a pilha e imprimimos
   todo valor que caia dentro da faixa carregada (base..base+code_mem). Com o
   mod.asm (objdump -b binary --adjust-vma=base) isso vira uma pilha de chamadas
   legivel — e' a unica forma barata de achar QUEM abortou. */
/* Le /proc/self/maps e devolve o fim da regiao que contem addr (0 se nenhuma).
   Sem isto, varrer a pilha DEPOIS de um estouro faz o proprio handler tomar
   SIGSEGV e o processo morre sem imprimir nada. */
/* SEM stdio e SEM malloc: quando o crash e' DENTRO do malloc (arena da libc),
   qualquer fopen no handler tambem morre e o dump some. Aqui so open/read. */
static char g_maps_buf[65536];
static ssize_t g_maps_len;

static void maps_slurp(void) {
    g_maps_len = 0;
    int fd = open("/proc/self/maps", O_RDONLY);
    if (fd < 0) {
        return;
    }
    ssize_t got;
    while (g_maps_len < (ssize_t)sizeof(g_maps_buf) - 1 &&
           (got = read(fd, g_maps_buf + g_maps_len, sizeof(g_maps_buf) - 1 - (size_t)g_maps_len)) >
               0) {
        g_maps_len += got;
    }
    g_maps_buf[g_maps_len > 0 ? g_maps_len : 0] = 0;
    close(fd);
}

static unsigned long hex_at(const char **p) {
    unsigned long v = 0;
    while (**p) {
        char c = **p;
        int d;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else {
            break;
        }
        v = v * 16 + (unsigned long)d;
        (*p)++;
    }
    return v;
}

static uintptr_t region_end_of(uintptr_t addr, uintptr_t *begin_out) {
    maps_slurp();
    const char *p = g_maps_buf;
    while (*p) {
        const char *line = p;
        unsigned long lo = hex_at(&p);
        unsigned long hi = 0;
        if (*p == '-') {
            p++;
            hi = hex_at(&p);
        }
        if (hi && addr >= lo && addr < hi) {
            (void)line;
            if (begin_out) {
                *begin_out = lo;
            }
            return hi;
        }
        while (*p && *p != '\n') {
            p++;
        }
        if (*p) {
            p++;
        }
    }
    return 0;
}

/* The Sims 3 (NextOS): espia palavras do MODULO por offset, para conferir
   estruturas que so existem dentro dele (ex.: o objeto de view que converte a
   coordenada do toque). SIMS3_PEEK="0x1f6284:6[,0x...:n]" — offset a partir da
   base carregada, seguido do numero de palavras. Diagnostico puro. */
void s3e_module_peek(void) {
    const char *spec = getenv("SIMS3_PEEK");
    if (!spec || !spec[0] || !g_module_base) {
        return;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", spec);
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        unsigned long off = 0, extra = 0;
        unsigned count = 1;
        int deref = 0;
        char *p = tok;
        if (*p == '*') {
            deref = 1;
            p++;
        }
        char *plus = strchr(p, '+');
        if (plus) {
            *plus = 0;
            extra = strtoul(plus + 1, NULL, 16);
        }
        if (sscanf(p, "%lx:%u", &off, &count) < 1) {
            continue;
        }
        if (count > 16) {
            count = 16;
        }
        if (off + 4 > g_module_size) {
            continue;
        }
        uintptr_t addr = g_module_base + off;
        if (deref) {
            uint32_t target = *(const uint32_t *)addr;
            fprintf(stderr, "[peek] *0x%lx+0x%lx = 0x%08x", off, extra, target);
            target += (uint32_t)extra;
            uintptr_t tbegin = 0;
            uintptr_t tend = region_end_of(target, &tbegin);
            if (tend && target + count * 4 <= tend) {
                const uint32_t *w = (const uint32_t *)(uintptr_t)target;
                fprintf(stderr, " ->");
                for (unsigned i = 0; i < count; ++i) {
                    fprintf(stderr, " [%u]=0x%08x(%d)", i, w[i], (int32_t)w[i]);
                }
            } else {
                fprintf(stderr, " (fora do modulo)");
            }
            fprintf(stderr, "\n");
        } else {
            const uint32_t *w = (const uint32_t *)addr;
            fprintf(stderr, "[peek] 0x%lx:", off);
            for (unsigned i = 0; i < count; ++i) {
                fprintf(stderr, " 0x%08x", w[i]);
            }
            fprintf(stderr, "\n");
        }
    }
    fflush(stderr);
}

/* Continua util e CONFIAVEL (nao adivinha layout): diz se um endereco esta numa
   regiao mapeada antes de ler. Usado pelos ganchos (RTTI do objeto devolvido) e
   pelo dump de crash. */
int reg_readable(uintptr_t a, size_t n) {
    uintptr_t begin = 0;
    uintptr_t end = region_end_of(a, &begin);
    return end && a + n <= end;
}

/* ⚠️ O ANDADOR DO REGISTRO FOI REMOVIDO DE PROPOSITO.
   Existiu aqui um `s3e_dump_registry()` (SIMS3_REGDUMP / SIMS3_REGID) que tentava
   percorrer a estrutura do registro do jogo. Ele MENTIA: relatava "0 objetos"
   para ids que os ganchos provam estar presentes (as consultas de id 0 e 1
   devolvem ponteiro valido, medido pelo log de RETORNO). Ele me levou a DUAS
   conclusoes erradas — "o registro esta vazio" e "falta a fase que cria os
   objetos". Instrumento que mente e' pior do que instrumento nenhum.

   O que vale sao os GANCHOS (SIMS3_HOOK_FN / _BL / _VT): leem argumentos, valor
   de RETORNO e RTTI do objeto devolvido, sem precisar adivinhar o layout. Se um
   dia for preciso andar na estrutura de novo, valide primeiro contra eles. */

/* ---- gancho de slot de VTABLE (diagnostico) --------------------------------
   Responde "este metodo virtual chega a ser chamado?" sem gdb e sem reescrever
   instrucao: troca-se a PALAVRA da vtable (o modulo esta mapeado RWX) por um
   trampolim que registra a chamada e segue para o original com os MESMOS
   registradores. SIMS3_HOOK_VT="1e22cc[,offset...]" (offsets do modulo). */
#define HOOK_MAX 8
static void *g_hook_orig[HOOK_MAX];
static const char *g_hook_name[HOOK_MAX];
static unsigned g_hook_calls[HOOK_MAX];
static unsigned g_hook_count;

/* Recebe tambem o `this` (r0 do metodo virtual). Serve para testar guardas de
   saida antecipada: em base+0xbba18 o registro comeca com
   `if (this[0x24c] && this[0x24c][8]) return;` — se essa guarda for verdadeira,
   o metodo e' chamado e NAO registra nada, que e' exatamente o sintoma. */
static uint32_t g_hook_last_id[HOOK_MAX];
static uint32_t g_hook_last_r2[HOOK_MAX];
static uint32_t g_hook_last_r0[HOOK_MAX];
uint32_t g_hook_caller;   /* lr no momento da chamada, gravado pelo trampolim */

static unsigned g_hook_ins[HOOK_MAX];   /* chamadas com criador != 0 (insercao) */
static unsigned g_hook_get[HOOK_MAX];   /* chamadas com criador == 0 (consulta) */
static uint32_t g_hook_ins_max[HOOK_MAX];

/* Decodifica um midp::String (duas indirecoes, texto UTF-16) para o log. */
static void print_midp_string(uint32_t obj) {
    if (obj <= 0x1000u || !reg_readable(obj, 32)) {
        return;
    }
    const uint32_t *w = (const uint32_t *)(uintptr_t)obj;
    uint32_t data = w[2], len = w[5];
    if (data <= 0x1000u || !reg_readable(data, 16)) {
        return;
    }
    const uint32_t *a = (const uint32_t *)(uintptr_t)data;
    uint32_t buf = a[2], blen = a[3];
    if (blen > len) {
        blen = len;
    }
    if (buf <= 0x1000u || blen == 0 || blen > 200 || !reg_readable(buf, blen * 2u)) {
        return;
    }
    const uint16_t *u = (const uint16_t *)(uintptr_t)buf;
    char tmp[201];
    unsigned k = 0;
    for (; k < blen && k < sizeof(tmp) - 1; ++k) {
        uint16_t c = u[k];
        tmp[k] = (c >= 32 && c < 127) ? (char)c : '?';
    }
    tmp[k] = 0;
    fprintf(stderr, "  texto=\"%s\"", tmp);
}

void s3e_hook_log(unsigned idx, uint32_t a0, uint32_t a1, uint32_t a2) {
    void *self = (void *)(uintptr_t)a0;
    if (idx < HOOK_MAX) {
        g_hook_last_id[idx] = a1;
        g_hook_last_r2[idx] = a2;
        g_hook_last_r0[idx] = a0;
        /* Contagem SEM teto: o log detalhado so mostra as primeiras chamadas, e
           por isso eu tinha perdido de vista quantas insercoes acontecem no
           total. Isto conta TODAS. */
        if (a2) {
            g_hook_ins[idx]++;
            if (a1 > g_hook_ins_max[idx]) {
                g_hook_ins_max[idx] = a1;
            }
        } else {
            g_hook_get[idx]++;
        }
    }
    if (idx >= HOOK_MAX) {
        return;
    }
    /* Primeiras chamadas com TODOS os argumentos: e' assim que se descobre QUAIS
       ids estao sendo registrados (r1) e com que criador (r2). */
    /* ids 0 e 1 sao ruido (registro das 5 categorias + consultas constantes da
       tela). O que interessa e' qualquer id de outra PAGINA (>= 2048), que e'
       exatamente onde o jogo quebra. */
    if (g_hook_calls[idx] < 8 || a1 >= 2048u) {
        uintptr_t caller = g_hook_caller;
        unsigned long coff = (g_module_base && caller >= g_module_base &&
                              caller < g_module_base + g_module_size)
                                 ? (unsigned long)(caller - g_module_base)
                                 : 0;
        fprintf(stderr,
                "[hook]   %s de modulo+0x%lx arg(r0=0x%08x, r1=%u [pagina %u slot %u], r2=0x%08x)",
                g_hook_name[idx] ? g_hook_name[idx] : "?", coff, a0, a1, a1 >> 11, a1 & 0x7ffu,
                a2);
        /* Se r2 for um objeto C++, o RTTI diz a CLASSE dele (ABI Itanium:
           vtable em obj+0, typeinfo em vtable-4, nome em typeinfo+4). E' assim
           que se descobre o que sao as 5 "categorias" registradas. */
        if (a2 && reg_readable(a2, 4)) {
            uint32_t vt = *(const uint32_t *)(uintptr_t)a2;
            if (vt >= 4 && reg_readable(vt - 4, 4)) {
                uint32_t ti = *(const uint32_t *)(uintptr_t)(vt - 4);
                if (ti && reg_readable(ti, 8)) {
                    uint32_t nm = ((const uint32_t *)(uintptr_t)ti)[1];
                    if (nm && reg_readable(nm, 2)) {
                        fprintf(stderr, "  classe=%.48s", (const char *)(uintptr_t)nm);
                        print_midp_string(a2);
                    }
                }
            }
        }
        /* SIMS3_HOOK_DEREF="48:3": le r0[48] como vetor e mostra o RTTI do
           elemento [3]. Serve para ver QUEM e' o tratador escolhido por
           mgr[48][tipo] em base+0xfa1a0 — se for o objeto errado, o caminho de
           "nao achou" chama o metodo errado. */
        const char *dr = getenv("SIMS3_HOOK_DEREF");
        unsigned doff = 0, didx = 0;
        if (dr && sscanf(dr, "%u:%u", &doff, &didx) == 2 && a0 > 0x1000u) {
            uint32_t arr = *(const uint32_t *)(uintptr_t)(a0 + doff);
            fprintf(stderr, "  | r0[%u]=0x%08x", doff, arr);
            if (arr > 0x1000u) {
                uint32_t obj = ((const uint32_t *)(uintptr_t)arr)[didx];
                fprintf(stderr, " [%u]=0x%08x", didx, obj);
                const char *nm = NULL;
                if (obj > 0x1000u) {
                    uint32_t vt = *(const uint32_t *)(uintptr_t)obj;
                    if (vt > 0x1000u) {
                        uint32_t ti = *(const uint32_t *)(uintptr_t)(vt - 4);
                        if (ti > 0x1000u) {
                            uint32_t np = ((const uint32_t *)(uintptr_t)ti)[1];
                            if (np > 0x1000u) {
                                nm = (const char *)(uintptr_t)np;
                            }
                        }
                    }
                }
                fprintf(stderr, " tratador=%.48s", nm ? nm : "(sem RTTI)");
            }
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    if (g_hook_calls[idx]++ == 0) {
        fprintf(stderr, "[hook] *** %s CHAMADO (1a vez) this=%p ***\n",
                g_hook_name[idx] ? g_hook_name[idx] : "?", self);
        uintptr_t o = (uintptr_t)self;
        if (o && reg_readable(o, 0x260)) {
            uint32_t f24c = *(const uint32_t *)(o + 0x24c);
            uint32_t f20 = *(const uint32_t *)(o + 0x20);
            fprintf(stderr, "[hook]     this[0x24c]=0x%08x this[0x20]=0x%08x", f24c, f20);
            if (f24c && reg_readable(f24c, 12)) {
                fprintf(stderr, "  this[0x24c][8]=0x%08x  => SAIDA ANTECIPADA %s",
                        ((const uint32_t *)(uintptr_t)f24c)[2],
                        ((const uint32_t *)(uintptr_t)f24c)[2] ? "SIM (nao registra!)" : "nao");
            } else {
                fprintf(stderr, "  => sem saida antecipada (this[0x24c] nulo)");
            }
            fprintf(stderr, "\n");
        }
        fflush(stderr);
    }
}

/* Log do RETORNO: sem ele nao da' para saber se uma consulta ACHOU ou nao.
   O trampolim chama o original e passa o r0 de volta por aqui. */
/* Andador VALIDADO: replica exatamente base+0x41a78..0x41b20 para um id, com o
   idioma corrente (map[32]). Ele so e' usado depois de CONFERIR que reproduz o
   ponteiro que a propria funcao devolveu para um id conhecido — foi a falta
   dessa validacao que fez o andador anterior mentir. */
static uint32_t map_probe(uint32_t map, uint32_t id) {
    if (map <= 0x1000u || !reg_readable(map, 64)) {
        return 0;
    }
    const uint32_t *m = (const uint32_t *)(uintptr_t)map;
    uint32_t cur = m[8], mask1 = m[9], shift = m[10], mask2 = m[11], holder = m[15];
    if (!holder || !reg_readable(holder, 12)) {
        return 0;
    }
    uint32_t data = ((const uint32_t *)(uintptr_t)holder)[2];
    if (!data) {
        return 0;
    }
    uint32_t page = (id & mask1) >> shift;
    uint32_t slot = id & mask2;
    uintptr_t e = data + (uintptr_t)page * 20u;
    if (!reg_readable(e, 20)) {
        return 0;
    }
    uint32_t holder2 = ((const uint32_t *)e)[3];
    if (!holder2 || !reg_readable(holder2, 12)) {
        return 0;
    }
    uint32_t arr = ((const uint32_t *)(uintptr_t)holder2)[2];
    uintptr_t entry = (uintptr_t)arr + (uintptr_t)cur * 20u;
    if (!arr || !reg_readable(entry, 20)) {
        return 0;
    }
    uint32_t list = ((const uint32_t *)entry)[3];
    uint32_t vec = ((const uint32_t *)entry)[2];
    if (!list || !vec || !reg_readable(vec, ((size_t)slot + 1u) * 4u)) {
        return 0;
    }
    return ((const uint32_t *)(uintptr_t)vec)[slot];
}

void s3e_hook_log_ret(unsigned idx, uint32_t ret) {
    if (idx >= HOOK_MAX) {
        return;
    }
    static unsigned found[HOOK_MAX], missed[HOOK_MAX];
    static uint32_t id_min[HOOK_MAX], id_max[HOOK_MAX];
    static int seen[HOOK_MAX];
    uint32_t id = g_hook_last_id[idx];
    if (ret) {
        found[idx]++;
        if (!seen[idx]) {
            seen[idx] = 1;
            id_min[idx] = id_max[idx] = id;
        }
        if (id < id_min[idx]) {
            id_min[idx] = id;
        }
        if (id > id_max[idx]) {
            id_max[idx] = id;
        }
    } else {
        missed[idx]++;
        fprintf(stderr, "[hook]   %s id=%u [pag %u] -> NAO ACHOU (achou=%u errou=%u)\n",
                g_hook_name[idx] ? g_hook_name[idx] : "?", id, id >> 11, found[idx], missed[idx]);
        fflush(stderr);
    }
    /* Uma vez por id: diz a CLASSE C++ do objeto devolvido (RTTI), que e' o que
       revela para que serve o registro. */
    if (ret) {
        static uint32_t shown_id[HOOK_MAX];
        if (shown_id[idx] != id + 1u) {
            shown_id[idx] = id + 1u;
            const char *nm = NULL;
            if (reg_readable(ret, 4)) {
                uint32_t vt = *(const uint32_t *)(uintptr_t)ret;
                if (vt >= 4 && reg_readable(vt - 4, 4)) {
                    uint32_t ti = *(const uint32_t *)(uintptr_t)(vt - 4);
                    if (ti && reg_readable(ti, 8)) {
                        uint32_t p = ((const uint32_t *)(uintptr_t)ti)[1];
                        if (p && reg_readable(p, 2)) {
                            nm = (const char *)(uintptr_t)p;
                        }
                    }
                }
            }
            fprintf(stderr, "[hook]   %s mapa=0x%08x id=%u -> 0x%08x classe=%.48s",
                    g_hook_name[idx] ? g_hook_name[idx] : "?", g_hook_last_r0[idx], id, ret,
                    nm ? nm : "(sem RTTI)");
            /* Valida o andador contra a resposta REAL e, se bater, mede quantos
               ids existem de fato na tabela do idioma corrente. Sem essa
               validacao o andador nao vale nada — foi assim que o anterior
               mentiu. */
            {
                static int validated;
                uint32_t mine = map_probe(g_hook_last_r0[idx], id);
                if (!validated) {
                    fprintf(stderr, "\n[probe] andador id=%u devolve 0x%08x (real 0x%08x) -> %s",
                            id, mine, ret, mine == ret ? "CONFERE" : "NAO CONFERE");
                    if (mine == ret && ret) {
                        validated = 1;
                        unsigned n = 0, hi = 0;
                        for (uint32_t q = 0; q < 8192u; ++q) {
                            if (map_probe(g_hook_last_r0[idx], q)) {
                                n++;
                                hi = q;
                            }
                        }
                        fprintf(stderr, "\n[probe] TABELA do idioma corrente: %u ids validos, "
                                        "maior id=%u",
                                n, hi);
                    }
                }
            }
            /* midp::String: procura nas primeiras palavras do objeto um ponteiro
               para texto legivel, para saber O QUE o mapa guarda de verdade. */
            if (ret > 0x1000u) {
                const uint32_t *w = (const uint32_t *)(uintptr_t)ret;
                /* midp::String: [2] = dados, [5] = comprimento, texto em UTF-16.
                   Sem decodificar o UTF-16 a string parece vazia (o segundo byte
                   e' 0 e uma varredura ASCII para no primeiro caractere). */
                uint32_t data = w[2], len = w[5];
                /* midp::String -> [2] aponta para um midp::array (vtable, ref,
                   buffer, tamanho); o texto de verdade esta em array[2], em
                   UTF-16. Duas indirecoes, nao uma. */
                if (data > 0x1000u && reg_readable(data, 16)) {
                    const uint32_t *a = (const uint32_t *)(uintptr_t)data;
                    uint32_t buf = a[2], blen = a[3];
                    if (blen > len) {
                        blen = len;
                    }
                    if (buf > 0x1000u && blen > 0 && blen < 200 &&
                        reg_readable(buf, blen * 2u)) {
                        const uint16_t *u = (const uint16_t *)(uintptr_t)buf;
                        char tmp[201];
                        unsigned k2 = 0;
                        for (; k2 < blen && k2 < sizeof(tmp) - 1; ++k2) {
                            uint16_t c = u[k2];
                            tmp[k2] = (c >= 32 && c < 127) ? (char)c : '?';
                        }
                        tmp[k2] = 0;
                        fprintf(stderr, "  texto=\"%s\"", tmp);
                    }
                }
            }
            fprintf(stderr, "\n");
            fflush(stderr);
        }
    }
    static unsigned tick[HOOK_MAX];
    if ((++tick[idx] % 400u) == 0u) {
        fprintf(stderr,
                "[hook]   %s: insercoes=%u (maior id inserido=%u) consultas=%u | achou=%u "
                "errou=%u; ids que ACHARAM: %u..%u\n",
                g_hook_name[idx] ? g_hook_name[idx] : "?", g_hook_ins[idx], g_hook_ins_max[idx],
                g_hook_get[idx], found[idx], missed[idx], id_min[idx], id_max[idx]);
        fflush(stderr);
    }
}

void s3e_hook_report(void) {
    for (unsigned i = 0; i < g_hook_count; ++i) {
        fprintf(stderr, "[hook] %s: %u chamadas\n", g_hook_name[i] ? g_hook_name[i] : "?",
                g_hook_calls[i]);
    }
    fflush(stderr);
}

#if defined(__arm__)
#define HOOK_THUNK(N)                                                                              \
    __attribute__((naked)) static void hook_thunk_##N(void) {                                      \
        __asm__ volatile("push {r0-r3, r12, lr}\n"                                                 \
                         "ldr  r12, 2f\n"                                                          \
                         "str  lr, [r12]\n"                                                        \
                         "mov  r3, r2\n"                                                           \
                         "mov  r2, r1\n"                                                           \
                         "mov  r1, r0\n"                                                           \
                         "mov  r0, #" #N "\n"                                                      \
                         "bl   s3e_hook_log\n"                                                     \
                         "pop  {r0-r3, r12, lr}\n"                                                 \
                         "push {r4, lr}\n"                                                         \
                         "ldr  r4, 1f\n"                                                           \
                         "ldr  r4, [r4, #" #N " * 4]\n"                                             \
                         "blx  r4\n"                                                               \
                         "mov  r1, r0\n"                                                           \
                         "push {r0, r1}\n"                                                         \
                         "mov  r0, #" #N "\n"                                                       \
                         "bl   s3e_hook_log_ret\n"                                                 \
                         "pop  {r0, r1}\n"                                                         \
                         "pop  {r4, lr}\n"                                                         \
                         "bx   lr\n"                                                               \
                         "1: .word g_hook_orig\n"                                                  \
                         "2: .word g_hook_caller\n");                                              \
    }
HOOK_THUNK(0)
HOOK_THUNK(1)
HOOK_THUNK(2)
HOOK_THUNK(3)
static void *const g_hook_thunks[4] = {(void *)(uintptr_t)&hook_thunk_0,
                                       (void *)(uintptr_t)&hook_thunk_1,
                                       (void *)(uintptr_t)&hook_thunk_2,
                                       (void *)(uintptr_t)&hook_thunk_3};

/* Gancho de SITIO DE CHAMADA: reescreve o alvo de um `bl` do modulo para um
   veneer nosso, que salta para o trampolim (log) e este segue para o alvo
   ORIGINAL, decodificado do proprio `bl`. Assim da' para espiar funcoes que nao
   estao em vtable nenhuma, sem destruir instrucao do modulo (so o alvo do bl
   muda) e sem gdb. SIMS3_HOOK_BL="bce88[,offset...]" (offsets do modulo). */
static uint8_t *g_bl_pool;
static size_t g_bl_used;

void s3e_install_bl_hooks(void) {
    const char *spec = getenv("SIMS3_HOOK_BL");
    if (!spec || !spec[0] || !g_module_base) {
        return;
    }
    if (!g_bl_pool) {
        /* dentro de +-32 MB do modulo para o `bl` alcancar */
        void *want = (void *)(g_module_base + 0x600000u);
        void *p = mmap(want, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            fprintf(stderr, "[hook] pool de veneers do bl falhou\n");
            return;
        }
        g_bl_pool = p;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", spec);
    static char names[4][32];
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok && g_hook_count < 4;
         tok = strtok_r(NULL, ",", &save)) {
        unsigned long off = strtoul(tok, NULL, 16);
        if (!off || off + 4 > g_module_size) {
            continue;
        }
        uint32_t *site = (uint32_t *)(uintptr_t)(g_module_base + off);
        uint32_t insn = *site;
        if ((insn >> 24) != 0xebu) {
            fprintf(stderr, "[hook] 0x%lx nao e' um bl (0x%08x)\n", off, insn);
            continue;
        }
        int32_t imm = (int32_t)(insn & 0x00ffffffu);
        if (imm & 0x00800000) {
            imm -= 0x01000000;
        }
        uintptr_t target = (uintptr_t)site + 8u + (uintptr_t)(imm * 4);

        unsigned i = g_hook_count++;
        g_hook_orig[i] = (void *)target;
        snprintf(names[i], sizeof(names[i]), "bl@0x%lx->0x%08lx", off, (unsigned long)target);
        g_hook_name[i] = names[i];

        uint32_t *veneer = (uint32_t *)(void *)(g_bl_pool + g_bl_used);
        g_bl_used += 8;
        veneer[0] = 0xe51ff004u; /* ldr pc,[pc,#-4] */
        veneer[1] = (uint32_t)(uintptr_t)g_hook_thunks[i];
        __builtin___clear_cache((char *)veneer, (char *)(veneer + 2));

        intptr_t rel = (intptr_t)(uintptr_t)veneer - (intptr_t)(uintptr_t)site - 8;
        if (rel < -0x02000000 || rel >= 0x02000000 || (rel & 3)) {
            fprintf(stderr, "[hook] veneer fora de alcance para 0x%lx\n", off);
            continue;
        }
        *site = 0xeb000000u | (((uint32_t)(rel >> 2)) & 0x00ffffffu);
        __builtin___clear_cache((char *)site, (char *)(site + 1));
        fprintf(stderr, "[hook] bl em 0x%lx desviado (alvo original 0x%08lx)\n", off,
                (unsigned long)target);
    }
    fflush(stderr);
}

/* Gancho de FUNCAO: reescreve a 1a instrucao da funcao para um salto ao nosso
   trampolim; o trampolim registra os argumentos, re-executa a instrucao salva
   (que e' independente de posicao — um `push`) e volta para funcao+4. Assim da'
   para ver TODAS as chamadas de uma funcao, venham de onde vierem (os 17
   sitios de `bl` do registro, por exemplo). SIMS3_HOOK_FN="419e0". */
void s3e_install_fn_hooks(void) {
    const char *spec = getenv("SIMS3_HOOK_FN");
    if (!spec || !spec[0] || !g_module_base) {
        return;
    }
    if (!g_bl_pool) {
        void *want = (void *)(g_module_base + 0x600000u);
        void *p = mmap(want, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            return;
        }
        g_bl_pool = p;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", spec);
    static char names[4][32];
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok && g_hook_count < 4;
         tok = strtok_r(NULL, ",", &save)) {
        unsigned long off = strtoul(tok, NULL, 16);
        if (!off || off + 8 > g_module_size) {
            continue;
        }
        uint32_t *fn = (uint32_t *)(uintptr_t)(g_module_base + off);
        uint32_t saved = *fn;
        if ((saved & 0xffff0000u) != 0xe92d0000u) {
            fprintf(stderr, "[hook] 0x%lx nao comeca com push (0x%08x)\n", off, saved);
            continue;
        }
        unsigned i = g_hook_count++;
        snprintf(names[i], sizeof(names[i]), "fn@0x%lx", off);
        g_hook_name[i] = names[i];

        /* trampolim de retorno: instrucao salva + salto para funcao+4 */
        uint32_t *tramp = (uint32_t *)(void *)(g_bl_pool + g_bl_used);
        g_bl_used += 16;
        tramp[0] = saved;
        tramp[1] = 0xe51ff004u;
        tramp[2] = (uint32_t)(uintptr_t)(fn + 1);
        __builtin___clear_cache((char *)tramp, (char *)(tramp + 3));
        g_hook_orig[i] = (void *)tramp;

        /* veneer que leva ao nosso thunk (o thunk salta para g_hook_orig[i]) */
        uint32_t *veneer = (uint32_t *)(void *)(g_bl_pool + g_bl_used);
        g_bl_used += 8;
        veneer[0] = 0xe51ff004u;
        veneer[1] = (uint32_t)(uintptr_t)g_hook_thunks[i];
        __builtin___clear_cache((char *)veneer, (char *)(veneer + 2));

        intptr_t rel = (intptr_t)(uintptr_t)veneer - (intptr_t)(uintptr_t)fn - 8;
        if (rel < -0x02000000 || rel >= 0x02000000 || (rel & 3)) {
            fprintf(stderr, "[hook] veneer fora de alcance para 0x%lx\n", off);
            continue;
        }
        *fn = 0xea000000u | (((uint32_t)(rel >> 2)) & 0x00ffffffu); /* b veneer */
        __builtin___clear_cache((char *)fn, (char *)(fn + 1));
        fprintf(stderr, "[hook] funcao 0x%lx instrumentada\n", off);
    }
    fflush(stderr);
}

/* SIMS3_PATCH="off=word[,off=word]" — grava palavras cruas no modulo carregado.
   Usado para experimentos de desvio de fluxo (o modulo esta mapeado RWX).
   NAO e' solucao: e' bisturi de diagnostico, sempre documentado no HANDOFF. */
void s3e_apply_word_patches(void) {
    const char *spec = getenv("SIMS3_PATCH");
    if (!spec || !spec[0] || !g_module_base) {
        return;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", spec);
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        unsigned long off = 0, val = 0;
        if (sscanf(tok, "%lx=%lx", &off, &val) != 2 || off + 4 > g_module_size) {
            continue;
        }
        uint32_t *w = (uint32_t *)(uintptr_t)(g_module_base + off);
        fprintf(stderr, "[patch] modulo+0x%lx: 0x%08x -> 0x%08lx\n", off, *w, val);
        *w = (uint32_t)val;
        __builtin___clear_cache((char *)w, (char *)(w + 1));
    }
    fflush(stderr);
}

void s3e_install_vtable_hooks(void) {
    const char *spec = getenv("SIMS3_HOOK_VT");
    if (!spec || !spec[0] || !g_module_base) {
        return;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", spec);
    static char names[4][32];
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok && g_hook_count < 4;
         tok = strtok_r(NULL, ",", &save)) {
        unsigned long off = strtoul(tok, NULL, 16);
        if (!off || off + 4 > g_module_size) {
            continue;
        }
        uint32_t *word = (uint32_t *)(uintptr_t)(g_module_base + off);
        unsigned i = g_hook_count++;
        g_hook_orig[i] = (void *)(uintptr_t)*word;
        snprintf(names[i], sizeof(names[i]), "vt@0x%lx->0x%08x", off, *word);
        g_hook_name[i] = names[i];
        *word = (uint32_t)(uintptr_t)g_hook_thunks[i];
        __builtin___clear_cache((char *)word, (char *)(word + 1));
        fprintf(stderr, "[hook] instalado em 0x%lx (original 0x%08x)\n", off,
                (uint32_t)(uintptr_t)g_hook_orig[i]);
    }
    fflush(stderr);
}
#else
void s3e_install_vtable_hooks(void) {
}
#endif

void s3e_dump_module_maps(void) {
    maps_slurp();
    if (g_maps_len > 0) {
        ssize_t w = write(2, g_maps_buf, (size_t)g_maps_len);
        (void)w;
    }
}

void s3e_dump_module_stack_from(const char *why, uintptr_t sp) {
    if (!g_module_base || !g_module_size) {
        return;
    }
    uintptr_t begin = 0;
    uintptr_t region_end = region_end_of(sp, &begin);
    if (!region_end) {
        fprintf(stderr, "[bt] %s — sp=0x%08lx NAO esta em regiao mapeada (estouro de pilha)\n",
                why, (unsigned long)sp);
        fflush(stderr);
        return;
    }
    fprintf(stderr, "[bt] regiao da pilha 0x%08lx-0x%08lx, sp=0x%08lx (%lu bytes usados ate o "
                    "topo)\n",
            (unsigned long)begin, (unsigned long)region_end, (unsigned long)sp,
            (unsigned long)(region_end - sp));
    uintptr_t lo = g_module_base;
    uintptr_t hi = g_module_base + g_module_size;
    fprintf(stderr, "[bt] %s — enderecos do modulo na pilha (base=0x%08lx):\n", why,
            (unsigned long)lo);
    /* Numa recursao infinita a pilha e' um padrao repetido: contamos repeticoes
       em vez de imprimir milhares de linhas iguais. */
    uint32_t last = 0;
    int repeat = 0, shown = 0;
    uintptr_t scan_end = sp + 0x20000;
    if (scan_end > region_end) {
        scan_end = region_end;
    }
    for (uintptr_t p = sp; p < scan_end && shown < 60; p += 4) {
        uint32_t v = *(const uint32_t *)p;
        if (v >= lo && v < hi) {
            if (v == last) {
                repeat++;
                continue;
            }
            if (repeat) {
                fprintf(stderr, "[bt]   (x%d)\n", repeat + 1);
                repeat = 0;
            }
            fprintf(stderr, "[bt]   0x%08x\n", v);
            last = v;
            shown++;
        }
    }
    if (repeat) {
        fprintf(stderr, "[bt]   (x%d)\n", repeat + 1);
    }
    fflush(stderr);
}

void s3e_dump_module_stack(const char *why) {
    s3e_dump_module_stack_from(why, (uintptr_t)__builtin_frame_address(0));
}

void s3eDebugOutputString(const char *text) {
    if (text && strstr(text, "terminate called")) {
        s3e_dump_module_stack("terminate");
    }
    if (text) {
        fprintf(stderr, "[game] %s\n", text);
        fflush(stderr);
    }
}

void s3eDebugPrint(int32_t channel, const char *text, int32_t color) {
    (void)color;
    if (text) {
        fprintf(stderr, "[game:%d] %s\n", channel, text);
        fflush(stderr);
    }
}

int32_t s3eDebugGetInt(uint32_t key) {
    (void)key;
    return 0;
}

int32_t s3eDebugIsDebuggerPresent(void) {
    return 0;
}

void s3eDebugTraceLine(const char *text) {
    if (text) {
        fprintf(stderr, "[trace] %s\n", text);
        fflush(stderr);
    }
}

/* A assinatura real da Marmalade traz argumentos; declarar `(void)` jogava fora
   exatamente a informacao que interessa (a condicao e o arquivo/linha que
   falharam). Os quatro primeiros argumentos ainda chegam em r0-r3, entao aqui
   eles sao recebidos e impressos — texto quando o ponteiro for legivel. */
static const char *assert_txt(uintptr_t p) {
    if (p > 0x1000u && reg_readable(p, 4)) {
        const char *s = (const char *)p;
        for (int i = 0; i < 120; i++) {
            if (s[i] == 0) return i ? s : "(vazio)";
            if ((unsigned char)s[i] < 9 || (unsigned char)s[i] > 126) break;
        }
    }
    return NULL;
}

int32_t s3eDebugAssertShow(uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    const char *t0 = assert_txt(a0), *t1 = assert_txt(a1);
    const char *t2 = assert_txt(a2), *t3 = assert_txt(a3);
    fprintf(stderr, "[game] *** ASSERT *** a0=0x%lx%s%s a1=0x%lx%s%s a2=0x%lx%s%s a3=0x%lx%s%s\n",
            (unsigned long)a0, t0 ? " = " : "", t0 ? t0 : "",
            (unsigned long)a1, t1 ? " = " : "", t1 ? t1 : "",
            (unsigned long)a2, t2 ? " = " : "", t2 ? t2 : "",
            (unsigned long)a3, t3 ? " = " : "", t3 ? t3 : "");
    fflush(stderr);
    return 0;
}

int32_t s3eDebugErrorShow(uint32_t flags, const char *text) {
    fprintf(stderr, "[game] *** ERROR (flags=0x%x): %s ***\n", flags, text ? text : "(null)");
    fflush(stderr);
    return 0;
}

int32_t s3eAccelerometerStart(void) {
    return 0;
}
int32_t s3eAccelerometerStop(void) {
    return 0;
}
int32_t s3eAccelerometerGetX(void) {
    return 0;
}
int32_t s3eAccelerometerGetY(void) {
    return 0;
}
int32_t s3eAccelerometerGetZ(void) {
    return 0;
}
int32_t s3eAccelerometerGetInt(uint32_t key) {
    (void)key;
    return 0;
}
int32_t s3eVideoGetInt(uint32_t key) {
    (void)key;
    return 0;
}

int32_t s3eVideoPlay(const char *filename, uint32_t repeat) {
    (void)filename;
    (void)repeat;
    return 0;
}

int32_t s3eVideoStop(void) {
    return 0;
}

int32_t s3eVideoResume(void) {
    return 0;
}

/* The Sims 3 (NextOS): o loader base stubava toda a familia s3eCompression
   devolvendo ERRO. Neste jogo isso NAO e' cosmetico: as 287 texturas do
   res.dz sao PNG DE VERDADE (magic 89504e47 medido apos o LZMA do DTRZ) e o
   decoder PNG do IwGx passa o stream zlib do IDAT por s3eCompressionDecomp*.
   Com o stub, toda textura sairia vazia. Implementado por cima do zlib do
   sistema (dlopen, como EGL/GLES — sem dependencia de build). */
struct s3e_zstream {
    /* espelha o inicio de z_stream do zlib (ABI estavel ha decadas) */
    const uint8_t *next_in;
    unsigned avail_in;
    unsigned long total_in;
    uint8_t *next_out;
    unsigned avail_out;
    unsigned long total_out;
    const char *msg;
    void *state;
    void *zalloc, *zfree, *opaque;
    int data_type;
    unsigned long adler, reserved;
};

static void *g_zlib;
static int (*z_inflateInit2_)(struct s3e_zstream *, int, const char *, int);
static int (*z_inflate)(struct s3e_zstream *, int);
static int (*z_inflateEnd)(struct s3e_zstream *);

struct s3e_decomp_ctx {
    struct s3e_zstream z;
    int started;
    int window_bits;
    int finished;
};

static int zlib_ready(void) {
    if (g_zlib) {
        return z_inflateInit2_ && z_inflate && z_inflateEnd;
    }
    const char *names[] = {"libz.so.1", "libz.so", NULL};
    g_zlib = open_first(names);
    if (!g_zlib) {
        fprintf(stderr, "[compress] libz nao encontrada — PNG/zlib indisponivel\n");
        return 0;
    }
    z_inflateInit2_ = dlsym(g_zlib, "inflateInit2_");
    z_inflate = dlsym(g_zlib, "inflate");
    z_inflateEnd = dlsym(g_zlib, "inflateEnd");
    if (!z_inflateInit2_ || !z_inflate || !z_inflateEnd) {
        fprintf(stderr, "[compress] libz sem simbolos de inflate\n");
        return 0;
    }
    return 1;
}

/* s3eCompressionAlgorithm: 0 = ZLIB (com cabecalho), 1 = GZIP, 2 = DEFLATE cru. */
static int window_bits_for(uint32_t type) {
    switch (type) {
    case 1:
        return 15 + 16; /* gzip */
    case 2:
        return -15; /* raw deflate */
    default:
        return 15; /* zlib */
    }
}

/* Falha de descompressao e' MUDA: o decoder PNG do IwGx recebe -1, desiste da
   textura e o widget vai pra tela com a cor chapada do material — sem erro de
   GL, sem arquivo faltando, sem nada no log. Por isso a FALHA e' registrada em
   TODA corrida (com teto), nao so' sob SIMS3_COMP_LOG. Mesma regua do
   PROGRAM LINK FAIL. */
static unsigned g_comp_fail = 0;
static void comp_fail_report(const char *onde, int rc, unsigned in_len, unsigned out_len) {
    g_comp_fail++;
    if (g_comp_fail <= 20) {
        fprintf(stderr, "[compress] FALHA #%u em %s: rc=%d entrada=%u saida=%u\n", g_comp_fail,
                onde, rc, in_len, out_len);
        fflush(stderr);
    }
}
unsigned s3e_comp_fail_count(void) {
    return g_comp_fail;
}

static int comp_log(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("SIMS3_COMP_LOG");
        v = e && e[0] == '1';
    }
    return v;
}

void *s3eCompressionDecompInit(uint32_t type) {
    if (comp_log()) {
        const uint32_t *frame = (const uint32_t *)(const void *)&type;
        fprintf(stderr, "[compress] DecompInit(a0=0x%x) lr=%p args=%08x %08x %08x\n", type,
                __builtin_return_address(0), frame[1], frame[2], frame[3]);
    }
    if (!zlib_ready()) {
        return NULL;
    }
    struct s3e_decomp_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }
    ctx->window_bits = window_bits_for(type);
    if (z_inflateInit2_(&ctx->z, ctx->window_bits, "1.2.11", (int)sizeof(struct s3e_zstream)) !=
        0) {
        free(ctx);
        return NULL;
    }
    ctx->started = 1;
    return ctx;
}

int32_t s3eCompressionDecompRead(void *context, const void *source, uint32_t source_len,
                                 void *target, uint32_t *target_len) {
    (void)0;
    struct s3e_decomp_ctx *ctx = context;
    if (comp_log()) {
        const uint32_t *frame = (const uint32_t *)(const void *)&target_len;
        fprintf(stderr,
                "[compress] DecompRead(a0=%p a1=%p a2=%u a3=%p) lr=%p stack=%08x %08x %08x %08x\n",
                context, source, source_len, target, __builtin_return_address(0), frame[1],
                frame[2], frame[3], frame[4]);
    }
    if (!ctx || !ctx->started || !target || !target_len) {
        comp_fail_report("DecompRead/args", 0, source_len, 0);
        if (target_len) {
            *target_len = 0;
        }
        return -1;
    }
    ctx->z.next_in = source;
    ctx->z.avail_in = source_len;
    ctx->z.next_out = target;
    ctx->z.avail_out = *target_len;
    unsigned before = ctx->z.avail_out;
    int rc = z_inflate(&ctx->z, 0 /* Z_NO_FLUSH */);
    *target_len = before - ctx->z.avail_out;
    if (rc == 1 /* Z_STREAM_END */) {
        ctx->finished = 1;
        return 0;
    }
    if (rc != 0) {
        comp_fail_report("DecompRead", rc, source_len, *target_len);
        return -1;
    }
    return 0;
}

int32_t s3eCompressionDecompFinal(void *context) {
    struct s3e_decomp_ctx *ctx = context;
    if (comp_log()) {
        fprintf(stderr, "[compress] DecompFinal(ctx=%p)\n", context);
    }
    if (!ctx) {
        return -1;
    }
    if (ctx->started) {
        z_inflateEnd(&ctx->z);
    }
    free(ctx);
    return 0;
}

/* The Sims 3 (NextOS): a assinatura REAL de s3eCompressionDecomp, medida no
   proprio modulo (call site em 0x4a12c14c, args montados em 0x4a12c114..0x4a12c14c):

     ldr r0,[r3,#4]    ; src           (dado comprimido)
     ldr r1,[r3,#12]   ; srcLen
     add r2,sp,#16 / str r0,[r2,#-4]!  ; r2 = &destBuffer  (PONTEIRO PRA PONTEIRO)
     add r3,sp,#8      ; r3 = &destLen (tamanho ja calculado, ex. 196864)
     str r1,[sp]       ; arg5 = 2      (algoritmo)

   => s3eCompressionDecomp(const void* src, uint32 srcLen, void** ppDest,
                           uint32* pDestLen, uint32 alg)

   O loader base declarava o 3o argumento como o BUFFER (void* target). Com
   isso escreviamos 192 KB DENTRO DA PILHA do modulo -> SIGSEGV em libz a ~3,8 KB
   do topo. O buffer de verdade e' *ppDest (malloc do proprio jogo).
   O algoritmo vem por numero (2 = zlib neste build), mas detectamos pelo
   CABECALHO do stream — regua da casa: medir por capacidade, nao pela string. */
static int window_bits_detect(const uint8_t *src, uint32_t len, uint32_t alg) {
    if (len >= 2) {
        if (src[0] == 0x1f && src[1] == 0x8b) {
            return 15 + 16; /* gzip */
        }
        if ((src[0] & 0x0f) == 8 && ((((uint32_t)src[0] << 8) | src[1]) % 31u) == 0) {
            return 15; /* zlib */
        }
    }
    return window_bits_for(alg);
}

int32_t s3eCompressionDecomp(const void *source, uint32_t source_len, void **pp_dest,
                             uint32_t *p_dest_len, uint32_t alg) {
    void *dest = pp_dest ? *pp_dest : NULL;
    uint32_t capacity = p_dest_len ? *p_dest_len : 0;

    if (comp_log()) {
        const uint8_t *sb = source;
        fprintf(stderr,
                "[compress] Decomp(src=%p len=%u dest=%p cap=%u alg=%u) hdr=%02x %02x\n", source,
                source_len, dest, capacity, alg, source_len ? sb[0] : 0,
                source_len > 1 ? sb[1] : 0);
    }

    if (!source || !source_len || !dest || !capacity) {
        comp_fail_report("Decomp/args", 0, source_len, capacity);
        if (p_dest_len) {
            *p_dest_len = 0;
        }
        return -1;
    }
    if (!zlib_ready()) {
        comp_fail_report("Decomp/zlib", 0, source_len, capacity);
        *p_dest_len = 0;
        return -1;
    }

    struct s3e_zstream z;
    memset(&z, 0, sizeof(z));
    int bits = window_bits_detect(source, source_len, alg);
    if (z_inflateInit2_(&z, bits, "1.2.11", (int)sizeof(z)) != 0) {
        comp_fail_report("Decomp/inflateInit", bits, source_len, capacity);
        *p_dest_len = 0;
        return -1;
    }
    z.next_in = source;
    z.avail_in = source_len;
    z.next_out = dest;
    z.avail_out = capacity;
    int rc = z_inflate(&z, 4 /* Z_FINISH */);
    uint32_t produced = capacity - z.avail_out;
    z_inflateEnd(&z);

    if (rc != 1 /* Z_STREAM_END */ && produced == 0) {
        comp_fail_report("Decomp/inflate", rc, source_len, capacity);
        *p_dest_len = 0;
        return -1;
    }
    /* Saida PARCIAL tambem estraga a textura, e passava calada: o PNG chega
       truncado e o widget fica chapado do mesmo jeito. */
    if (rc != 1 && produced < capacity) {
        comp_fail_report("Decomp/parcial", rc, source_len, produced);
    }
    *p_dest_len = produced;
    if (comp_log()) {
        fprintf(stderr, "[compress] Decomp ok: %u -> %u bytes (rc=%d)\n", source_len, produced,
                rc);
    }
    return 0;
}

uint32_t s3eInetHtonl(uint32_t value) {
    return htonl(value);
}
uint32_t s3eInetNtohl(uint32_t value) {
    return ntohl(value);
}
uint16_t s3eInetHtons(uint16_t value) {
    return htons(value);
}
uint16_t s3eInetNtohs(uint16_t value) {
    return ntohs(value);
}

int32_t s3eInetAton(const char *address, uint32_t *out) {
    struct in_addr parsed;
    if (!address || inet_aton(address, &parsed) == 0) {
        return 1;
    }
    if (out) {
        *out = parsed.s_addr;
    }
    return 0;
}

const char *s3eInetNtoa(uint32_t address) {
    static __thread char buffer[INET_ADDRSTRLEN];
    struct in_addr in;
    in.s_addr = address;
    const char *result = inet_ntop(AF_INET, &in, buffer, sizeof(buffer));
    return result ? buffer : "0.0.0.0";
}

const char *s3eInetToString(uint32_t address) {
    return s3eInetNtoa(address);
}

int32_t s3eInetLookup(const char *hostname, uint32_t *out, void *callback, void *user_data) {
    (void)hostname;
    (void)callback;
    (void)user_data;
    if (out) {
        *out = 0;
    }
    return -1;
}

int32_t s3eInetLookupCancel(void *lookup) {
    (void)lookup;
    return 0;
}
void *s3eSocketCreate(uint32_t type, uint32_t protocol, uint32_t flags) {
    (void)type;
    (void)protocol;
    (void)flags;
    return NULL;
}
int32_t s3eSocketClose(void *socket) {
    (void)socket;
    return 0;
}
int32_t s3eSocketBind(void *socket, const void *address, uint16_t port) {
    (void)socket;
    (void)address;
    (void)port;
    return 0;
}
int32_t s3eSocketListen(void *socket, int32_t backlog) {
    (void)socket;
    (void)backlog;
    return 0;
}
void *s3eSocketAccept(void *socket, void *address) {
    (void)socket;
    (void)address;
    return NULL;
}
int32_t s3eSocketConnect(void *socket, const void *address, uint16_t port) {
    (void)socket;
    (void)address;
    (void)port;
    return -1;
}
int32_t s3eSocketSend(void *socket, const void *buffer, uint32_t length, uint32_t flags) {
    (void)socket;
    (void)buffer;
    (void)length;
    (void)flags;
    return -1;
}
int32_t s3eSocketSendTo(void *socket, const void *buffer, uint32_t length, uint32_t flags,
                        const void *address, uint16_t port) {
    (void)socket;
    (void)buffer;
    (void)length;
    (void)flags;
    (void)address;
    (void)port;
    return -1;
}
int32_t s3eSocketRecv(void *socket, void *buffer, uint32_t length, uint32_t flags) {
    (void)socket;
    (void)buffer;
    (void)length;
    (void)flags;
    return 0;
}
int32_t s3eSocketRecvFrom(void *socket, void *buffer, uint32_t length, uint32_t flags,
                          void *address) {
    (void)socket;
    (void)buffer;
    (void)length;
    (void)flags;
    (void)address;
    return 0;
}
int32_t s3eSocketReadable(void *socket) {
    (void)socket;
    return 0;
}
int32_t s3eSocketWritable(void *socket) {
    (void)socket;
    return 0;
}
int32_t s3eSocketGetInt(void *socket, uint32_t key) {
    (void)socket;
    (void)key;
    return 0;
}
int32_t s3eSocketGetError(void) {
    return -1;
}
const char *s3eSocketGetString(uint32_t key) {
    (void)key;
    return "network disabled";
}
int32_t s3eSocketGetLocalName(void *socket, void *address) {
    (void)socket;
    (void)address;
    return 1;
}
int32_t s3eSocketGetPeerName(void *socket, void *address) {
    (void)socket;
    (void)address;
    return 1;
}

int32_t s3eMemoryGetInt(uint32_t key) {
    (void)key;
    return 768 * 1024 * 1024;
}

int32_t s3eMemorySetInt(uint32_t key, int32_t value) {
    (void)key;
    (void)value;
    return 0;
}

int32_t s3eMemorySetUserMemMgr(void *mgr) {
    if (!mgr) {
        memset(&g_user_mem_mgr, 0, sizeof(g_user_mem_mgr));
        g_user_mem_mgr_set = 0;
        return 0;
    }
    struct s3e_user_mem_mgr candidate;
    memcpy(&candidate, mgr, sizeof(candidate));
    if (!candidate.alloc || !candidate.realloc || !candidate.free) {
        g_memory_error = EINVAL;
        return 1;
    }
    g_user_mem_mgr = candidate;
    g_user_mem_mgr_set = 1;
    return 0;
}

int32_t s3eMemoryGetUserMemMgr(void *out) {
    if (!out) {
        g_memory_error = EINVAL;
        return 1;
    }
    struct s3e_user_mem_mgr current = g_user_mem_mgr;
    if (!g_user_mem_mgr_set) {
        current.alloc = (void *)(uintptr_t)&s3eMallocBase;
        current.realloc = (void *)(uintptr_t)&s3eReallocBase;
        current.free = (void *)(uintptr_t)&s3eFreeBase;
    }
    memcpy(out, &current, sizeof(current));
    return 0;
}

int32_t s3eMemoryHeapCreate(uint32_t heap_index) {
    if (heap_index >= sizeof(g_heaps) / sizeof(g_heaps[0])) {
        g_memory_error = EINVAL;
        return 1;
    }
    if (!g_heaps[heap_index].base) {
        uint32_t size = heap_index == 0 ? 128u * 1024u * 1024u : 16u * 1024u * 1024u;
        g_heaps[heap_index].base = calloc(1, size);
        if (!g_heaps[heap_index].base) {
            g_memory_error = ENOMEM;
            return 1;
        }
        g_heaps[heap_index].size = size;
    }
    return 0;
}

int32_t s3eMemoryHeapDestroy(uint32_t heap_index) {
    if (heap_index >= sizeof(g_heaps) / sizeof(g_heaps[0])) {
        g_memory_error = EINVAL;
        return 1;
    }
    free(g_heaps[heap_index].base);
    g_heaps[heap_index].base = NULL;
    g_heaps[heap_index].size = 0;
    return 0;
}

void *s3eMemoryHeapAddress(uint32_t heap_index) {
    if (heap_index >= sizeof(g_heaps) / sizeof(g_heaps[0])) {
        g_memory_error = EINVAL;
        return NULL;
    }
    if (!g_heaps[heap_index].base && s3eMemoryHeapCreate(heap_index) != 0) {
        return NULL;
    }
    return g_heaps[heap_index].base;
}

int32_t s3eMemoryGetError(void) {
    return g_memory_error;
}
const char *s3eMemoryGetErrorString(void) {
    return strerror(g_memory_error);
}

int32_t s3eSurfaceRegister(uint32_t id, void *callback, void *user_data) {
    (void)id;
    (void)callback;
    (void)user_data;
    return 0;
}
int32_t s3eSurfaceUnRegister(uint32_t id, void *callback) {
    (void)id;
    (void)callback;
    return 0;
}

int32_t s3eSurfaceGetInt(uint32_t key) {
    int32_t ret;
    switch (key) {
    case 0: /* S3E_SURFACE_WIDTH */
        ret = g_native_window.width;
        break;
    case 1: /* S3E_SURFACE_HEIGHT */
        ret = g_native_window.height;
        break;
    case 2: /* S3E_SURFACE_PITCH */
        ret = (int32_t)(g_native_window.width * sizeof(uint32_t));
        break;
    case 3: /* S3E_SURFACE_PIXEL_TYPE */
        ret = 4;
        break;
    /* NFS Shift (NextOS): keys 4/5 = S3E_SURFACE_DEVICE_WIDTH/HEIGHT — o NFS
       Shift monta o glViewport a cada frame a partir destas (nao das keys 0/1);
       o loader base devolvia 0 -> glViewport(0,0,0,0) -> tudo clipado (tela
       branca). Devolvemos as dimensoes da surface (device == surface aqui). */
    case 4: /* S3E_SURFACE_DEVICE_WIDTH */
        ret = g_native_window.width;
        break;
    case 5: /* S3E_SURFACE_DEVICE_HEIGHT */
        ret = g_native_window.height;
        break;
    case 11:
        /* Medida: o jogo consulta a chave 11 e ate' agora recebia 0 em silencio.
           Nesta faixa do s3eSurface do Marmalade mora a DPI da tela, e interface
           escalada por DPI=0 colapsa — que e' o formato da lasca chapada vista
           no Create-A-Sim. Ajustavel por SIMS3_SURF11 para MEDIR antes de cravar
           uma semantica. */
        {
            const char *e = getenv("SIMS3_SURF11");
            ret = e ? atoi(e) : 0;
        }
        break;
    default:
        /* 🚨 Chave desconhecida devolvendo 0 em SILENCIO ja' derrubou este port
           uma vez: era a chave 4 de s3eFileGetFileInt (o tamanho do arquivo).
           Uma consulta de layout respondida com 0 colapsa a geometria do widget
           sem erro nenhum — que e' exatamente o sintoma do Create-A-Sim. Toda
           chave que cai aqui fala, uma vez cada. */
        {
            static uint32_t vistas[32];
            static unsigned n = 0;
            int repetida = 0;
            for (unsigned i = 0; i < n; ++i) {
                if (vistas[i] == key) {
                    repetida = 1;
                    break;
                }
            }
            if (!repetida) {
                if (n < 32) {
                    vistas[n++] = key;
                }
                fprintf(stderr, "[s3e] *** SurfaceGetInt CHAVE DESCONHECIDA %u -> 0 ***\n", key);
                fflush(stderr);
            }
        }
        ret = 0;
        break;
    }
    if (getenv("SIMS3_GL_LOG")) {
        fprintf(stderr, "[s3e] SurfaceGetInt(key=%u) -> %d\n", key, ret);
    }
    return ret;
}

void *s3eSurfacePtr(void) {
    return g_surface_pixels;
}
int32_t s3eSurfaceSetup(void) {
    return 0;
}
int32_t s3eSurfaceShow(void) {
    return 0;
}
int32_t s3eGLRegister(uint32_t id, void *callback, void *user_data) {
    (void)id;
    (void)callback;
    (void)user_data;
    return 0;
}
int32_t s3eGLUnRegister(uint32_t id, void *callback) {
    (void)id;
    (void)callback;
    return 0;
}

int32_t s3eGLGetInt(uint32_t key) {
    int32_t ret;
    switch (key) {
    case 0:
        ret = g_native_window.width;
        break;
    case 1:
        ret = g_native_window.height;
        break;
    case 2:
        ret = 2;
        break;
    default:
        ret = 0;
        break;
    }
    if (getenv("SIMS3_GL_LOG")) {
        fprintf(stderr, "[s3e] GLGetInt(key=%u) -> %d\n", key, ret);
    }
    return ret;
}

void *s3eGLGetNativeWindow(void) {
    if (getenv("SIMS3_GL_LOG")) {
        fprintf(stderr, "[s3e] GLGetNativeWindow -> %p {w=%u,h=%u}\n", (void *)&g_native_window,
                g_native_window.width, g_native_window.height);
    }
    return &g_native_window;
}

uintptr_t s3eThreadGetCurrent(void) {
    static struct s3e_host_thread main_thread;
    if (!main_thread.magic) {
        main_thread.magic = 0x54485244u;
        main_thread.id = pthread_self();
    }
    return 4000;
}

void *s3eMutexCreate(void) {
    struct s3e_host_mutex *mutex = calloc(1, sizeof(*mutex));
    if (!mutex) {
        return NULL;
    }
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int rc = pthread_mutex_init(&mutex->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    if (rc != 0) {
        free(mutex);
        return NULL;
    }
    mutex->magic = S3E_HOST_MUTEX_MAGIC;
    return mutex;
}

int32_t s3eMutexDestroy(void *handle) {
    struct s3e_host_mutex *mutex = handle;
    if (!mutex || mutex->magic != S3E_HOST_MUTEX_MAGIC) {
        return 1;
    }
    mutex->magic = 0;
    pthread_mutex_destroy(&mutex->mutex);
    free(mutex);
    return 0;
}

int32_t s3eMutexAcquire(void *handle, int32_t timeout_ms) {
    struct s3e_host_mutex *mutex = handle;
    if (!mutex || mutex->magic != S3E_HOST_MUTEX_MAGIC) {
        return 1;
    }
    if (timeout_ms == 0) {
        return pthread_mutex_trylock(&mutex->mutex) == 0 ? 0 : 1;
    }
    return pthread_mutex_lock(&mutex->mutex) == 0 ? 0 : 1;
}

int32_t s3eMutexRelease(void *handle) {
    struct s3e_host_mutex *mutex = handle;
    if (!mutex || mutex->magic != S3E_HOST_MUTEX_MAGIC) {
        return 1;
    }
    return pthread_mutex_unlock(&mutex->mutex) == 0 ? 0 : 1;
}

uintptr_t s3eReturn0(void) {
    return 0;
}
int32_t s3eReturnMinus1(void) {
    return -1;
}
uintptr_t s3eStub(void) {
    return 0;
}

int32_t s3eTouchpadInit(void) {
    return 1;
}
void s3eTouchpadTerminate(void) {}
int32_t s3eTouchpadGetInt(uint32_t key) {
    switch (key) {
    case 0:
        return 1;
    case 1:
        return XPERIA_TOUCHPAD_WIDTH;
    case 2:
        return XPERIA_TOUCHPAD_HEIGHT;
    default:
        return -1;
    }
}

int32_t s3eTouchpadRegister(uint32_t id, void *callback, void *user_data) {
    if (id < sizeof(g_touchpad_callbacks) / sizeof(g_touchpad_callbacks[0])) {
        g_touchpad_callbacks[id].callback = callback;
        g_touchpad_callbacks[id].user_data = user_data;
    }
    return 0;
}

int32_t s3eTouchpadUnRegister(uint32_t id, void *callback) {
    if (id < sizeof(g_touchpad_callbacks) / sizeof(g_touchpad_callbacks[0]) &&
        (!callback || callback == g_touchpad_callbacks[id].callback)) {
        g_touchpad_callbacks[id].callback = NULL;
        g_touchpad_callbacks[id].user_data = NULL;
    }
    return 0;
}

int32_t isDeviceCallbackRegister(void *callback, void *user_data) {
    (void)callback;
    (void)user_data;
    return 0;
}
int32_t isDeviceCallbackUnregister(void *callback) {
    (void)callback;
    return 0;
}
int32_t isDeviceSetTabletThreshold(int32_t threshold) {
    return threshold;
}
int32_t isDeviceGetDisplayType(void) {
    return 2;
}

void *isDeviceGetExternalResources(void) {
    memset(g_is_device_resources, 0, sizeof(g_is_device_resources));
    g_is_device_resources[0x10] = 1;
    char *data_path = (char *)g_is_device_resources + IS_DEVICE_RESOURCE_PATH_A;
    char *expansion_path = (char *)g_is_device_resources + IS_DEVICE_RESOURCE_PATH_B;
    const char assets_suffix[] = "/assets/";
    size_t data_root_len = strnlen(g_root, IS_DEVICE_RESOURCE_PATH_LEN - 2);
    size_t expansion_root_len =
        strnlen(g_root, IS_DEVICE_RESOURCE_PATH_LEN - sizeof(assets_suffix));
    memcpy(data_path, g_root, data_root_len);
    data_path[data_root_len++] = '/';
    data_path[data_root_len] = 0;
    memcpy(expansion_path, g_root, expansion_root_len);
    memcpy(expansion_path + expansion_root_len, assets_suffix, sizeof(assets_suffix));
    return g_is_device_resources;
}

/* ==== The Sims 3 (NextOS): extensao s3eThread ============================
   MEDIDO no modulo: em 0x4a188e18 o runtime do jogo pergunta ao host pela
   extensao de hash 0xcc0b4a28 com um buffer de 80 bytes (20 ponteiros). Se a
   chamada falha, ele marca "threads indisponiveis" e o pthread_mutex_lock
   interno (0x4a18981c) passa a devolver -1 SEMPRE — o que faz o
   __cxa_guard_acquire da libstdc++ (0x4a18e140) lancar
   __gnu_cxx::__concurrence_lock_error e o jogo chamar s3eDeviceAbort.
   Sem esta extensao o jogo NAO passa do primeiro static local. Era esse o
   "abort sem mensagem" — o loader base descartava o canal de debug.

   A ordem dos 20 slots foi MEDIDA, nao adivinhada:
     - os wrappers do modulo (0x4a188e88..0x4a1895c0) sao TRANSPARENTES: checam
       disponibilidade e fazem `ldr r3,[tabela,#off]; blx r3` sem tocar em
       r0-r3. Logo o offset do `ldr` da o indice do slot e os argumentos sao os
       do chamador;
     - slots realmente usados por este build: 3,4,6,8,9,11,12,13,14,16,17;
     - **slots 0,1,2 (criar/juntar thread) NUNCA sao chamados** — este jogo so
       precisa de LOCKS e de IDENTIDADE DE THREAD, nao cria thread nenhuma;
     - slot 4 e' chamado sem argumento nenhum (wrapper 0x4a188f68 nao monta
       args) => s3eThreadGetCurrent;
     - slot 9 devolve um handle que reaparece como r0 de 12 e 13 => LockCreate;
     - slot 12 e' chamado por pthread_mutex_lock com (lock, 0xffffffff) =>
       LockAcquire(lock, timeout_ms), -1 = infinito;
     - slot 13 e' chamado com (lock) logo apos => LockRelease.
   Como o processo e' de UMA thread so (Marmalade e' cooperativo, s3eDeviceYield),
   mutex recursivo de verdade satisfaz o contrato sem inventar comportamento. */

#define S3E_THREAD_HASH 0xcc0b4a28u
#define S3E_THREAD_SLOTS 20
#define S3E_THREAD_MAX_LOCKS 256

static int thread_log(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("SIMS3_THREAD_LOG");
        v = e && e[0] == '1';
    }
    return v;
}

/* Contagem por slot, SEMPRE ligada: o Create-A-Sim congela enquanto o menu
   anima, com o laco identico. Se o CAS pede um slot que hoje e' so' um toco
   ("ok, 0"), a diferenca aparece aqui — e nao aparece em log nenhum sem isto. */
unsigned long g_thread_slot_calls[20];
static void thread_trace(const char *what, uint32_t a, uint32_t b) {
    static int budget = 5000;
    if (thread_log() && budget > 0) {
        budget--;
        fprintf(stderr, "[thread] %s(0x%08x, 0x%08x)\n", what, a, b);
        if (budget == 0) {
            fprintf(stderr, "[thread] (log truncado)\n");
        }
        fflush(stderr);
    }
}

struct s3e_lock {
    pthread_mutex_t mutex;
    int used;
};

static struct s3e_lock g_s3e_locks[S3E_THREAD_MAX_LOCKS];
static size_t g_s3e_lock_count;
static pthread_mutex_t g_s3e_lock_pool = PTHREAD_MUTEX_INITIALIZER;

static S3E_SOFTFP void *s3e_thread_lock_create(void) {
    pthread_mutex_lock(&g_s3e_lock_pool);
    struct s3e_lock *lock = NULL;
    if (g_s3e_lock_count < S3E_THREAD_MAX_LOCKS) {
        lock = &g_s3e_locks[g_s3e_lock_count++];
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&lock->mutex, &attr);
        pthread_mutexattr_destroy(&attr);
        lock->used = 1;
    }
    pthread_mutex_unlock(&g_s3e_lock_pool);
    thread_trace("LockCreate", (uint32_t)(uintptr_t)lock, 0);
    return lock;
}

static int lock_is_ours(const struct s3e_lock *lock) {
    return lock >= g_s3e_locks && lock < g_s3e_locks + S3E_THREAD_MAX_LOCKS && lock->used;
}

static S3E_SOFTFP int32_t s3e_thread_lock_destroy(void *handle) {
    struct s3e_lock *lock = handle;
    thread_trace("LockDestroy", (uint32_t)(uintptr_t)handle, 0);
    if (!lock_is_ours(lock)) {
        return -1;
    }
    /* O pool e' estatico e o jogo destroi poucos locks; marcamos como livre sem
       reaproveitar o slot (reaproveitar arriscaria um handle antigo ainda vivo). */
    pthread_mutex_destroy(&lock->mutex);
    lock->used = 0;
    return 0;
}

static S3E_SOFTFP int32_t s3e_thread_lock_acquire(void *handle, int32_t timeout_ms) {
    struct s3e_lock *lock = handle;
    thread_trace("LockAcquire", (uint32_t)(uintptr_t)handle, (uint32_t)timeout_ms);
    if (!lock_is_ours(lock)) {
        return -1;
    }
    if (timeout_ms == 0) {
        return pthread_mutex_trylock(&lock->mutex) == 0 ? 0 : -1;
    }
    return pthread_mutex_lock(&lock->mutex) == 0 ? 0 : -1;
}

static S3E_SOFTFP int32_t s3e_thread_lock_release(void *handle) {
    struct s3e_lock *lock = handle;
    thread_trace("LockRelease", (uint32_t)(uintptr_t)handle, 0);
    if (!lock_is_ours(lock)) {
        return -1;
    }
    return pthread_mutex_unlock(&lock->mutex) == 0 ? 0 : -1;
}

/* MEDIDO em 0x4a186af0 e 0x4a18a1c0/0x4a18a0dc: o valor devolvido por
   s3eThreadGetCurrent NAO e' um ponteiro opaco — e' um INDICE DE THREAD que o
   TLS do modulo valida com `sub rX, r0, #4000; cmp rX, #128; bhi <falha>`, ou
   seja tem de cair em [4000, 4128]. E o proprio modulo usa 4000 como id da
   thread principal quando decide que nao ha threads (0x4a186b14).
   Devolver um ponteiro (pthread_self) fazia o setspecific falhar em silencio;
   ai o getter de singleton em 0x4a187f2c nunca achava o objeto registrado,
   chamava a si mesmo (0x4a187f9c) e o processo morria por ESTOURO DE PILHA
   dentro do malloc — sem uma linha de log. */
#define S3E_THREAD_MAIN_ID 4000u

static S3E_SOFTFP void *s3e_thread_get_current(void) {
    return (void *)(uintptr_t)S3E_THREAD_MAIN_ID;
}

/* Variaveis de condicao. MEDIDO: o slot 14 e' o unico "desconhecido" chamado
   nesta fase (2x) e devolver 0 (handle NULO) faz o pthread_cond_broadcast do
   modulo falhar -> __gnu_cxx::__concurrence_broadcast_error -> abort. */
struct s3e_cond {
    pthread_cond_t cond;
    int used;
};

static struct s3e_cond g_s3e_conds[S3E_THREAD_MAX_LOCKS];
static size_t g_s3e_cond_count;

static S3E_SOFTFP void *s3e_thread_cond_create(void) {
    pthread_mutex_lock(&g_s3e_lock_pool);
    struct s3e_cond *cond = NULL;
    if (g_s3e_cond_count < S3E_THREAD_MAX_LOCKS) {
        cond = &g_s3e_conds[g_s3e_cond_count++];
        pthread_cond_init(&cond->cond, NULL);
        cond->used = 1;
    }
    pthread_mutex_unlock(&g_s3e_lock_pool);
    thread_trace("CondCreate", (uint32_t)(uintptr_t)cond, 0);
    return cond;
}

static int cond_is_ours(const struct s3e_cond *cond) {
    return cond >= g_s3e_conds && cond < g_s3e_conds + S3E_THREAD_MAX_LOCKS && cond->used;
}

static S3E_SOFTFP int32_t s3e_thread_cond_destroy(void *handle) {
    struct s3e_cond *cond = handle;
    thread_trace("CondDestroy", (uint32_t)(uintptr_t)handle, 0);
    if (!cond_is_ours(cond)) {
        return -1;
    }
    pthread_cond_destroy(&cond->cond);
    cond->used = 0;
    return 0;
}

static S3E_SOFTFP int32_t s3e_thread_cond_signal(void *handle) {
    struct s3e_cond *cond = handle;
    thread_trace("CondSignal", (uint32_t)(uintptr_t)handle, 0);
    if (!cond_is_ours(cond)) {
        return -1;
    }
    return pthread_cond_signal(&cond->cond) == 0 ? 0 : -1;
}

static S3E_SOFTFP int32_t s3e_thread_cond_broadcast(void *handle) {
    struct s3e_cond *cond = handle;
    thread_trace("CondBroadcast", (uint32_t)(uintptr_t)handle, 0);
    if (!cond_is_ours(cond)) {
        return -1;
    }
    return pthread_cond_broadcast(&cond->cond) == 0 ? 0 : -1;
}

static S3E_SOFTFP int32_t s3e_thread_cond_wait(void *cond_handle, void *lock_handle,
                                               int32_t timeout_ms) {
    struct s3e_cond *cond = cond_handle;
    struct s3e_lock *lock = lock_handle;
    thread_trace("CondWait", (uint32_t)(uintptr_t)cond_handle, (uint32_t)timeout_ms);
    if (!cond_is_ours(cond) || !lock_is_ours(lock)) {
        return -1;
    }
    /* Processo de UMA thread so: esperar para sempre aqui seria travar o jogo
       sem ninguem para sinalizar. Espera com teto curto e devolve timeout — e'
       o mesmo efeito de um wait espurio, que todo uso de cond tem de tolerar. */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long ms = (timeout_ms > 0 && timeout_ms < 50) ? timeout_ms : 50;
    ts.tv_nsec += ms * 1000000L;
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec %= 1000000000L;
    int rc = pthread_cond_timedwait(&cond->cond, &lock->mutex, &ts);
    return (rc == 0 || rc == ETIMEDOUT) ? 0 : -1;
}

static S3E_SOFTFP int32_t s3e_thread_yield(void) {
    sched_yield();
    return 0;
}

static S3E_SOFTFP int32_t s3e_thread_ok(uint32_t a, uint32_t b) {
    (void)a;
    (void)b;
    return 0;
}

static S3E_SOFTFP int32_t s3e_thread_unsupported(uint32_t a, uint32_t b) {
    /* Se o jogo PEDIR uma thread (criar/juntar/soltar) e nos recusarmos, uma
       fase inteira pode nunca acontecer — por exemplo carregar recursos num
       trabalhador. Por isso a recusa nunca e' silenciosa. */
    fprintf(stderr, "[thread] *** O JOGO PEDIU criar/juntar/soltar THREAD "
                    "(a=0x%08x b=0x%08x) e recebeu 'nao disponivel' ***\n",
            a, b);
    fflush(stderr);
    (void)a;
    (void)b;
    /* Slots que este build NUNCA chama (criar/juntar thread, medidos como nao
       usados). Responder "nao disponivel" e' o correto: sucesso falso trava o
       jogo esperando um callback que nunca vem. */
    return -1;
}

static S3E_SOFTFP int32_t s3e_thread_ok03(uint32_t a, uint32_t b) {
    g_thread_slot_calls[3]++;
    thread_trace("slot03", a, b);
    return 0;
}
static S3E_SOFTFP int32_t s3e_thread_ok05(uint32_t a, uint32_t b) {
    g_thread_slot_calls[5]++;
    thread_trace("slot05", a, b);
    return 0;
}
static S3E_SOFTFP int32_t s3e_thread_ok06(uint32_t a, uint32_t b) {
    g_thread_slot_calls[6]++;
    thread_trace("slot06", a, b);
    return 0;
}
static S3E_SOFTFP int32_t s3e_thread_ok07(uint32_t a, uint32_t b) {
    g_thread_slot_calls[7]++;
    thread_trace("slot07", a, b);
    return 0;
}
static S3E_SOFTFP int32_t s3e_thread_ok08(uint32_t a, uint32_t b) {
    g_thread_slot_calls[8]++;
    thread_trace("slot08", a, b);
    return 0;
}
static S3E_SOFTFP int32_t s3e_thread_ok10(uint32_t a, uint32_t b) {
    g_thread_slot_calls[10]++;
    thread_trace("slot10", a, b);
    return 0;
}
static S3E_SOFTFP int32_t s3e_thread_ok14(uint32_t a, uint32_t b) {
    g_thread_slot_calls[14]++;
    thread_trace("slot14", a, b);
    return 0;
}
static S3E_SOFTFP int32_t s3e_thread_ok15(uint32_t a, uint32_t b) {
    g_thread_slot_calls[15]++;
    thread_trace("slot15", a, b);
    return 0;
}
static S3E_SOFTFP int32_t s3e_thread_ok16(uint32_t a, uint32_t b) {
    g_thread_slot_calls[16]++;
    thread_trace("slot16", a, b);
    return 0;
}
static S3E_SOFTFP int32_t s3e_thread_ok17(uint32_t a, uint32_t b) {
    g_thread_slot_calls[17]++;
    thread_trace("slot17", a, b);
    return 0;
}
static S3E_SOFTFP int32_t s3e_thread_ok18(uint32_t a, uint32_t b) {
    g_thread_slot_calls[18]++;
    thread_trace("slot18", a, b);
    return 0;
}
static S3E_SOFTFP int32_t s3e_thread_ok19(uint32_t a, uint32_t b) {
    g_thread_slot_calls[19]++;
    thread_trace("slot19", a, b);
    return 0;
}

static void *g_s3e_thread_table[S3E_THREAD_SLOTS] = {
    (void *)(uintptr_t)&s3e_thread_unsupported,  /*  0 create  (nao usado)   */
    (void *)(uintptr_t)&s3e_thread_unsupported,  /*  1 join    (nao usado)   */
    (void *)(uintptr_t)&s3e_thread_unsupported,  /*  2 detach  (nao usado)   */
    (void *)(uintptr_t)&s3e_thread_ok03,         /*  3                        */
    (void *)(uintptr_t)&s3e_thread_get_current,  /*  4 GetCurrent   (medido) */
    (void *)(uintptr_t)&s3e_thread_ok05,         /*  5                        */
    (void *)(uintptr_t)&s3e_thread_ok06,         /*  6                        */
    (void *)(uintptr_t)&s3e_thread_ok07,         /*  7                        */
    (void *)(uintptr_t)&s3e_thread_ok08,         /*  8                        */
    (void *)(uintptr_t)&s3e_thread_lock_create,  /*  9 LockCreate   (medido) */
    (void *)(uintptr_t)&s3e_thread_ok10,         /* 10                        */
    (void *)(uintptr_t)&s3e_thread_lock_destroy, /* 11 LockDestroy            */
    (void *)(uintptr_t)&s3e_thread_lock_acquire, /* 12 LockAcquire  (medido) */
    (void *)(uintptr_t)&s3e_thread_lock_release, /* 13 LockRelease  (medido) */
    (void *)(uintptr_t)&s3e_thread_cond_create,  /* 14 CondCreate  (medido)  */
    (void *)(uintptr_t)&s3e_thread_ok15,         /* 15                        */
    (void *)(uintptr_t)&s3e_thread_ok16,         /* 16                        */
    (void *)(uintptr_t)&s3e_thread_ok17,         /* 17                        */
    (void *)(uintptr_t)&s3e_thread_ok18,         /* 18                        */
    (void *)(uintptr_t)&s3e_thread_ok19,         /* 19                        */
};

int32_t s3eExtGetHash(uint32_t hash, void *iface, uint32_t size) {
    /* SIMS3_EXT_LOG=1 registra hash+tamanho de cada pedido de extensao: da para
       saber quantos slots a tabela precisa ter sem adivinhar. */
    if (getenv("SIMS3_EXT_LOG")) {
        fprintf(stderr, "[ext] pedido hash=0x%08x size=%u (%u slots)\n", hash, size,
                size / (unsigned)sizeof(void *));
        fflush(stderr);
    }
    if (hash == S3E_THREAD_HASH) {
        if (iface && size == sizeof(g_s3e_thread_table)) {
            memcpy(iface, g_s3e_thread_table, size);
            fprintf(stderr, "[thread] extensao s3eThread entregue (%u bytes, %d slots)\n", size,
                    S3E_THREAD_SLOTS);
            return 0;
        }
        fprintf(stderr, "[thread] s3eThread pedida com size=%u (esperado %u) — recusada\n", size,
                (unsigned)sizeof(g_s3e_thread_table));
        return 1;
    }

    if (hash == IS_DEVICE_HASH) {
        void *device_table[5] = {
            (void *)(uintptr_t)&isDeviceCallbackRegister,
            (void *)(uintptr_t)&isDeviceCallbackUnregister,
            (void *)(uintptr_t)&isDeviceSetTabletThreshold,
            (void *)(uintptr_t)&isDeviceGetDisplayType,
            (void *)(uintptr_t)&isDeviceGetExternalResources,
        };
        if (iface && size == sizeof(device_table)) {
            memcpy(iface, device_table, size);
            return 0;
        }
        return 1;
    }
    if (hash == S3E_TOUCHPAD_HASH) {
        void *touchpad_table[5] = {
            (void *)(uintptr_t)&s3eTouchpadRegister, (void *)(uintptr_t)&s3eTouchpadUnRegister,
            (void *)(uintptr_t)&s3eReturn0,          (void *)(uintptr_t)&s3eReturn0,
            (void *)(uintptr_t)&s3eTouchpadGetInt,
        };
        if (iface && size == sizeof(touchpad_table)) {
            memcpy(iface, touchpad_table, size);
            return 0;
        }
        return 1;
    }
    if (iface && size > 0) {
        memset(iface, 0, size);
    }
    return 1;
}

/* The Sims 3 (NextOS): o trampolim de stub devolvia 0 EM SILENCIO. Um simbolo
   gl/egl essencial stubado e' invisivel assim — e devolver 0 para algo como
   glGenVertexArrays ou glCheckFramebufferStatus faz o jogo seguir com um
   recurso invalido e morrer longe daqui. Agora cada stub avisa na PRIMEIRA
   chamada (nao no resolve: resolver 227 simbolos gl nao quer dizer usar). */
uintptr_t s3e_trampoline_dispatch(uint32_t index) {
    static uint8_t called[512];
    if (index < sizeof(called) && !called[index]) {
        called[index] = 1;
        fprintf(stderr, "[stub] *** O JOGO CHAMOU o stub \"%s\" (devolvendo 0) ***\n",
                index < g_stub_count && g_stub_names[index] ? g_stub_names[index] : "?");
        fflush(stderr);
    }
    return 0;
}

void *make_stub(const char *symbol) {
    enum { STUB_SIZE = 20 };
    if (!g_stub_code) {
        g_stub_code_size = 16384;
        g_stub_code = mmap(NULL, g_stub_code_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (g_stub_code == MAP_FAILED) {
            g_stub_code = NULL;
            return (void *)(uintptr_t)&s3eStub;
        }
    }
    if (g_stub_count >= sizeof(g_stub_names) / sizeof(g_stub_names[0]) ||
        (g_stub_count + 1) * STUB_SIZE > g_stub_code_size) {
        return (void *)(uintptr_t)&s3eStub;
    }
    size_t index = g_stub_count++;
    g_stub_names[index] = strdup(symbol ? symbol : "unknown");
    /* NFS Shift (NextOS): SIMS3_GL_LOG=1 lista cada simbolo que virou STUB
       (no-op). Um simbolo gl/egl essencial stubado explica textura branca. */
    if (getenv("SIMS3_GL_LOG")) {
        fprintf(stderr, "[stub] %s\n", symbol ? symbol : "unknown");
    }
    uint32_t *code = (uint32_t *)(void *)(g_stub_code + index * STUB_SIZE);
    code[0] = 0xe59f0004u;
    code[1] = 0xe59ff004u;
    code[2] = 0xe1a00000u;
    code[3] = (uint32_t)index;
    code[4] = (uint32_t)(uintptr_t)&s3e_trampoline_dispatch;
    __builtin___clear_cache((char *)code, (char *)(code + 5));
    return code;
}

int32_t s3eRegisterNoop(uint32_t id, void *callback, void *user_data) {
    (void)id;
    (void)callback;
    (void)user_data;
    return 0;
}

int32_t s3eDeviceRegister(uint32_t id, void *callback, void *user_data) {
    return s3eRegisterNoop(id, callback, user_data);
}

int32_t s3eDeviceUnRegister(uint32_t id, void *callback) {
    return s3eRegisterNoop(id, callback, NULL);
}
