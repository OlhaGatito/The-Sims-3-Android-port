#include "s3e_host_internal.h"

/* The Sims 3 (NextOS): o jogo pede o proprio container com separador do
   WINDOWS — medido no log: open("LowRes\\res.dz","rb") -> MISS. A camada de
   arquivo herdada so conhecia "/", entao o jogo NUNCA conseguia abrir o
   res.dz para montar o filesystem dele; o mapa de recursos ficava VAZIO e a
   saida da tela de idioma morria num balde de hash nulo. Normalizamos "\" em
   "/" na entrada de TODA a camada de arquivo — a Marmalade aceita os dois. */
static const char *vfs_normalize(const char *name, char *buf, size_t buf_size) {
    if (!name) {
        return "";
    }
    if (!strchr(name, '\\')) {
        return name;
    }
    size_t i = 0;
    for (; name[i] && i + 1 < buf_size; ++i) {
        buf[i] = (name[i] == '\\') ? '/' : name[i];
    }
    buf[i] = 0;
    return buf;
}

unsigned long g_fileop_read, g_fileop_getchar;
/* Observacao ARMADA por evento, nao por ponteiro: o FILE* e' reciclado e
   comparar ponteiro faz a observacao migrar de arquivo (ja atribuiu bytes de
   .m3g a tabela de strings). Depois do open casado, registramos as proximas N
   operacoes de leitura, que sao as do parser. */
static int g_watch_armed;
static void *g_watch_file;
static unsigned long g_watch_read;
static void s3e_file_watch(void *file);

static void make_path(char *out, size_t out_size, const char *name) {
    if (name && name[0] == '/') {
        snprintf(out, out_size, "%s", name);
    } else {
        snprintf(out, out_size, "%s/%s", g_root, name ? name : "");
    }
}

static int path_exists(const char *path) {
    return access(path, F_OK) == 0;
}

static int use_existing_path(char *out, size_t out_size, const char *path) {
    if (!path_exists(path)) {
        return 0;
    }
    snprintf(out, out_size, "%s", path);
    return 1;
}

static int try_asset_path(char *out, size_t out_size, const char *prefix, const char *name) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s/%s", g_root, prefix, name);
    return use_existing_path(out, out_size, path);
}

static const char *base_name(const char *name) {
    const char *slash = strrchr(name ? name : "", '/');
    return slash ? slash + 1 : (name ? name : "");
}

static int flat_group_name(const char *name, char *out, size_t out_size) {
    const char *slash = strchr(name ? name : "", '/');
    if (!slash || !strstr(name, ".group.bin")) {
        return 0;
    }
    size_t section_len = (size_t)(slash - name);
    if (section_len == 0 || section_len >= 128) {
        return 0;
    }
    char section[128];
    memcpy(section, name, section_len);
    section[section_len] = 0;
    const char *leaf = strrchr(name, '/') + 1;
    if (strcmp(leaf, ".group.bin") == 0) {
        snprintf(out, out_size, "%s.group.bin", section);
    } else {
        snprintf(out, out_size, "%s", leaf);
    }
    return 1;
}

static uint16_t read_le16(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_le32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void lower_copy(char *out, size_t out_size, const char *in) {
    size_t i = 0;
    if (!out_size) {
        return;
    }
    for (; in && in[i] && i + 1 < out_size; ++i) {
        out[i] = (char)tolower((unsigned char)in[i]);
    }
    out[i] = 0;
}

static int read_c_string(FILE *file, char *out, size_t out_size) {
    size_t len = 0;
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (ch == 0) {
            if (out_size) {
                out[len < out_size ? len : out_size - 1] = 0;
            }
            return 1;
        }
        if (len + 1 < out_size) {
            out[len++] = (char)ch;
        }
    }
    if (out_size) {
        out[len < out_size ? len : out_size - 1] = 0;
    }
    return 0;
}

