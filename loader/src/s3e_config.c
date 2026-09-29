#include "s3e_host_internal.h"

static struct s3e_config_entry g_config_entries[S3E_CONFIG_MAX_ENTRIES];
static size_t g_config_entry_count;

static char *trim(char *text) {
    while (*text && isspace((unsigned char)*text)) {
        text++;
    }
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *--end = 0;
    }
    return text;
}

static void strip_comment(char *line) {
    int in_quote = 0;
    for (char *p = line; *p; ++p) {
        if (*p == '"') {
            in_quote = !in_quote;
        } else if (*p == '#' && !in_quote) {
            *p = 0;
            return;
        }
    }
}

static void strip_quotes(char *value) {
    size_t len = strlen(value);
    if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
        memmove(value, value + 1, len - 2);
        value[len - 2] = 0;
    }
}

static int config_find(const char *section, const char *key) {
    for (size_t i = 0; i < g_config_entry_count; ++i) {
        if (strcasecmp(g_config_entries[i].section, section) == 0 &&
            strcasecmp(g_config_entries[i].key, key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static const char *config_get(const char *section, const char *key) {
    if (!section || !key) {
        return NULL;
    }
    int index = config_find(section, key);
    return index >= 0 ? g_config_entries[index].value : NULL;
}

static void config_set(const char *section, const char *key, const char *value) {
    if (!section || !key || !section[0] || !key[0]) {
        return;
    }
    int index = config_find(section, key);
    if (index < 0) {
        if (g_config_entry_count >= S3E_CONFIG_MAX_ENTRIES) {
            return;
        }
        index = (int)g_config_entry_count++;
    }
    snprintf(g_config_entries[index].section, sizeof(g_config_entries[index].section), "%s",
             section);
    snprintf(g_config_entries[index].key, sizeof(g_config_entries[index].key), "%s", key);
    snprintf(g_config_entries[index].value, sizeof(g_config_entries[index].value), "%s",
             value ? value : "");
}

static int contains_case_insensitive(const char *text, const char *needle) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0) {
        return 1;
    }
    for (const char *p = text; *p; ++p) {
        if (strncasecmp(p, needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static int config_platform_matches(const char *value) {
    return contains_case_insensitive(value, "ANY") || contains_case_insensitive(value, "ANDROID");
}

static int config_id_matches(const char *value) {
    if (contains_case_insensitive(value, "ANY")) {
        return 1;
    }
    if (!config_platform_matches(value)) {
        return 0;
    }
    if (!strchr(value, '"')) {
        return 1;
    }
    return contains_case_insensitive(value, "R800i") ||
           contains_case_insensitive(value, "Sony Ericsson Xperia Play");
}

static int config_condition_matches(const char *condition) {
    char copy[256];
    snprintf(copy, sizeof(copy), "%s", condition ? condition : "");
    char *expr = trim(copy);
    if (!expr[0]) {
        return 1;
    }
    if (strncasecmp(expr, "OS=", 3) == 0) {
        return config_platform_matches(expr + 3);
    }
    if (strncasecmp(expr, "ID=", 3) == 0 || strncasecmp(expr, "CLASS=", 6) == 0) {
        char *equals = strchr(expr, '=');
        return equals && config_id_matches(trim(equals + 1));
    }
    if (expr[0] == '[') {
        char *section_end = strchr(expr, ']');
        char *equals = strstr(expr, "==");
        if (!section_end || !equals || equals <= section_end) {
            return 0;
        }
        *section_end = 0;
        *equals = 0;
        char *section = trim(expr + 1);
        char *key = trim(section_end + 1);
        char *value = trim(equals + 2);
        strip_quotes(value);
        const char *current = config_get(section, key);
        return current && strcasecmp(current, value) == 0;
    }
    return 0;
}

static int parse_config_int_value(const char *value, int depth, int32_t *out) {
    if (!value || !out || depth > 4) {
        return 0;
    }
    char copy[256];
    snprintf(copy, sizeof(copy), "%s", value);
    char *text = trim(copy);
    if (!text[0]) {
        return 0;
    }
    if (text[0] == '[') {
        char *section_end = strchr(text, ']');
        if (!section_end) {
            return 0;
        }
        *section_end = 0;
        char *section = trim(text + 1);
        char *key = trim(section_end + 1);
        char *op = strpbrk(key, "+-");
        char operator= 0;
        int32_t adjustment = 0;
        if (op) {
            operator= * op;
            *op = 0;
            char *adj = trim(op + 1);
            char *end = NULL;
            long parsed = strtol(adj, &end, 0);
            if (end == adj) {
                return 0;
            }
            adjustment = (int32_t)parsed;
        }
        int32_t base = 0;
        if (!parse_config_int_value(config_get(section, trim(key)), depth + 1, &base)) {
            return 0;
        }
        if (operator== '+') {
            base += adjustment;
        } else if (operator== '-') {
            base -= adjustment;
        }
        *out = base;
        return 1;
    }
    char *end = NULL;
    long parsed = strtol(text, &end, 0);
    if (end == text) {
        return 0;
    }
    *out = (int32_t)parsed;
    return 1;
}

/* NFS Shift (NextOS): overrides de ICF por env, sem rebuild.
   SIMS3_ICF="secao.chave=valor;chave=valor" (secao default = game).
   Ex.: SIMS3_ICF="game.ShowFPS=1;game.SkipTutorial=1;trace.SOUND=1" */
static void apply_env_overrides(void) {
    const char *env = getenv("SIMS3_ICF");
    if (!env || !env[0]) {
        return;
    }
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", env);
    char *save = NULL;
    for (char *tok = strtok_r(buf, ";", &save); tok; tok = strtok_r(NULL, ";", &save)) {
        char *equals = strchr(tok, '=');
        if (!equals) {
            continue;
        }
        *equals = 0;
        char *key = trim(tok);
        char *value = trim(equals + 1);
        strip_quotes(value);
        const char *section = "game";
        char *dot = strchr(key, '.');
        if (dot) {
            *dot = 0;
            section = trim(key);
            key = trim(dot + 1);
        }
        config_set(section, key, value);
        fprintf(stderr, "[config] override [%s] %s=%s\n", section, key, value);
    }
}

void s3e_host_set_config(const uint8_t *data, uint32_t size) {
    g_config_entry_count = 0;
    if (data && size) {
        char *text = malloc((size_t)size + 1);
        if (text) {
            memcpy(text, data, size);
            text[size] = 0;
            char section[64] = "";
            int active = 1;
            char *save = NULL;
            for (char *line = strtok_r(text, "\n", &save); line;
                 line = strtok_r(NULL, "\n", &save)) {
                line[strcspn(line, "\r")] = 0;
                strip_comment(line);
                char *clean = trim(line);
                if (!clean[0]) {
                    continue;
                }
                size_t len = strlen(clean);
                if (clean[0] == '{' && clean[len - 1] == '}') {
                    clean[len - 1] = 0;
                    active = config_condition_matches(clean + 1);
                    continue;
                }
                if (clean[0] == '[' && clean[len - 1] == ']') {
                    clean[len - 1] = 0;
                    snprintf(section, sizeof(section), "%s", trim(clean + 1));
                    continue;
                }
                if (!active) {
                    continue;
                }
                char *equals = strchr(clean, '=');
                if (!equals) {
                    continue;
                }
                *equals = 0;
                char *key = trim(clean);
                char *value = trim(equals + 1);
                strip_quotes(value);
                config_set(section, key, value);
            }
            free(text);
        }
    }

    /* NFS Shift (NextOS): o ICF embutido do modulo ja traz tudo ([game]
       KeyControl*, SysGlesVersion=1, CommonDzFile/GfxDzFile). Aqui so
       aplicamos os overrides de env por cima. */
    apply_env_overrides();
}

/* The Sims 3 (NextOS): o modulo nao traz ICF embutido (config_size==0). O jogo
   depende do app.icf/s3e.icf que no Android ficam em assets/. Lemos os dois do
   disco na ORDEM da Marmalade — s3e.icf (global do SDK) primeiro, app.icf (do
   app) por cima — e so entao aplicamos os overrides de env. O parser de
   condicionais ja casa {OS=ANDROID}, que e' onde moram SysGlesVersion=2 e
   MemSize0=70000000 deste jogo. */
static void append_icf_file(char **text, size_t *len, size_t *cap, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return;
    }
    char chunk[4096];
    size_t got;
    while ((got = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (*len + got + 2 > *cap) {
            size_t ncap = (*cap ? *cap * 2 : 8192);
            while (ncap < *len + got + 2) {
                ncap *= 2;
            }
            char *n = realloc(*text, ncap);
            if (!n) {
                fclose(f);
                return;
            }
            *text = n;
            *cap = ncap;
        }
        memcpy(*text + *len, chunk, got);
        *len += got;
    }
    fclose(f);
    (*text)[(*len)++] = '\n';
    (*text)[*len] = 0;
    fprintf(stderr, "[config] loaded %s\n", path);
}

void s3e_host_set_config_from_files(const char *root) {
    static const char *names[] = {"assets/s3e.icf", "assets/app.icf", "s3e.icf", "app.icf"};
    char *text = NULL;
    size_t len = 0, cap = 0;
    char path[1200];
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        snprintf(path, sizeof(path), "%s/%s", root ? root : ".", names[i]);
        append_icf_file(&text, &len, &cap, path);
    }
    if (!text) {
        fprintf(stderr, "[config] no external ICF found under %s\n", root ? root : ".");
        s3e_host_set_config(NULL, 0);
        return;
    }
    s3e_host_set_config((const uint8_t *)text, (uint32_t)len);
    free(text);
    fprintf(stderr, "[config] %zu entries active\n", g_config_entry_count);
}

int32_t s3eConfigGetInt(const char *section, const char *key, int32_t *out) {
    const char *value = config_get(section, key);
    int32_t parsed = 0;
    int found = value && parse_config_int_value(value, 0, &parsed);
    if (found && out) {
        *out = parsed;
    }
    return found ? 0 : 1;
}

int32_t s3eConfigGetString(const char *section, const char *key, char *out) {
    const char *value = config_get(section, key);
    if (!value || !out) {
        return 1;
    }
    strcpy(out, value);
    return 0;
}
