/* NFS Shift (NextOS) — leitor derbh (arquivos .dz da Marmalade).
 *
 * As .dz do jogo (common.dz, gfx.dz) sao arquivos DTRZ cujos membros estao
 * comprimidos em LZMA-alone (cabecalho 0x5d ...). O jogo registra um filesystem
 * derbh via s3eFileAddUserFileSys esperando que a camada s3eFile roteie os opens
 * de membros pra ele (que descomprime). O loader base stubava esse registro, de
 * modo que os membros nunca eram servidos -> grupos de recurso NULL -> crash na
 * 1a textura ("EASplash").
 *
 * Em vez de reimplementar o roteamento por callbacks (ABI fragil), servimos os
 * membros DIRETO: indexamos as .dz, e no s3eFileOpen de um membro descomprimimos
 * o LZMA e entregamos os bytes ja descomprimidos via fmemopen. Determinístico e
 * testavel. O casamento de nome e por BASENAME (o jogo pede "iwlabs/x.group.bin",
 * "res/y.m3g", "/z.m3g" — todos batem com o membro "x.group.bin"/"y.m3g"/...).
 */
#include "s3e_host_internal.h"
#include "LzmaDec.h"

#define DERBH_MAX_ARCHIVES 4
#define DERBH_NAME_MAX 128
#define DERBH_LZMA_HEADER 13u /* 5 props + 8 uncompressed-size (LZMA-alone) */

#define DERBH_DIR_MAX 64
#define DERBH_MAX_GROUPS 256

struct derbh_entry {
    char base[DERBH_NAME_MAX]; /* basename minusculo */
    char dir[DERBH_DIR_MAX];   /* grupo do membro, normalizado ("l10n/en") */
    int archive;
    uint32_t off;         /* offset do membro (comeca no cabecalho LZMA) */
    uint32_t comp_size;   /* tamanho comprimido (inclui cabecalho de 13B) */
    uint32_t uncomp_size; /* tamanho descomprimido */
};

static char g_archive_path[DERBH_MAX_ARCHIVES][1200];
static long g_archive_size[DERBH_MAX_ARCHIVES];
static int g_archive_count;
static struct derbh_entry *g_entries;
static size_t g_entry_count;
static size_t g_entry_cap;
static int g_loaded;

static void *derbh_sz_alloc(ISzAllocPtr p, size_t size) {
    (void)p;
    return malloc(size);
}
static void derbh_sz_free(ISzAllocPtr p, void *addr) {
    (void)p;
    free(addr);
}
static const ISzAlloc g_derbh_alloc = {derbh_sz_alloc, derbh_sz_free};

static uint16_t derbh_rd16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t derbh_rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void derbh_base_lower(const char *name, char *out, size_t out_size) {
    const char *slash = strrchr(name ? name : "", '/');
    const char *base = slash ? slash + 1 : (name ? name : "");
    size_t i = 0;
    for (; base[i] && i + 1 < out_size; ++i) {
        out[i] = (char)tolower((unsigned char)base[i]);
    }
    out[i] = 0;
}