static int dtrz_load_index(void) {
    if (g_dtrz.loaded) {
        return g_dtrz.count > 0;
    }
    g_dtrz.loaded = 1;
    static const char *archives[] = {
        "blackops_etc.dz",
        "blackops_atitc.dz",
        "blackops_dxt.dz",
        "blackops_gles1.dz",
    };
    for (size_t i = 0; i < sizeof(archives) / sizeof(archives[0]); ++i) {
        char candidate[sizeof(g_dtrz.path)];
        snprintf(candidate, sizeof(candidate), "%s/assets/%s", g_root, archives[i]);
        if (path_exists(candidate)) {
            snprintf(g_dtrz.path, sizeof(g_dtrz.path), "%s", candidate);
            break;
        }
    }
    if (!g_dtrz.path[0]) {
        snprintf(g_dtrz.path, sizeof(g_dtrz.path), "%s/assets/blackops_gles1.dz", g_root);
    }

    FILE *file = fopen(g_dtrz.path, "rb");
    if (!file) {
        return 0;
    }
    uint8_t header[9];
    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "DTRZ", 4) != 0) {
        fclose(file);
        return 0;
    }
    uint16_t file_count = read_le16(header + 4);
    uint16_t group_count = read_le16(header + 6);
    if (group_count > 0) {
        group_count--;
    }
    if (file_count > DTRZ_MAX_ENTRIES) {
        fclose(file);
        return 0;
    }
    for (uint16_t i = 0; i < file_count; ++i) {
        if (!read_c_string(file, g_dtrz.entries[i].name, sizeof(g_dtrz.entries[i].name))) {
            fclose(file);
            return 0;
        }
    }
    char scratch[DTRZ_NAME_MAX];
    for (uint16_t i = 0; i < group_count; ++i) {
        if (!read_c_string(file, scratch, sizeof(scratch))) {
            fclose(file);
            return 0;
        }
    }
    uint8_t marker[4];
    if (fread(marker, 1, sizeof(marker), file) != sizeof(marker) || read_le32(marker) != 1 ||
        fseek(file, (long)file_count * 6L, SEEK_CUR) != 0) {
        fclose(file);
        return 0;
    }
    for (uint16_t i = 0; i < file_count; ++i) {
        uint8_t record[16];
        if (fread(record, 1, sizeof(record), file) != sizeof(record)) {
            fclose(file);
            return 0;
        }
        struct dtrz_entry *entry = &g_dtrz.entries[i];
        entry->offset = read_le32(record);
        entry->size = read_le32(record + 4);
        lower_copy(entry->lower_name, sizeof(entry->lower_name), entry->name);
        lower_copy(entry->lower_base, sizeof(entry->lower_base), base_name(entry->name));
    }
    g_dtrz.count = file_count;
    fclose(file);
    return 1;
}

static int dtrz_archive_redirect_path(const char *name, char *out, size_t out_size) {
    const char *requested = base_name(name ? name : "");
    if (strcasecmp(requested, "blackops_gles1.dz") != 0) {
        return 0;
    }
    if (!dtrz_load_index() || !g_dtrz.path[0]) {
        return 0;
    }
    const char *selected = base_name(g_dtrz.path);
    if (strcasecmp(selected, requested) == 0 || !path_exists(g_dtrz.path)) {
        return 0;
    }
    snprintf(out, out_size, "%s", g_dtrz.path);
    return 1;
}

static const struct dtrz_entry *dtrz_find_entry(const char *name) {
    if (!dtrz_load_index()) {
        return NULL;
    }
    char lower_name[DTRZ_NAME_MAX];
    lower_copy(lower_name, sizeof(lower_name), name ? name : "");
    const char *lower_base = base_name(lower_name);
    for (size_t i = 0; i < g_dtrz.count; ++i) {
        const struct dtrz_entry *entry = &g_dtrz.entries[i];
        if (strcmp(entry->lower_name, lower_name) == 0 ||
            strcmp(entry->lower_base, lower_base) == 0) {
            return entry;
        }
    }
    return NULL;
}

static int dtrz_entry_exists(const char *name) {
    return dtrz_find_entry(name) != NULL;
}

static int dtrz_prefer_entry(const char *name) {
    char lower[256];
    lower_copy(lower, sizeof(lower), name ? name : "");
    return strncmp(lower, "data-etc/", 9) == 0 || strncmp(lower, "data-dxt/", 9) == 0 ||
           strncmp(lower, "data-atitc/", 11) == 0;
}

