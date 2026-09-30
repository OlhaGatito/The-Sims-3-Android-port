#include "s3e_host.h"
#include "s3e_image.h"
#include "s3e_host_internal.h"
#include "nxmix.h"
#include "LzmaDec.h"

#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#if defined(__linux__)
#include <ucontext.h>
#endif
#include <unistd.h>
#include <dlfcn.h>

static uintptr_t g_loaded_base;
static uint32_t g_loaded_size;
static int g_nullacc_recovery;
static int g_mapfix;
static unsigned g_mapfix_hits;
static unsigned g_mapfix_hits2;
static const char g_empty_string[8] __attribute__((aligned(8))) = "";
static uint32_t g_bucket_allocator_table[33] __attribute__((aligned(8)));

enum {
    BUCKET_ALLOCATOR_OBJECT_OFFSET = 0x41d498u,
    BUCKET_ALLOCATOR_TABLE_SLOT = 0x4cu,
};

static void attach_bucket_allocator_table(uint32_t object) {
    uint32_t *table_slot = (uint32_t *)(uintptr_t)(object + BUCKET_ALLOCATOR_TABLE_SLOT);
    *table_slot = (uint32_t)(uintptr_t)g_bucket_allocator_table;
}

static void prepare_bucket_allocator_table(uint32_t object) {
    if (!object) {
        return;
    }

    attach_bucket_allocator_table(object);
    memset(g_bucket_allocator_table, 0, sizeof(g_bucket_allocator_table));
}

static bool recover_bucket_allocator_fault(ucontext_t *uc) {
    uintptr_t pc = uc->uc_mcontext.arm_pc;
    if (pc != g_loaded_base + 0x374be8u && pc != g_loaded_base + 0x374bf4u) {
        return false;
    }

    uint32_t object = uc->uc_mcontext.arm_r5 ? uc->uc_mcontext.arm_r5 : uc->uc_mcontext.arm_r0;
    uint32_t index = uc->uc_mcontext.arm_r4 ? uc->uc_mcontext.arm_r4 : uc->uc_mcontext.arm_r1;
    if (!object) {
        return false;
    }
    if (index >= 32 && pc == g_loaded_base + 0x374be8u) {
        index = 1;
    }
    if (index >= 32) {
        return false;
    }

    attach_bucket_allocator_table(object);

    uc->uc_mcontext.arm_r0 = object;
    uc->uc_mcontext.arm_r1 = index;
    uc->uc_mcontext.arm_r2 = (uint32_t)(uintptr_t)g_bucket_allocator_table;
    uc->uc_mcontext.arm_r4 = index;
    uc->uc_mcontext.arm_r5 = object;

    if (pc == g_loaded_base + 0x374be8u) {
        prepare_bucket_allocator_table(object);
        uc->uc_mcontext.arm_pc = g_loaded_base + 0x374be8u;
    } else {
        g_bucket_allocator_table[index] = 0;
        uc->uc_mcontext.arm_pc = g_loaded_base + 0x374c34u;
    }
    return true;
}

static bool recover_null_buffer_write(ucontext_t *uc) {
    if (uc->uc_mcontext.arm_pc != g_loaded_base + 0xcb8eeu || uc->uc_mcontext.arm_r0 != 0) {
        return false;
    }

    uint32_t *sp = (uint32_t *)(uintptr_t)uc->uc_mcontext.arm_sp;
    uc->uc_mcontext.arm_r4 = sp[0];
    uc->uc_mcontext.arm_r5 = sp[1];
    uc->uc_mcontext.arm_r6 = sp[2];
    uc->uc_mcontext.arm_r0 = 0;
    uc->uc_mcontext.arm_sp += 16;
    uc->uc_mcontext.arm_pc = sp[3] & ~1u;
    return true;
}