static int derbh_read_cstr(FILE *f, char *out, size_t out_size) {
    size_t len = 0;
    int ch;
    while ((ch = fgetc(f)) != EOF) {
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

/* Indexa uma .dz DTRZ. Le so a regiao de indice (cabecalho+nomes+registros),
   nao os 150MB de dados. Depois calcula o tamanho comprimido pelo gap entre
   offsets ordenados. */
static int derbh_index_archive(const char *path) {
    if (g_archive_count >= DERBH_MAX_ARCHIVES) {
        return 0;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    long fsize = ftell(f);
    if (fsize <= 9) {
        fclose(f);
        return 0;
    }
    rewind(f);

    uint8_t hdr[9];
    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr) || memcmp(hdr, "DTRZ", 4) != 0) {
        fclose(f);
        return 0;
    }
    uint16_t file_count = derbh_rd16(hdr + 4);
    uint16_t group_count = derbh_rd16(hdr + 6);
    if (file_count == 0) {
        fclose(f);
        return 0;
    }

    int archive_index = g_archive_count;
    snprintf(g_archive_path[archive_index], sizeof(g_archive_path[archive_index]), "%s", path);
    g_archive_size[archive_index] = fsize;

    /* nomes dos membros */
    char (*names)[DERBH_NAME_MAX] = calloc(file_count, DERBH_NAME_MAX);
    if (!names) {
        fclose(f);
        return 0;
    }
    for (uint16_t i = 0; i < file_count; ++i) {
        if (!derbh_read_cstr(f, names[i], DERBH_NAME_MAX)) {
            free(names);
            fclose(f);
            return 0;
        }
    }
    /* The Sims 3 (NextOS): os nomes de GRUPO deixam de ser descartados.
       O res.dz tem SEIS membros chamados strings_generic.bin (um por idioma) e
       outros seis strings_hdr.bin; casar so por basename servia sempre o
       PRIMEIRO (alemao) — era por isso que a tela de idioma vinha "Sprache" com
       "Deutsch" em todas as linhas. O jogo pede o caminho COMPLETO
       ("/l10n/en/strings_hdr.bin"), entao guardamos o grupo de cada membro. */
    char (*groups)[DERBH_DIR_MAX] = calloc(DERBH_MAX_GROUPS, DERBH_DIR_MAX);
    if (!groups) {
        free(names);
        fclose(f);
        return 0;
    }
    char scratch[DERBH_NAME_MAX];
    uint16_t group_names = group_count > 0 ? (uint16_t)(group_count - 1) : 0;
    for (uint16_t i = 0; i < group_names; ++i) {
        if (!derbh_read_cstr(f, scratch, sizeof(scratch))) {
            free(groups);
            free(names);
            fclose(f);
            return 0;
        }
        if (i < DERBH_MAX_GROUPS) {
            /* normaliza: separador do container e' "\\", o jogo pede com "/" */
            size_t k = 0;
            for (size_t j = 0; scratch[j] && k + 1 < DERBH_DIR_MAX; ++j) {
                char c = scratch[j] == '\\' ? '/' : scratch[j];
                groups[i][k++] = (char)tolower((unsigned char)c);
            }
            groups[i][k] = 0;
        }
    }
    /* marker de 4 bytes: common.dz usa 1, gfx.dz usa 0 — NAO exigir valor fixo
       (o loader base exigia ==1, o que rejeitava o gfx.dz e derrubava TODAS as
       texturas). So consumimos os 4 bytes. */
    uint8_t marker[4];
    if (fread(marker, 1, sizeof(marker), f) != sizeof(marker)) {
        free(groups);
        free(names);
        fclose(f);
        return 0;
    }
    /* registros de 6 bytes por membro: (0xffff, indice de grupo, ordem).
       MEDIDO no res.dz do Sims 3 conferindo o CONTEUDO descomprimido dos 12
       membros de l10n: o indice de grupo e' 2-based sobre a lista de nomes
       (grupo g -> nomes[g-2]); g<2 nao tem nome (raiz). Confere nos 6 idiomas:
       g=5,6 -> l10n/de (alemao), g=7,8 -> l10n/en (ingles), 9,10 -> es_ES,
       11,12 -> fr_FR, 13,14 -> it, 15,16 -> pt_BR. */
    uint16_t *member_group = calloc(file_count, sizeof(uint16_t));
    if (!member_group) {
        free(groups);
        free(names);
        fclose(f);
        return 0;
    }
    for (uint16_t i = 0; i < file_count; ++i) {
        uint8_t rec6[6];
        if (fread(rec6, 1, sizeof(rec6), f) != sizeof(rec6)) {
            free(member_group);
            free(groups);
            free(names);
            fclose(f);
            return 0;
        }
        member_group[i] = derbh_rd16(rec6 + 2);
    }
    /* registros de 16 bytes: off(+0), uncomp_size(+4), (+8 dup), (+12 flag) */
    uint32_t *offs = calloc(file_count, sizeof(uint32_t));
    uint32_t *usz = calloc(file_count, sizeof(uint32_t));
    if (!offs || !usz) {
        free(offs);
        free(usz);
        free(member_group);
        free(groups);
        free(names);
        fclose(f);
        return 0;
    }
    for (uint16_t i = 0; i < file_count; ++i) {
        uint8_t rec[16];
        if (fread(rec, 1, sizeof(rec), f) != sizeof(rec)) {
            free(offs);
            free(usz);
            free(member_group);
            free(groups);
            free(names);
            fclose(f);
            return 0;
        }
        offs[i] = derbh_rd32(rec);
        usz[i] = derbh_rd32(rec + 4);
    }
    fclose(f);

    /* tamanho comprimido = gap ate o proximo offset (ordenado); ultimo vai ate
       o fim do arquivo. */
    for (uint16_t i = 0; i < file_count; ++i) {
        uint32_t next = (uint32_t)fsize;
        for (uint16_t j = 0; j < file_count; ++j) {
            if (offs[j] > offs[i] && offs[j] < next) {
                next = offs[j];
            }
        }
        uint32_t comp = (next > offs[i]) ? (next - offs[i]) : 0;

        if (g_entry_count >= g_entry_cap) {
            size_t ncap = g_entry_cap ? g_entry_cap * 2 : 2048;
            struct derbh_entry *n = realloc(g_entries, ncap * sizeof(*g_entries));
            if (!n) {
                free(offs);
                free(usz);
                free(member_group);
                free(groups);
                free(names);
                return 0;
            }
            g_entries = n;
            g_entry_cap = ncap;
        }
        struct derbh_entry *e = &g_entries[g_entry_count++];
        derbh_base_lower(names[i], e->base, sizeof(e->base));
        e->dir[0] = 0;
        {
            int g = (int)member_group[i] - 2;
            if (g >= 0 && g < DERBH_MAX_GROUPS) {
                snprintf(e->dir, sizeof(e->dir), "%s", groups[g]);
            }
        }
        e->archive = archive_index;
        e->off = offs[i];
        e->comp_size = comp;
        e->uncomp_size = usz[i];
    }

    g_archive_count++;
    free(offs);
    free(usz);
    free(member_group);
    free(groups);
    free(names);
    return 1;
}

void derbh_init(void) {
    if (g_loaded) {
        return;
    }
    g_loaded = 1;
    /* The Sims 3 (NextOS): o OBB traz DUAS arvores — assets/HighRes/res.dz e
       assets/LowRes/res.dz. Quem escolhe e o JOGO (por resolucao), mas o pacote
       so leva a LowRes (Mali-450). Indexamos o que existir, LowRes primeiro:
       679 membros medidos, todos LZMA-alone, 100% descomprimindo. */
    static const char *archives[] = {
        "assets/LowRes/res.dz",
        "assets/HighRes/res.dz",
        "res.dz",
    };
    char path[1200];
    for (size_t i = 0; i < sizeof(archives) / sizeof(archives[0]); ++i) {
        snprintf(path, sizeof(path), "%s/%s", g_root, archives[i]);
        if (access(path, R_OK) == 0) {
            derbh_index_archive(path);
        }
    }
    if (getenv("SIMS3_FILE_LOG")) {
        fprintf(stderr, "[derbh] indexed %d archive(s), %zu members\n", g_archive_count,
                g_entry_count);
    }
}

/* Diretorio pedido, normalizado e minusculo, sem barras nas pontas.
   "/l10n/en/strings_hdr.bin" -> "l10n/en";  "x.png" -> "" */
static void derbh_dir_lower(const char *name, char *out, size_t out_size) {
    out[0] = 0;
    if (!name) {
        return;
    }
    const char *slash = strrchr(name, '/');
    const char *back = strrchr(name, '\\');
    if (back > slash) {
        slash = back;
    }
    if (!slash) {
        return;
    }
    const char *start = name;
    while (*start == '/' || *start == '\\') {
        start++;
    }
    if (slash <= start) {
        return;
    }
    size_t k = 0;
    for (const char *p = start; p < slash && k + 1 < out_size; ++p) {
        char c = (*p == '\\') ? '/' : *p;
        out[k++] = (char)tolower((unsigned char)c);
    }
    out[k] = 0;
}

static const struct derbh_entry *derbh_find(const char *name) {
    if (!g_loaded) {
        derbh_init();
    }
    char base[DERBH_NAME_MAX];
    derbh_base_lower(name, base, sizeof(base));
    if (!base[0]) {
        return NULL;
    }

    /* The Sims 3 (NextOS): o basename NAO e' unico neste container — ha SEIS
       strings_generic.bin e SEIS strings_hdr.bin, um por idioma. Servir sempre o
       primeiro entregava o ALEMAO para todos os pedidos, e a tela de idioma
       aparecia como "Sprache" com "Deutsch" em todas as linhas (regra #5).
       Quando o pedido traz diretorio ("/l10n/en/strings_hdr.bin"), ele MANDA:
       so casa o membro cujo grupo bate. Sem diretorio, mantem o comportamento
       antigo (primeiro que casar), que ja servia os 667 membros unicos. */
    char want_dir[DERBH_DIR_MAX];
    derbh_dir_lower(name, want_dir, sizeof(want_dir));
    if (want_dir[0]) {
        for (size_t i = 0; i < g_entry_count; ++i) {
            if (strcmp(g_entries[i].base, base) == 0 &&
                strcmp(g_entries[i].dir, want_dir) == 0) {
                return &g_entries[i];
            }
        }
    }

    for (size_t i = 0; i < g_entry_count; ++i) {
        if (strcmp(g_entries[i].base, base) == 0) {
            return &g_entries[i];
        }
    }
    return NULL;
}

int derbh_exists(const char *name) {
    return derbh_find(name) != NULL;
}

/* Descomprime o membro (LZMA-alone) e devolve um FILE* de memoria. O buffer e
   liberado quando o s3eFileClose fecha o FILE (via track_memory_file). */
void *derbh_open(const char *name) {
    const struct derbh_entry *e = derbh_find(name);
    if (!e) {
        return NULL;
    }
    /* The Sims 3 (NextOS): o container tem membro LEGITIMO de tamanho ZERO
       (medido: image.bin, 0 bytes descomprimidos). Devolver NULL para ele fazia
       o s3eFileOpen("/bin/image.bin") dar MISS, e o jogo tratava como recurso
       ausente. fmemopen com tamanho 0 falha na glibc, entao servimos /dev/null:
       leitura da EOF na hora e o tamanho e' 0, que e' exatamente a verdade. */
    if (e->uncomp_size == 0) {
        if (getenv("SIMS3_FILE_LOG")) {
            fprintf(stderr, "[derbh] served %s (membro vazio, 0 bytes)\n", name);
        }
        return fopen("/dev/null", "rb");
    }
    if (e->comp_size <= DERBH_LZMA_HEADER) {
        return NULL;
    }
    FILE *arc = fopen(g_archive_path[e->archive], "rb");
    if (!arc) {
        return NULL;
    }
    uint8_t *comp = malloc(e->comp_size);
    if (!comp) {
        fclose(arc);
        return NULL;
    }
    if (fseek(arc, (long)e->off, SEEK_SET) != 0 ||
        fread(comp, 1, e->comp_size, arc) != e->comp_size) {
        free(comp);
        fclose(arc);
        return NULL;
    }
    fclose(arc);

    uint8_t *out = malloc(e->uncomp_size);
    if (!out) {
        free(comp);
        return NULL;
    }
    SizeT dst_len = e->uncomp_size;
    SizeT src_len = e->comp_size - DERBH_LZMA_HEADER; /* pula 5 props + 8 size */
    ELzmaStatus status;
    /* propData = os 5 bytes de propriedades LZMA-alone (props + dictsize) */
    SRes rc = LzmaDecode(out, &dst_len, comp + DERBH_LZMA_HEADER, &src_len, comp, 5,
                         LZMA_FINISH_ANY, &status, &g_derbh_alloc);
    free(comp);
    if (rc != SZ_OK || dst_len != e->uncomp_size) {
        if (getenv("SIMS3_FILE_LOG")) {
            fprintf(stderr, "[derbh] LZMA fail for %s rc=%d got=%lu want=%u\n", name, (int)rc,
                    (unsigned long)dst_len, e->uncomp_size);
        }
        free(out);
        return NULL;
    }

    /* SIMS3_DUMP_MATCH=<pedaco do nome>: grava o membro ja descomprimido em
       /tmp. E' o jeito honesto de LER o conteudo do .dz (tabela de strings,
       por exemplo) sem reimplementar o formato fora do loader. */
    {
        const char *want = getenv("SIMS3_DUMP_MATCH");
        if (want && want[0] && strstr(name, want)) {
            char out_path[512];
            const char *b = strrchr(name, '/');
            snprintf(out_path, sizeof(out_path), "/tmp/%s", b ? b + 1 : name);
            FILE *df = fopen(out_path, "wb");
            if (df) {
                fwrite(out, 1, e->uncomp_size, df);
                fclose(df);
                fprintf(stderr, "[derbh] despejado %s (%u bytes)\n", out_path,
                        (unsigned)e->uncomp_size);
                fflush(stderr);
            }
        }
    }
    FILE *mem = fmemopen(out, e->uncomp_size, "rb");
    if (!mem) {
        free(out);
        return NULL;
    }
    derbh_track_memory_file(mem, out);
    if (getenv("SIMS3_FILE_LOG")) {
        fprintf(stderr, "[derbh] served %s (%u bytes)\n", name, e->uncomp_size);
    }
    return mem;
}