/* NFS Shift (NextOS): exposto p/ o leitor derbh registrar o buffer descomprimido
   junto ao FILE de memoria, de modo que s3eFileClose libere ambos. */
void derbh_track_memory_file(void *file, void *buffer);

static void track_memory_file(FILE *file, void *buffer) {
    struct memory_file *item = malloc(sizeof(*item));
    if (!item) {
        fclose(file);
        free(buffer);
        return;
    }
    item->file = file;
    item->buffer = buffer;
    item->next = g_memory_files;
    g_memory_files = item;
}

void derbh_track_memory_file(void *file, void *buffer) {
    track_memory_file((FILE *)file, buffer);
}

static int close_memory_file(FILE *file) {
    struct memory_file **link = &g_memory_files;
    while (*link) {
        struct memory_file *item = *link;
        if (item->file == file) {
            *link = item->next;
            fclose(item->file);
            free(item->buffer);
            free(item);
            return 1;
        }
        link = &item->next;
    }
    return 0;
}

static FILE *open_dtrz_entry(const char *name, char *opened_path, size_t opened_path_size) {
    const struct dtrz_entry *entry = dtrz_find_entry(name);
    if (!entry) {
        return NULL;
    }
    FILE *archive = fopen(g_dtrz.path, "rb");
    if (!archive) {
        return NULL;
    }
    size_t alloc_size = entry->size ? entry->size : 1;
    void *buffer = malloc(alloc_size);
    if (!buffer) {
        fclose(archive);
        return NULL;
    }
    if (fseek(archive, (long)entry->offset, SEEK_SET) != 0 ||
        fread(buffer, 1, entry->size, archive) != entry->size) {
        fclose(archive);
        free(buffer);
        return NULL;
    }
    fclose(archive);
    FILE *file = fmemopen(buffer, alloc_size, "rb");
    if (!file) {
        free(buffer);
        return NULL;
    }
    track_memory_file(file, buffer);
    snprintf(opened_path, opened_path_size, "DTRZ:%s", entry->name);
    return file;
}

static int resolve_read_path(const char *name, char *out, size_t out_size) {
    char path[1200];
    const char *safe_name = name ? name : "";
    if (dtrz_archive_redirect_path(safe_name, out, out_size)) {
        return 1;
    }
    make_path(path, sizeof(path), safe_name);
    if (use_existing_path(out, out_size, path)) {
        return 1;
    }
    if (safe_name[0] == '/') {
        /* The Sims 3 (NextOS): a barra inicial e' a raiz DO APP no VFS da
           Marmalade (mesma regra da make_user_path). Sem isto o jogo GRAVAVA o
           "/sim3set" em HOME mas o s3eFileCheckExists("/sim3set") continuava
           dando 0 — ele salvava a configuracao e nao conseguia reler, ficando
           preso na primeira execucao para sempre. Procuramos primeiro em HOME
           (dado gravavel do jogador) e depois na raiz do port. */
        const char *rel = safe_name;
        while (*rel == '/') {
            rel++;
        }
        if (!*rel) {
            return 0;
        }
        const char *home = getenv("HOME");
        if (home && home[0]) {
            snprintf(path, sizeof(path), "%s/%s", home, rel);
            if (use_existing_path(out, out_size, path)) {
                return 1;
            }
        }
        snprintf(path, sizeof(path), "%s/%s", g_root, rel);
        if (use_existing_path(out, out_size, path)) {
            return 1;
        }
        snprintf(path, sizeof(path), "%s/assets/%s", g_root, rel);
        if (use_existing_path(out, out_size, path)) {
            return 1;
        }
        return 0;
    }
    if (try_asset_path(out, out_size, "assets", safe_name) ||
        try_asset_path(out, out_size, "assets/data-gles1", safe_name) ||
        try_asset_path(out, out_size, "assets/data-sw", safe_name)) {
        return 1;
    }
    char flat_name[512];
    if (flat_group_name(safe_name, flat_name, sizeof(flat_name)) &&
        (try_asset_path(out, out_size, "assets/data-gles1", flat_name) ||
         try_asset_path(out, out_size, "assets/data-sw", flat_name) ||
         try_asset_path(out, out_size, "assets", flat_name))) {
        return 1;
    }
    const char *leaf = base_name(safe_name);
    if (leaf != safe_name && (try_asset_path(out, out_size, "assets/data-gles1", leaf) ||
                              try_asset_path(out, out_size, "assets/data-sw", leaf) ||
                              try_asset_path(out, out_size, "assets", leaf))) {
        return 1;
    }
    return 0;
}