static bool recover_null_buffer_slot(ucontext_t *uc) {
    uintptr_t pc = uc->uc_mcontext.arm_pc;
    if ((pc != g_loaded_base + 0xd291cu && pc != g_loaded_base + 0xd293au) ||
        uc->uc_mcontext.arm_r2 != 0) {
        return false;
    }

    uint32_t *sp = (uint32_t *)(uintptr_t)uc->uc_mcontext.arm_sp;
    uc->uc_mcontext.arm_r3 = sp[0];
    uc->uc_mcontext.arm_r4 = sp[1];
    uc->uc_mcontext.arm_r5 = sp[2];
    uc->uc_mcontext.arm_r6 = sp[3];
    uc->uc_mcontext.arm_r7 = sp[4];
    uc->uc_mcontext.arm_r0 = 0;
    uc->uc_mcontext.arm_sp += 24;
    uc->uc_mcontext.arm_pc = sp[5] & ~1u;
    return true;
}

/* The Sims 3 (NextOS): o dump do loader base so imprime registradores. Com o
   modulo chamando funcoes do HOST por ponteiro, um pc fora de 0x4a000000 nao
   diz nada sozinho — precisamos saber em QUE biblioteca/simbolo caiu. dladdr +
   a linha de /proc/self/maps resolvem isso sem gdb no device. */
static void describe_addr(const char *label, uintptr_t addr) {
    if (!addr) {
        return;
    }
    Dl_info info;
    memset(&info, 0, sizeof(info));
    if (dladdr((void *)addr, &info) && info.dli_fname) {
        fprintf(stderr, "  %s 0x%08lx -> %s@0x%08lx+0x%lx", label, (unsigned long)addr,
                info.dli_fname, (unsigned long)(uintptr_t)info.dli_fbase,
                (unsigned long)(addr - (uintptr_t)info.dli_fbase));
        if (info.dli_sname) {
            fprintf(stderr, " (%s+0x%lx)", info.dli_sname,
                    (unsigned long)(addr - (uintptr_t)info.dli_saddr));
        }
        fprintf(stderr, "\n");
        return;
    }
    fprintf(stderr, "  %s 0x%08lx -> (sem simbolo)\n", label, (unsigned long)addr);
}

static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s [--run] [--root DIR] IMAGE.s3e.unpacked\n", argv0);
    fprintf(stderr, "       %s --unpack-s3e INPUT.s3e OUTPUT.xe3u\n", argv0);
}

#define S3E_LZMA_HEADER_SIZE 13u
#define S3E_LZMA_MAX_INPUT_SIZE (64u * 1024u * 1024u)
#define S3E_LZMA_MAX_OUTPUT_SIZE (256u * 1024u * 1024u)

static void *s3e_lzma_alloc(ISzAllocPtr allocator, size_t size) {
    (void)allocator;
    return malloc(size);
}

static void s3e_lzma_free(ISzAllocPtr allocator, void *address) {
    (void)allocator;
    free(address);
}

static const ISzAlloc s3e_lzma_allocator = {
    s3e_lzma_alloc,
    s3e_lzma_free
};

static int unpack_s3e_file(const char *source_path, const char *destination_path) {
    if (strcmp(source_path, destination_path) == 0) {
        fprintf(stderr, "[unpack] input and output paths must differ\n");
        return 2;
    }

    FILE *source_file = fopen(source_path, "rb");
    if (!source_file) {
        perror("[unpack] cannot open S3E input");
        return 1;
    }
    if (fseek(source_file, 0, SEEK_END) != 0) {
        perror("[unpack] cannot seek S3E input");
        fclose(source_file);
        return 1;
    }

    long source_length_long = ftell(source_file);
    if (source_length_long < (long)S3E_LZMA_HEADER_SIZE ||
        (unsigned long)source_length_long > S3E_LZMA_MAX_INPUT_SIZE) {
        fprintf(stderr, "[unpack] invalid or oversized LZMA-alone input (%ld bytes)\n",
                source_length_long);
        fclose(source_file);
        return 1;
    }
    if (fseek(source_file, 0, SEEK_SET) != 0) {
        perror("[unpack] cannot rewind S3E input");
        fclose(source_file);
        return 1;
    }

    size_t source_length = (size_t)source_length_long;
    uint8_t *source = (uint8_t *)malloc(source_length);
    if (!source) {
        fprintf(stderr, "[unpack] out of memory reading S3E input\n");
        fclose(source_file);
        return 1;
    }
    if (fread(source, 1, source_length, source_file) != source_length) {
        fprintf(stderr, "[unpack] could not read complete S3E input\n");
        free(source);
        fclose(source_file);
        return 1;
    }
    fclose(source_file);

    uint32_t dictionary_size = (uint32_t)source[1] |
                               ((uint32_t)source[2] << 8) |
                               ((uint32_t)source[3] << 16) |
                               ((uint32_t)source[4] << 24);
    if (dictionary_size > S3E_LZMA_MAX_OUTPUT_SIZE) {
        fprintf(stderr, "[unpack] invalid or oversized LZMA dictionary (%u bytes)\n",
                dictionary_size);
        free(source);
        return 1;
    }

    uint64_t expected_length = 0;
    for (unsigned i = 0; i < 8; ++i) {
        expected_length |= (uint64_t)source[5 + i] << (8u * i);
    }
    if (expected_length < 4 || expected_length > S3E_LZMA_MAX_OUTPUT_SIZE) {
        fprintf(stderr, "[unpack] invalid or oversized uncompressed length (%llu bytes)\n",
                (unsigned long long)expected_length);
        free(source);
        return 1;
    }

    size_t output_length = (size_t)expected_length;
    uint8_t *output = (uint8_t *)malloc(output_length);
    if (!output) {
        fprintf(stderr, "[unpack] out of memory allocating %zu-byte output\n", output_length);
        free(source);
        return 1;
    }

    SizeT decoded_length = (SizeT)output_length;
    SizeT compressed_length = (SizeT)(source_length - S3E_LZMA_HEADER_SIZE);
    ELzmaStatus decode_status = LZMA_STATUS_NOT_SPECIFIED;
    /* The LZMA-alone header supplies the exact output size. FINISH_ANY lets
       the bundled SDK stop at that size without requiring a stream end marker. */
    SRes decode_result = LzmaDecode(output, &decoded_length,
                                    source + S3E_LZMA_HEADER_SIZE, &compressed_length,
                                    source, 5, LZMA_FINISH_ANY, &decode_status,
                                    &s3e_lzma_allocator);
    free(source);

    if (decode_result != SZ_OK || decoded_length != output_length) {
        fprintf(stderr,
                "[unpack] LZMA SDK decode failed: result=%d decoded=%lu expected=%zu status=%d\n",
                (int)decode_result, (unsigned long)decoded_length, output_length,
                (int)decode_status);
        free(output);
        return 1;
    }
    if (memcmp(output, "XE3U", 4) != 0) {
        fprintf(stderr, "[unpack] decoded payload does not have the XE3U signature\n");
        free(output);
        return 1;
    }

    char temporary_path[4096];
    int temporary_length = snprintf(temporary_path, sizeof(temporary_path),
                                    "%s.tmp.%ld", destination_path, (long)getpid());
    if (temporary_length < 0 || (size_t)temporary_length >= sizeof(temporary_path)) {
        fprintf(stderr, "[unpack] destination path is too long\n");
        free(output);
        return 1;
    }

    FILE *destination_file = fopen(temporary_path, "wb");
    if (!destination_file) {
        perror("[unpack] cannot create temporary output");
        free(output);
        return 1;
    }
    if (fwrite(output, 1, output_length, destination_file) != output_length ||
        fflush(destination_file) != 0) {
        perror("[unpack] could not write complete output");
        fclose(destination_file);
        unlink(temporary_path);
        free(output);
        return 1;
    }
    if (fclose(destination_file) != 0) {
        perror("[unpack] could not close output");
        unlink(temporary_path);
        free(output);
        return 1;
    }
    if (rename(temporary_path, destination_path) != 0) {
        perror("[unpack] could not publish decoded output");
        unlink(temporary_path);
        free(output);
        return 1;
    }

    fprintf(stderr, "[unpack] LZMA SDK decoded %zu -> %zu bytes; XE3U verified\n",
            source_length, output_length);
    free(output);
    return 0;
}