static int is_read_mode(const char *mode) {
    return mode && mode[0] == 'r';
}

static int is_archive_read_mode(const char *mode) {
    return is_read_mode(mode) && strchr(mode, '+') == NULL;
}

static int is_user_file_mode(const char *mode) {
    return mode && (strchr(mode, 'U') || strchr(mode, '+') || mode[0] == 'w' || mode[0] == 'a');
}

static int is_user_file_name(const char *name) {
    const char *dot = strrchr(name ? name : "", '.');
    return dot && strcasecmp(dot, ".i3d") == 0;
}

static void sanitize_file_mode(const char *mode, char *out, size_t out_size) {
    size_t n = 0;
    if (!out_size) {
        return;
    }
    for (const char *p = mode ? mode : "rb"; *p && n + 1 < out_size; ++p) {
        if (*p != 'U') {
            out[n++] = *p;
        }
    }
    out[n] = 0;
    if (!out[0]) {
        snprintf(out, out_size, "rb");
    }
}

/* The Sims 3 (NextOS): no VFS da Marmalade a barra inicial e' a raiz DO APP,
   nao a raiz do sistema de arquivos. O loader base mandava "/sim3set" direto
   para "/sim3set" no rootfs — que no NextOS e' SOMENTE LEITURA — e a gravacao
   falhava calada ("MISS"). Sem conseguir salvar o sim3set (arquivo de
   configuracao/estado), o jogo NAO SAI da primeira execucao: a tela de idioma
   aceitava arrasto (rolagem) mas nenhum toque tinha efeito, porque toda escolha
   depende de persistir. Aqui a barra inicial e' descartada e o caminho sai
   sempre sob HOME (dado do jogo, gravavel). */
static void make_user_path(char *out, size_t out_size, const char *name) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        home = g_root;
    }
    const char *rel = name ? name : "";
    while (*rel == '/') {
        rel++;
    }
    snprintf(out, out_size, "%s/%s", home, rel);
}

static void make_parent_dirs(const char *path) {
    char tmp[1200];
    snprintf(tmp, sizeof(tmp), "%s", path ? path : "");
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        return 0;
    }
    make_parent_dirs(dst);
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return 0;
    }
    uint8_t buffer[8192];
    size_t got;
    int ok = 1;
    while ((got = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, got, out) != got) {
            ok = 0;
            break;
        }
    }
    if (ferror(in)) {
        ok = 0;
    }
    fclose(out);
    fclose(in);
    return ok;
}

static long file_size_for_seek(FILE *file) {
    long here = ftell(file);
    if (here < 0 || fseek(file, 0, SEEK_END) != 0) {
        return -1;
    }
    long size = ftell(file);
    fseek(file, here, SEEK_SET);
    return size;
}

static long g_last_open_size = 0;

/* Ancora de tempo para o roteiro de toque. O relogio de parede engana: cada
   tela demora um tanto diferente a cada corrida, e toque em instante fixo ora
   acerta ora erra. Aqui marcamos QUANDO um arquivo-marco foi aberto (por
   padrao scene_cas.m3g, que so abre quando o Create-A-Sim comeca a carregar),
   e o roteiro pendura os toques nesse evento em vez do relogio. */
static uint64_t g_anchor_ms;

uint64_t s3e_file_anchor_ms(void) { return g_anchor_ms; }

static void anchor_note(const char *name) {
    if (g_anchor_ms) return;
    const char *want = getenv("SIMS3_ANCHOR");
    if (!want || !want[0] || !name || !strstr(name, want)) return;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    g_anchor_ms = (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
    fprintf(stderr, "[ancora] \"%s\" abriu — roteiro pendurado neste instante\n", name);
    fflush(stderr);
}
static char g_last_open_name[512];

void *s3eFileOpen(const char *name, const char *mode) {
    /* SIMS3_OPEN_FROM="anim3d" -> mostra de QUE ponto do modulo veio o open.
       E' assim que se acha o carregador de uma tabela sem procurar na
       disassembly inteira: o endereco de retorno cai no sitio de chamada. */
    {
        const char *want = getenv("SIMS3_OPEN_FROM");
        if (want && want[0] && name && strstr(name, want)) {
            void *ra = __builtin_return_address(0);
            uintptr_t off = (g_module_base && (uintptr_t)ra >= g_module_base &&
                             (uintptr_t)ra < g_module_base + g_module_size)
                                ? (uintptr_t)ra - g_module_base
                                : 0;
            fprintf(stderr, "[from] open(\"%s\") chamado de modulo+0x%lx\n", name,
                    (unsigned long)off);
            fflush(stderr);
            /* o endereco de retorno cai no embrulho de arquivo do CRT; quem
               interessa e' o CHAMADOR de verdade, entao varremos a pilha. */
            s3e_dump_module_stack_from("open", (uintptr_t)__builtin_frame_address(0));
        }
    }
    char path[1200];
    char opened_path[1200] = "";
    char norm[1200];
    const char *safe_name = vfs_normalize(name, norm, sizeof(norm));
    const char *safe_mode = mode ? mode : "rb";
    char fopen_mode[16];
    sanitize_file_mode(safe_mode, fopen_mode, sizeof(fopen_mode));
    FILE *file = NULL;

    if (is_user_file_mode(safe_mode) || is_user_file_name(safe_name)) {
        char seed_path[1200];
        make_user_path(path, sizeof(path), safe_name);
        make_parent_dirs(path);
        if (!is_user_file_name(safe_name) && safe_mode[0] == 'r' && !path_exists(path) &&
            resolve_read_path(safe_name, seed_path, sizeof(seed_path))) {
            copy_file(seed_path, path);
        }
        file = fopen(path, fopen_mode);
        if (file) {
            snprintf(opened_path, sizeof(opened_path), "%s", path);
        }
    } else if (is_archive_read_mode(safe_mode) && dtrz_prefer_entry(safe_name) &&
               (file = open_dtrz_entry(safe_name, opened_path, sizeof(opened_path))) != NULL) {
    } else if (is_read_mode(safe_mode) && resolve_read_path(safe_name, path, sizeof(path))) {
        file = fopen(path, fopen_mode);
    } else {
        make_path(path, sizeof(path), safe_name);
        if (!is_read_mode(safe_mode)) {
            make_parent_dirs(path);
        }
        file = fopen(path, fopen_mode);
    }
    if (file && !opened_path[0]) {
        snprintf(opened_path, sizeof(opened_path), "%s", path);
    }
    if (!file && is_read_mode(safe_mode) && safe_name[0] != '/') {
        snprintf(path, sizeof(path), "%s/assets/%s", g_root, safe_name);
        file = fopen(path, fopen_mode);
    }
    if (!file && is_archive_read_mode(safe_mode)) {
        file = open_dtrz_entry(safe_name, opened_path, sizeof(opened_path));
    }
    /* NFS Shift (NextOS): membro de .dz (derbh) — descomprime e serve. Fallback
       so p/ leitura, DEPOIS de checar arquivo solto (override) e o dtrz base. */
    if (!file && is_read_mode(safe_mode)) {
        file = (FILE *)derbh_open(safe_name);
        if (file) {
            snprintf(opened_path, sizeof(opened_path), "DERBH:%s", base_name(safe_name));
        }
    }
    if (!file && is_read_mode(safe_mode) && strcmp(base_name(safe_name), "console.bin") == 0) {
        file = fopen("/dev/null", "rb");
    }
    {
        const char *want = getenv("SIMS3_OPEN_FROM");
        if (want && want[0] && file && strstr(safe_name, want)) {
            s3e_file_watch(file);
            fprintf(stderr, "[watch] ARQUIVO OBSERVADO: \"%s\" -> FILE*=%p\n", safe_name, file);
            fflush(stderr);
        }
    }
    /* NFS Shift (NextOS): auditoria opt-in — SIMS3_FILE_LOG=1 mostra cada
       s3eFileOpen (nome pedido, modo, path resolvido, hit/miss). Essencial p/
       mapear onde o modulo procura common.dz/gfx.dz e os recursos do OBB. */
    /* A trinca s3eFileGetFileInt(key=1/4/5) vem SEMPRE logo depois do open
       (medido no log), e o handle que ela recebe NAO e' o nosso FILE* — e' o
       embrulho do jogo. Entao guardamos aqui o tamanho do ultimo arquivo
       aberto, unica forma honesta de responder aquela consulta. */
    if (file) {
        anchor_note(safe_name);
        g_last_open_size = file_size_for_seek(file);
        snprintf(g_last_open_name, sizeof(g_last_open_name), "%s", safe_name);
    }
    /* 🚨 MISS de arquivo e' MUDO: o jogo pede, nao recebe, e simplesmente deixa
       de montar aquela parte da tela — sem erro de GL, sem assert, sem nada.
       Ate hoje o MISS so' aparecia sob SIMS3_FILE_LOG, entao "nenhum arquivo
       faltando" numa corrida normal nao provava nada. Passa a valer sempre,
       com teto e sem repetir o mesmo nome. */
    if (!file) {
        static char vistos[64][128];
        static unsigned n_vistos = 0;
        int repetido = 0;
        for (unsigned i = 0; i < n_vistos; ++i) {
            if (strcmp(vistos[i], safe_name) == 0) {
                repetido = 1;
                break;
            }
        }
        if (!repetido) {
            if (n_vistos < 64) {
                snprintf(vistos[n_vistos], sizeof(vistos[0]), "%s", safe_name);
                n_vistos++;
            }
            fprintf(stderr, "[file] NAO ACHOU: \"%s\" (modo %s)\n", safe_name, safe_mode);
            fflush(stderr);
        }
    }
    if (getenv("SIMS3_FILE_LOG")) {
        /* carimbo de tempo: e' o que permite descobrir QUAL arquivo marca a
           entrada de cada tela, para pendurar o roteiro de toque nele em vez
           de no relogio de parede. */
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        fprintf(stderr, "[t=%lu] ", (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000));
        fprintf(stderr, "[file] open(\"%s\", \"%s\") -> %s%s\n", safe_name, safe_mode,
                file ? (opened_path[0] ? opened_path : "(ok)") : "MISS",
                file ? "" : "");
    }
    return file;
}

int32_t s3eFileClose(void *file) {
    /* O ponteiro de FILE e' RECICLADO pela libc: sem limpar aqui, a observacao
       migra para outro arquivo e passa a mentir (foi assim que apareceu um
       "total lido" de 3,5 MB num arquivo de 39 KB, e bytes de .m3g atribuidos
       a tabela de strings). */
    if (file && file == g_watch_file) {
        fprintf(stderr, "[watch] FECHADO FILE*=%p apos %lu leituras\n", file, g_watch_read);
        fflush(stderr);
        g_watch_file = NULL;
        g_watch_read = 0;
    }
    if (!file) {
        return -1;
    }
    if (close_memory_file((FILE *)file)) {
        return 0;
    }
    return fclose((FILE *)file);
}

/* Instrumento: para o arquivo casado por SIMS3_OPEN_FROM, acompanha tamanho e
   TOTAL LIDO. Se o jogo abre a tabela de strings mas le 0 bytes (ou um tamanho
   errado), o parse produz zero strings sem nenhum erro aparecer. */

static void s3e_file_watch(void *file) {
    g_watch_file = file;
    g_watch_read = 0;
}

uint32_t s3eFileRead(void *buffer, uint32_t elem_size, uint32_t count, void *file) {
    g_fileop_read++;
    int armed = (file && file == g_watch_file && g_watch_read < 3);
    uint32_t n = file ? (uint32_t)fread(buffer, elem_size, count, (FILE *)file) : 0;
    if (armed) {
        g_watch_read++;
        fprintf(stderr, "[watch] Read(%p, %u x %u) = %u  bytes:", file, elem_size, count, n);
        const unsigned char *b = (const unsigned char *)buffer;
        unsigned lim = n * elem_size;
        if (lim > 16) {
            lim = 16;
        }
        for (unsigned q = 0; q < lim; ++q) {
            fprintf(stderr, " %02x", b[q]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    if (file && file == g_watch_file) {
        int first = (g_watch_read == 0);
        g_watch_read += (unsigned long)n * elem_size;
        fprintf(stderr, "[watch] read(%u x %u) = %u  (total %lu)", elem_size, count, n,
                g_watch_read);
        if (first && n > 0 && buffer) {
            const unsigned char *b = (const unsigned char *)buffer;
            unsigned lim = n * elem_size;
            if (lim > 12) {
                lim = 12;
            }
            fprintf(stderr, "  primeiros bytes:");
            for (unsigned q = 0; q < lim; ++q) {
                fprintf(stderr, " %02x", b[q]);
            }
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    return n;
}

uint32_t s3eFileWrite(const void *buffer, uint32_t elem_size, uint32_t count, void *file) {
    return file ? (uint32_t)fwrite(buffer, elem_size, count, (FILE *)file) : 0;
}

int32_t s3eFileGetChar(void *file) {
    g_fileop_getchar++;

    int c = file ? fgetc((FILE *)file) : -1;
    if (file && file == g_watch_file) {
        g_watch_read++;
    }
    return c;
}

int32_t s3eFilePutChar(int32_t c, void *file) {
    return file ? fputc(c, (FILE *)file) : -1;
}

int32_t s3eFileFlush(void *file) {
    return file ? fflush((FILE *)file) : -1;
}

int32_t s3eFileSeek(void *file, int32_t offset, int32_t origin) {
    return file ? fseek((FILE *)file, offset, origin) : -1;
}

int32_t s3eFileTell(void *file) {
    return file ? (int32_t)ftell((FILE *)file) : -1;
}

int32_t s3eFileGetSize(void *file) {
    int32_t sz = file ? (int32_t)file_size_for_seek((FILE *)file) : -1;
    if (file && file == g_watch_file) {
        fprintf(stderr, "[watch] GetSize = %d\n", sz);
        fflush(stderr);
    }
    return sz;
}

/* NFS Shift (NextOS): import do modulo que o loader base nao exportava. */
int32_t s3eFileEOF(void *file) {
    return file && feof((FILE *)file) ? 1 : 0;
}

int32_t s3eFileCheckExists(const char *name) {
    char path[1200];
    char norm[1200];
    const char *safe_name = vfs_normalize(name, norm, sizeof(norm));
    int found = 0;
    if (is_user_file_name(safe_name)) {
        make_user_path(path, sizeof(path), safe_name);
        found = access(path, F_OK) == 0 ? 1 : 0;
    } else if (dtrz_prefer_entry(safe_name) && dtrz_entry_exists(safe_name)) {
        found = 1;
    } else if (resolve_read_path(safe_name, path, sizeof(path))) {
        found = 1;
    } else {
        snprintf(path, sizeof(path), "%s/assets/%s", g_root, safe_name);
        found = access(path, F_OK) == 0 ? 1 : dtrz_entry_exists(safe_name) ? 1 : 0;
    }
    /* NFS Shift (NextOS): membro de .dz (derbh) conta como existente. */
    if (!found && derbh_exists(safe_name)) {
        found = 1;
    }
    if (getenv("SIMS3_FILE_LOG")) {
        fprintf(stderr, "[file] exists(\"%s\") -> %d\n", safe_name, found);
    }
    return found;
}

int32_t s3eFileGetError(void) {
    return errno;
}

const char *s3eFileGetErrorString(void) {
    return strerror(errno);
}

void *s3eFileOpenFromMemory(void *buffer, uint32_t size) {
    return buffer && size ? fmemopen(buffer, size, "rb") : NULL;
}

/* The Sims 3 (NextOS): o loader base devolvia 0 para TODA propriedade de
   arquivo. Isso e' um "arquivo vazio" para quem pergunta o tamanho — e o jogo
   abre a tabela de strings, pergunta, ve zero e FECHA SEM LER (medido: 0
   leituras em strings_generic.bin, contra 40 bytes lidos no strings_hdr.bin e
   12.241 no button_freeplay.png). Resultado: so as strings de cabecalho
   existiam. Aqui a propriedade de TAMANHO passa a responder o tamanho real. */
int64_t s3eFileGetFileInt(const char *filename, uint32_t key) {
    /* ABI Marmalade: o primeiro argumento e' o NOME, nao um s3eFile*, e o
       retorno e' int64 (r0:r1 no ARM de 32 bits). A assinatura antiga deixava
       r1 contendo a propria chave; assim toda consulta era um valor de 64 bits
       falso e nao-zero. Os call sites do Sims 3 confirmam as chaves usadas:
       1=is-directory, 4=size e 5=modified-time em milissegundos. */
    char norm[1200];
    char path[1200];
    const char *safe_name = vfs_normalize(filename, norm, sizeof(norm));
    struct stat st;
    int have_stat = resolve_read_path(safe_name, path, sizeof(path)) && stat(path, &st) == 0;

    if (getenv("SIMS3_FILE_LOG")) {
        fprintf(stderr, "[file] GetFileInt(name=\"%s\", key=%u) [ultimo=%s %ld]\n",
                safe_name, key, g_last_open_name, g_last_open_size);
        fflush(stderr);
    }

    switch (key) {
    case 1: /* S3E_FILE_IS_DIR */
        return have_stat && S_ISDIR(st.st_mode) ? 1 : 0;
    case 4: /* S3E_FILE_SIZE */
        if (strcmp(safe_name, g_last_open_name) == 0 && g_last_open_size >= 0) {
            return (int64_t)g_last_open_size;
        }
        if (have_stat && S_ISREG(st.st_mode)) {
            return (int64_t)st.st_size;
        }
        {
            const struct dtrz_entry *entry = dtrz_find_entry(safe_name);
            if (entry) {
                return (int64_t)entry->size;
            }
        }
        return -1;
    case 5: /* S3E_FILE_MODIFIED_TIME */
        if (have_stat) {
            return (int64_t)st.st_mtime * 1000 + st.st_mtim.tv_nsec / 1000000;
        }
        /* Os membros de APK/DERBH nao possuem timestamp individual. */
        return 0;
    default:
        return 0;
    }
}

int32_t s3eFileMakeDirectory(const char *name) {
    char path[1200];
    make_path(path, sizeof(path), name);
    return mkdir(path, 0777) == 0 || errno == EEXIST ? 0 : -1;
}

int32_t s3eFileDelete(const char *name) {
    char path[1200];
    make_path(path, sizeof(path), name);
    return unlink(path);
}

int32_t s3eFileRename(const char *old_name, const char *new_name) {
    char old_path[1200];
    char new_path[1200];
    make_path(old_path, sizeof(old_path), old_name);
    make_path(new_path, sizeof(new_path), new_name);
    return rename(old_path, new_path);
}

/* NFS Shift (NextOS): ABI real da s3e =
   s3eFileAddUserFileSys(s3eFileUserFileSys *fs, const char *baseUri, s3eBool prepend).
   O middleware derbh (arquivos .dz) se registra por aqui; o loader base stubava
   isso -> os membros das .dz nunca eram servidos -> grupos de recursos NULL ->
   crash na 1a textura ("EASplash"). Logamos os args p/ mapear a ABL antes de
   implementar o roteamento. */
int32_t s3eFileAddUserFileSys(void *fs, const char *base_uri, int32_t prepend) {
    (void)base_uri;
    (void)prepend;
    if (getenv("SIMS3_FILE_LOG") && fs) {
        uint32_t *w = (uint32_t *)fs;
        fprintf(stderr, "[file] AddUserFileSys fs=%p struct:\n", fs);
        for (int i = 0; i < 16; ++i) {
            uintptr_t base = 0x4a000000u;
            uint32_t v = w[i];
            if (v >= base && v < base + 0x1c3624u) {
                fprintf(stderr, "  [%2d] 0x%08x  (module+0x%06x)\n", i, v, (unsigned)(v - base));
            } else {
                fprintf(stderr, "  [%2d] 0x%08x\n", i, v);
            }
        }
    }
    return 0;
}

void *s3eFileListDirectory(const char *path) {
    (void)path;
    return NULL;
}

int32_t s3eFileListClose(void *list) {
    (void)list;
    return 0;
}