static void crash_handler(int sig, siginfo_t *info, void *context) {
#if defined(__linux__) && defined(__arm__)
    ucontext_t *uc = (ucontext_t *)context;
    /* NFS Shift (NextOS): as recuperacoes de fault do loader base sao calibradas
       BYTE-A-BYTE pro boz.s3e (offsets fixos, ex.: 0xcb8ee cai DENTRO do range
       do modulo do Shift). No Shift elas nao significam nada e poderiam
       "recuperar" um crash real no PC errado -> DESLIGADAS p/ termos dumps de
       crash limpos. As funcoes ficam p/ referencia (marcadas unused abaixo). */
    (void)recover_bucket_allocator_fault;
    (void)recover_null_buffer_write;
    (void)recover_null_buffer_slot;

    /* NFS Shift (NextOS): accessors otimizados do modulo de TEXTO/scene-graph
       (ex.: 0x1270ec, 0x126f54) tem o padrao `ldr r0,[r0,#8]` (0xe5900008)
       seguido de `b <helper null-safe>` (o helper faz cmp r0,#0; beq -> 0). O
       compilador removeu o null-check ANTES do ldr; quando o objeto (string/
       entidade) vem nulo, o ldr faz SIGSEGV em +8. Recuperacao GERAL: se r0==0 e
       a instrucao no pc e' exatamente esse ldr seguido de branch incondicional,
       pulamos o ldr e caimos no helper com r0=0 (devolve "vazio"). Isso espelha
       a semantica null-safe do PROPRIO jogo, sem inventar comportamento nem
       precisar mapear cada accessor. */
    /* The Sims 3 (NextOS): a recuperacao generica de accessor nulo veio do NFS
       Shift (mesmo compilador Marmalade). Aqui ela fica DESLIGADA por padrao —
       queremos dumps de crash limpos deste modulo — e o limite superior passa a
       ser o tamanho REAL do modulo carregado (o 0x1c3624 era do Shift).
       Ligar com SIMS3_NULLACC=1 se o mesmo padrao aparecer. */
    /* The Sims 3 (NextOS): recuperacao ESPECIFICA e MEDIDA de um acesso a mapa
       de hash com o vetor de baldes NULO, no caminho de sair da tela de idioma.

       Em base+0x41c10 o modulo faz `ldr r9,[r5]` logo depois de `lsl r5,r5,#2`,
       onde r5 e' o INDICE do balde (`hash & mask`, calculado em base+0x41ab0).
       Quando o vetor de baldes ainda nao existe, o compilador dobrou a base
       NULA e o endereco efetivo vira o proprio indice*4 — por isso o SIGSEGV cai
       sempre num endereco minusculo (medido: 0xff0 e 0xfb8, ou seja indices
       1020 e 1006), nunca num ponteiro plausivel.

       ⚠️ NAO E' SOLUCAO: devolver "nao achou" (r9=0) apenas EMPURRA a falha —
       o chamador recebe objeto nulo e morre em base+0x2804c. Fica DESLIGADA por
       padrao (SIMS3_MAPFIX=1 liga) e existe so como instrumento de diagnostico.
       A causa de verdade e' o vetor de baldes nao existir neste ponto. */
    /* Segundo null-check dobrado, no MESMO caminho: em base+0x2804c o modulo faz
       `ldr r3,[r4,#8]` com r4 = objeto NAO ENCONTRADO (nulo). Duas instrucoes
       depois ele TEM a saida correta: `cmp r3,#0; moveq r5,r3; beq +0x28130`.
       Ou seja, o proprio jogo sabe tratar "sem dados" — so nao checou o
       ponteiro antes. Repomos a checagem: r3=0, r5=0 e salta para a saida. */
    if (sig == SIGSEGV && g_module_base && g_mapfix >= 2 &&
        (uintptr_t)uc->uc_mcontext.arm_pc == g_module_base + 0x2804cu &&
        uc->uc_mcontext.arm_r4 == 0 && info && (uintptr_t)info->si_addr < 0x10000u) {
        /* r4 == 0 = objeto inexistente. A saida "sem dados" em +0x28130 ainda
           faz `ldr r3,[r4]` para soltar a referencia — com nulo nao ha o que
           soltar. O ponto certo e' +0x28158: `mov r0,r5; pop` = devolve r5.
           Com r5 = 0 fica "entrou nulo, sai nulo". */
        uc->uc_mcontext.arm_r3 = 0;
        uc->uc_mcontext.arm_r5 = 0;
        uc->uc_mcontext.arm_pc = g_module_base + 0x28158u;
        if (!g_mapfix_hits2++) {
            fprintf(stderr, "[fix] objeto nao encontrado em +0x2804c — usando a saida "
                            "'sem dados' do proprio jogo\n");
            fflush(stderr);
        }
        return;
    }

    if (sig == SIGSEGV && g_module_base && g_mapfix &&
        (uintptr_t)uc->uc_mcontext.arm_pc == g_module_base + 0x41c10u && info &&
        (uintptr_t)info->si_addr < 0x10000u) {
        uc->uc_mcontext.arm_r9 = 0;
        uc->uc_mcontext.arm_pc += 4u;
        if (!g_mapfix_hits++) {
            fprintf(stderr, "[fix] balde de hash nulo em +0x41c10 — devolvendo 'nao achou'\n");
            fflush(stderr);
        }
        return;
    }

    if (g_nullacc_recovery && sig == SIGSEGV && uc->uc_mcontext.arm_r0 == 0) {
        uint32_t pc = (uint32_t)uc->uc_mcontext.arm_pc;
        if (pc >= (uint32_t)g_loaded_base && pc + 8u <= (uint32_t)g_loaded_base + g_loaded_size) {
            const uint32_t *insn = (const uint32_t *)(uintptr_t)pc;
            if (insn[0] == 0xe5900008u && (insn[1] >> 24) == 0xeau) {
                uc->uc_mcontext.arm_pc = pc + 4u;
                return;
            }
        }
    }

    describe_addr("pc", (uintptr_t)uc->uc_mcontext.arm_pc);
    describe_addr("lr", (uintptr_t)uc->uc_mcontext.arm_lr);
    fprintf(stderr,
            "signal %d addr=%p pc=0x%08lx lr=0x%08lx sp=0x%08lx r0=0x%08lx r1=0x%08lx r2=0x%08lx "
            "r3=0x%08lx\n",
            sig, info ? info->si_addr : NULL, (unsigned long)uc->uc_mcontext.arm_pc,
            (unsigned long)uc->uc_mcontext.arm_lr, (unsigned long)uc->uc_mcontext.arm_sp,
            (unsigned long)uc->uc_mcontext.arm_r0, (unsigned long)uc->uc_mcontext.arm_r1,
            (unsigned long)uc->uc_mcontext.arm_r2, (unsigned long)uc->uc_mcontext.arm_r3);
    fprintf(stderr, "  regs r4=0x%08lx r5=0x%08lx r6=0x%08lx r7=0x%08lx r8=0x%08lx r9=0x%08lx "
                    "sl=0x%08lx fp=0x%08lx ip=0x%08lx\n",
            (unsigned long)uc->uc_mcontext.arm_r4, (unsigned long)uc->uc_mcontext.arm_r5,
            (unsigned long)uc->uc_mcontext.arm_r6, (unsigned long)uc->uc_mcontext.arm_r7,
            (unsigned long)uc->uc_mcontext.arm_r8, (unsigned long)uc->uc_mcontext.arm_r9,
            (unsigned long)uc->uc_mcontext.arm_r10, (unsigned long)uc->uc_mcontext.arm_fp,
            (unsigned long)uc->uc_mcontext.arm_ip);
    if (g_module_base && g_module_size) {
        s3e_dump_module_stack_from("crash", (uintptr_t)uc->uc_mcontext.arm_sp);
        if (getenv("SIMS3_MAPS")) {
            s3e_dump_module_maps();
        }
    }
#else
    (void)context;
    fprintf(stderr, "signal %d addr=%p\n", sig, info ? info->si_addr : NULL);
#endif
    _Exit(128 + sig);
}

static void install_crash_handlers(void) {
    /* The Sims 3 (NextOS): sem pilha alternativa, um SIGSEGV por ESTOURO DE
       PILHA mata o processo SEM imprimir nada — o handler nao tem onde rodar e
       o log fica vazio. Log vazio nao e' ausencia de erro. */
    /* glibc exposes SIGSTKSZ as a runtime value with _GNU_SOURCE; a fixed\n       64 KiB buffer keeps this static alternate signal stack portable. */\n    static char altstack[64u * 1024u];
    stack_t ss = {.ss_sp = altstack, .ss_size = sizeof(altstack), .ss_flags = 0};
    sigaltstack(&ss, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
}

static void terminate_handler(int sig) {
    (void)sig;
    _Exit(0);
}

static void install_terminate_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminate_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "--unpack-s3e") == 0) {
        return unpack_s3e_file(argv[2], argv[3]);
    }

    bool run = false;
    const char *root = NULL;
    const char *image_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--run") == 0) {
            run = true;
        } else if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (!image_path) {
            image_path = argv[i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (!image_path) {
        usage(argv[0]);
        return 2;
    }

    g_nullacc_recovery = getenv("SIMS3_NULLACC") && getenv("SIMS3_NULLACC")[0] == '1';
    /* SIMS3_MAPFIX=1 -> so a reposicao do null-check da BUSCA (base+0x41c10),
       que deixa o jogo receber "nao achou" e seguir pelo carregador dele.
       SIMS3_MAPFIX=2 -> tambem a de base+0x2804c. */
    {
        const char *mf = getenv("SIMS3_MAPFIX");
        g_mapfix = mf ? atoi(mf) : 0;
    }
    if (getenv("SIMS3_AUDIO_SELFTEST")) {
        void *sdl2 = NULL;
        const char *names[] = {"libSDL2-2.0.so.0", "libSDL2.so", NULL};
        for (int i = 0; names[i] && !sdl2; ++i) {
            sdl2 = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        }
        int rc = sdl2 ? nxmix_selftest(sdl2, 2) : 1;
        fprintf(stderr, "[nxmix] selftest rc=%d\n", rc);
        _Exit(rc);
    }

    install_crash_handlers();
    install_terminate_handlers();

    struct s3e_image image;
    if (!s3e_image_load(image_path, &image)) {
        return 1;
    }
    if (!s3e_image_parse_symbols(&image)) {
        s3e_image_free(&image);
        return 1;
    }

    fprintf(stderr, "S3E version=0x%x arch=0x%x symbols=%zu code=0x%x mem=0x%x\n",
            image.header.version, image.header.arch, image.symbols.count,
            image.header.code_file_size, image.header.code_mem_size);

    if (!s3e_host_init(root)) {
        s3e_image_free(&image);
        return 1;
    }
    /* The Sims 3 (NextOS): este modulo tem config_size==0 — o ICF NAO esta
       embutido no .s3e (medido: config_offset==fixup_offset, size 0). O jogo
       depende do app.icf/s3e.icf do APK (SysGlesVersion=2, MemSize0=70000000,
       DispFixRot, Language=efigs). Sem isso o host fica sem config nenhuma.
       Ordem Marmalade: s3e.icf (global) primeiro, app.icf (do app) por cima. */
    if (image.header.config_size) {
        s3e_host_set_config(image.file_data + image.header.config_offset,
                            image.header.config_size);
    } else {
        s3e_host_set_config_from_files(root ? root : ".");
    }

    struct s3e_loaded_image loaded;
    if (!s3e_image_map_and_relocate(&image, s3e_host_resolve, &loaded)) {
        s3e_host_shutdown();
        s3e_image_free(&image);
        return 1;
    }

    fprintf(stderr, "mapped S3E at %p, entry=%p\n", (void *)loaded.base,
            (void *)(loaded.base + loaded.entry_offset));
    g_loaded_base = (uintptr_t)loaded.base;
    g_loaded_size = image.header.code_mem_size;
    g_module_base = (uintptr_t)loaded.base;
    g_module_size = image.header.code_mem_size;
    s3e_install_vtable_hooks();
    s3e_install_bl_hooks();
    s3e_install_fn_hooks();
    s3e_apply_word_patches();
    if (run) {
        int (*entry)(void) = (int (*)(void))(uintptr_t)(loaded.base + loaded.entry_offset);
        int rc = entry();
        fprintf(stderr, "S3E entry returned %d\n", rc);
    }

    s3e_loaded_image_unmap(&loaded);
    s3e_host_shutdown();
    s3e_image_free(&image);
    return 0;
}
