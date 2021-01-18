/*
 *  Swirl - another tiny C compiler
 */

#if !defined ONE_SOURCE || ONE_SOURCE
#include "swirlpp.c"
#include "swirlgen.c"
#include "swirlelf.c"
#include "swirlrun.c"
#ifdef SWIRL_TARGET_I386
#include "i386-gen.c"
#include "i386-link.c"
#include "i386-asm.c"
#elif defined(SWIRL_TARGET_ARM)
#include "arm-gen.c"
#include "arm-link.c"
#include "arm-asm.c"
#elif defined(SWIRL_TARGET_ARM64)
#include "arm64-gen.c"
#include "arm64-link.c"
#include "arm-asm.c"
#elif defined(SWIRL_TARGET_C67)
#include "c67-gen.c"
#include "c67-link.c"
#include "swirlcoff.c"
#elif defined(SWIRL_TARGET_X86_64)
#include "x86_64-gen.c"
#include "x86_64-link.c"
#include "i386-asm.c"
#elif defined(SWIRL_TARGET_RISCV64)
#include "riscv64-gen.c"
#include "riscv64-link.c"
#include "riscv64-asm.c"
#else
#error unknown target
#endif
#ifdef CONFIG_SWIRL_ASM
#include "swirlasm.c"
#endif
#ifdef SWIRL_TARGET_PE
#include "swirlpe.c"
#endif
#ifdef SWIRL_TARGET_MACHO
#include "swirlmacho.c"
#endif
#endif /* ONE_SOURCE */

#include "swirl.h"

/********************************************************/
/* global variables */

/* XXX: get rid of this ASAP (or maybe not) */
ST_DATA struct SwirlState *swirl_state;

#ifdef MEM_DEBUG
static int nb_states;
#endif

/********************************************************/
#ifdef _WIN32
ST_FUNC char *normalize_slashes(char *path)
{
    char *p;
    for (p = path; *p; ++p)
        if (*p == '\\')
            *p = '/';
    return path;
}

static HMODULE swirl_module;

/* on win32, we suppose the lib and includes are at the location of 'swirl.exe' */
static void swirl_set_lib_path_w32(SwirlState *s)
{
    char path[1024], *p;
    GetModuleFileNameA(swirl_module, path, sizeof path);
    p = swirl_basename(normalize_slashes(strlwr(path)));
    if (p > path)
        --p;
    *p = 0;
    swirl_set_lib_path(s, path);
}

#ifdef SWIRL_TARGET_PE
static void swirl_add_systemdir(SwirlState *s)
{
    char buf[1000];
    GetSystemDirectory(buf, sizeof buf);
    swirl_add_library_path(s, normalize_slashes(buf));
}
#endif

#ifdef LIBSWIRL_AS_DLL
BOOL WINAPI DllMain (HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved)
{
    if (DLL_PROCESS_ATTACH == dwReason)
        swirl_module = hDll;
    return TRUE;
}
#endif
#endif

/********************************************************/
#if CONFIG_SWIRL_SEMLOCK == 0
#define WAIT_SEM()
#define POST_SEM()
#elif defined _WIN32
static int swirl_sem_init;
static CRITICAL_SECTION swirl_cr;
static void wait_sem(void)
{
    if (!swirl_sem_init)
        InitializeCriticalSection(&swirl_cr), swirl_sem_init = 1;
    EnterCriticalSection(&swirl_cr);
}
#define WAIT_SEM() wait_sem()
#define POST_SEM() LeaveCriticalSection(&swirl_cr);
#elif defined __APPLE__
/* Half-compatible MacOS doesn't have non-shared (process local)
   semaphores.  Use the dispatch framework for lightweight locks.  */
#include <dispatch/dispatch.h>
static int swirl_sem_init;
static dispatch_semaphore_t swirl_sem;
static void wait_sem(void)
{
    if (!swirl_sem_init)
      swirl_sem = dispatch_semaphore_create(1), swirl_sem_init = 1;
    dispatch_semaphore_wait(swirl_sem, DISPATCH_TIME_FOREVER);
}
#define WAIT_SEM() wait_sem()
#define POST_SEM() dispatch_semaphore_signal(swirl_sem)
#else
#include <semaphore.h>
static int swirl_sem_init;
static sem_t swirl_sem;
static void wait_sem(void)
{
    if (!swirl_sem_init)
        sem_init(&swirl_sem, 0, 1), swirl_sem_init = 1;
    while (sem_wait (&swirl_sem) < 0 && errno == EINTR);
}
#define WAIT_SEM() wait_sem()
#define POST_SEM() sem_post(&swirl_sem)
#endif

/********************************************************/
/* copy a string and truncate it. */
ST_FUNC char *pstrcpy(char *buf, size_t buf_size, const char *s)
{
    char *q, *q_end;
    int c;

    if (buf_size > 0) {
        q = buf;
        q_end = buf + buf_size - 1;
        while (q < q_end) {
            c = *s++;
            if (c == '\0')
                break;
            *q++ = c;
        }
        *q = '\0';
    }
    return buf;
}

/* strcat and truncate. */
ST_FUNC char *pstrcat(char *buf, size_t buf_size, const char *s)
{
    size_t len;
    len = strlen(buf);
    if (len < buf_size)
        pstrcpy(buf + len, buf_size - len, s);
    return buf;
}

ST_FUNC char *pstrncpy(char *out, const char *in, size_t num)
{
    memcpy(out, in, num);
    out[num] = '\0';
    return out;
}

/* extract the basename of a file */
PUB_FUNC char *swirl_basename(const char *name)
{
    char *p = strchr(name, 0);
    while (p > name && !IS_DIRSEP(p[-1]))
        --p;
    return p;
}

/* extract extension part of a file
 *
 * (if no extension, return pointer to end-of-string)
 */
PUB_FUNC char *swirl_fileextension (const char *name)
{
    char *b = swirl_basename(name);
    char *e = strrchr(b, '.');
    return e ? e : strchr(b, 0);
}

/********************************************************/
/* memory management */

#undef free
#undef malloc
#undef realloc

#ifndef MEM_DEBUG

PUB_FUNC void swirl_free(void *ptr)
{
    free(ptr);
}

PUB_FUNC void *swirl_malloc(unsigned long size)
{
    void *ptr;
    ptr = malloc(size);
    if (!ptr && size)
        _swirl_error("memory full (malloc)");
    return ptr;
}

PUB_FUNC void *swirl_mallocz(unsigned long size)
{
    void *ptr;
    ptr = swirl_malloc(size);
    if (size)
        memset(ptr, 0, size);
    return ptr;
}

PUB_FUNC void *swirl_realloc(void *ptr, unsigned long size)
{
    void *ptr1;
    ptr1 = realloc(ptr, size);
    if (!ptr1 && size)
        _swirl_error("memory full (realloc)");
    return ptr1;
}

PUB_FUNC char *swirl_strdup(const char *str)
{
    char *ptr;
    ptr = swirl_malloc(strlen(str) + 1);
    strcpy(ptr, str);
    return ptr;
}

#else

#define MEM_DEBUG_MAGIC1 0xFEEDDEB1
#define MEM_DEBUG_MAGIC2 0xFEEDDEB2
#define MEM_DEBUG_MAGIC3 0xFEEDDEB3
#define MEM_DEBUG_FILE_LEN 40
#define MEM_DEBUG_CHECK3(header) \
    ((mem_debug_header_t*)((char*)header + header->size))->magic3
#define MEM_USER_PTR(header) \
    ((char *)header + offsetof(mem_debug_header_t, magic3))
#define MEM_HEADER_PTR(ptr) \
    (mem_debug_header_t *)((char*)ptr - offsetof(mem_debug_header_t, magic3))

struct mem_debug_header {
    unsigned magic1;
    unsigned size;
    struct mem_debug_header *prev;
    struct mem_debug_header *next;
    int line_num;
    char file_name[MEM_DEBUG_FILE_LEN + 1];
    unsigned magic2;
    ALIGNED(16) unsigned magic3;
};

typedef struct mem_debug_header mem_debug_header_t;

static mem_debug_header_t *mem_debug_chain;
static unsigned mem_cur_size;
static unsigned mem_max_size;

static mem_debug_header_t *malloc_check(void *ptr, const char *msg)
{
    mem_debug_header_t * header = MEM_HEADER_PTR(ptr);
    if (header->magic1 != MEM_DEBUG_MAGIC1 ||
        header->magic2 != MEM_DEBUG_MAGIC2 ||
        MEM_DEBUG_CHECK3(header) != MEM_DEBUG_MAGIC3 ||
        header->size == (unsigned)-1) {
        fprintf(stderr, "%s check failed\n", msg);
        if (header->magic1 == MEM_DEBUG_MAGIC1)
            fprintf(stderr, "%s:%u: block allocated here.\n",
                header->file_name, header->line_num);
        exit(1);
    }
    return header;
}

PUB_FUNC void *swirl_malloc_debug(unsigned long size, const char *file, int line)
{
    int ofs;
    mem_debug_header_t *header;

    header = malloc(sizeof(mem_debug_header_t) + size);
    if (!header)
        _swirl_error("memory full (malloc)");

    header->magic1 = MEM_DEBUG_MAGIC1;
    header->magic2 = MEM_DEBUG_MAGIC2;
    header->size = size;
    MEM_DEBUG_CHECK3(header) = MEM_DEBUG_MAGIC3;
    header->line_num = line;
    ofs = strlen(file) - MEM_DEBUG_FILE_LEN;
    strncpy(header->file_name, file + (ofs > 0 ? ofs : 0), MEM_DEBUG_FILE_LEN);
    header->file_name[MEM_DEBUG_FILE_LEN] = 0;

    header->next = mem_debug_chain;
    header->prev = NULL;
    if (header->next)
        header->next->prev = header;
    mem_debug_chain = header;

    mem_cur_size += size;
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;

    return MEM_USER_PTR(header);
}

PUB_FUNC void swirl_free_debug(void *ptr)
{
    mem_debug_header_t *header;
    if (!ptr)
        return;
    header = malloc_check(ptr, "swirl_free");
    mem_cur_size -= header->size;
    header->size = (unsigned)-1;
    if (header->next)
        header->next->prev = header->prev;
    if (header->prev)
        header->prev->next = header->next;
    if (header == mem_debug_chain)
        mem_debug_chain = header->next;
    free(header);
}

PUB_FUNC void *swirl_mallocz_debug(unsigned long size, const char *file, int line)
{
    void *ptr;
    ptr = swirl_malloc_debug(size,file,line);
    memset(ptr, 0, size);
    return ptr;
}

PUB_FUNC void *swirl_realloc_debug(void *ptr, unsigned long size, const char *file, int line)
{
    mem_debug_header_t *header;
    int mem_debug_chain_update = 0;
    if (!ptr)
        return swirl_malloc_debug(size, file, line);
    header = malloc_check(ptr, "swirl_realloc");
    mem_cur_size -= header->size;
    mem_debug_chain_update = (header == mem_debug_chain);
    header = realloc(header, sizeof(mem_debug_header_t) + size);
    if (!header)
        _swirl_error("memory full (realloc)");
    header->size = size;
    MEM_DEBUG_CHECK3(header) = MEM_DEBUG_MAGIC3;
    if (header->next)
        header->next->prev = header;
    if (header->prev)
        header->prev->next = header;
    if (mem_debug_chain_update)
        mem_debug_chain = header;
    mem_cur_size += size;
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;
    return MEM_USER_PTR(header);
}

PUB_FUNC char *swirl_strdup_debug(const char *str, const char *file, int line)
{
    char *ptr;
    ptr = swirl_malloc_debug(strlen(str) + 1, file, line);
    strcpy(ptr, str);
    return ptr;
}

PUB_FUNC void swirl_memcheck(void)
{
    if (mem_cur_size) {
        mem_debug_header_t *header = mem_debug_chain;
        fprintf(stderr, "MEM_DEBUG: mem_leak= %d bytes, mem_max_size= %d bytes\n",
            mem_cur_size, mem_max_size);
        while (header) {
            fprintf(stderr, "%s:%u: error: %u bytes leaked\n",
                header->file_name, header->line_num, header->size);
            header = header->next;
        }
#if MEM_DEBUG-0 == 2
        exit(2);
#endif
    }
}
#endif /* MEM_DEBUG */

#define free(p) use_swirl_free(p)
#define malloc(s) use_swirl_malloc(s)
#define realloc(p, s) use_swirl_realloc(p, s)

/********************************************************/
/* dynarrays */

ST_FUNC void dynarray_add(void *ptab, int *nb_ptr, void *data)
{
    int nb, nb_alloc;
    void **pp;

    nb = *nb_ptr;
    pp = *(void ***)ptab;
    /* every power of two we double array size */
    if ((nb & (nb - 1)) == 0) {
        if (!nb)
            nb_alloc = 1;
        else
            nb_alloc = nb * 2;
        pp = swirl_realloc(pp, nb_alloc * sizeof(void *));
        *(void***)ptab = pp;
    }
    pp[nb++] = data;
    *nb_ptr = nb;
}

ST_FUNC void dynarray_reset(void *pp, int *n)
{
    void **p;
    for (p = *(void***)pp; *n; ++p, --*n)
        if (*p)
            swirl_free(*p);
    swirl_free(*(void**)pp);
    *(void**)pp = NULL;
}

static void swirl_split_path(SwirlState *s, void *p_ary, int *p_nb_ary, const char *in)
{
    const char *p;
    do {
        int c;
        CString str;

        cstr_new(&str);
        for (p = in; c = *p, c != '\0' && c != PATHSEP[0]; ++p) {
            if (c == '{' && p[1] && p[2] == '}') {
                c = p[1], p += 2;
                if (c == 'B')
                    cstr_cat(&str, s->swirl_lib_path, -1);
                if (c == 'f' && file) {
                    /* substitute current file's dir */
                    const char *f = file->true_filename;
                    const char *b = swirl_basename(f);
                    if (b > f)
                        cstr_cat(&str, f, b - f - 1);
                    else
                        cstr_cat(&str, ".", 1);
                }
            } else {
                cstr_ccat(&str, c);
            }
        }
        if (str.size) {
            cstr_ccat(&str, '\0');
            dynarray_add(p_ary, p_nb_ary, swirl_strdup(str.data));
        }
        cstr_free(&str);
        in = p+1;
    } while (*p);
}

/********************************************************/

static void strcat_vprintf(char *buf, int buf_size, const char *fmt, va_list ap)
{
    int len;
    len = strlen(buf);
    vsnprintf(buf + len, buf_size - len, fmt, ap);
}

static void strcat_printf(char *buf, int buf_size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    strcat_vprintf(buf, buf_size, fmt, ap);
    va_end(ap);
}

#define ERROR_WARN 0
#define ERROR_NOABORT 1
#define ERROR_ERROR 2

PUB_FUNC void swirl_enter_state(SwirlState *s1)
{
    WAIT_SEM();
    swirl_state = s1;
}

PUB_FUNC void swirl_exit_state(void)
{
    swirl_state = NULL;
    POST_SEM();
}

static void error1(int mode, const char *fmt, va_list ap)
{
    char buf[2048];
    BufferedFile **pf, *f;
    SwirlState *s1 = swirl_state;

    buf[0] = '\0';
    if (s1 == NULL)
        /* can happen only if called from swirl_malloc(): 'out of memory' */
        goto no_file;

    if (s1 && !s1->error_set_jmp_enabled)
        /* swirl_state just was set by swirl_enter_state() */
        swirl_exit_state();

    if (mode == ERROR_WARN) {
        if (s1->warn_none)
            return;
        if (s1->warn_error)
            mode = ERROR_ERROR;
    }

    f = NULL;
    if (s1->error_set_jmp_enabled) { /* we're called while parsing a file */
        /* use upper file if inline ":asm:" or token ":paste:" */
        for (f = file; f && f->filename[0] == ':'; f = f->prev)
            ;
    }
    if (f) {
        for(pf = s1->include_stack; pf < s1->include_stack_ptr; pf++)
            strcat_printf(buf, sizeof(buf), "In file included from %s:%d:\n",
                (*pf)->filename, (*pf)->line_num);
        strcat_printf(buf, sizeof(buf), "%s:%d: ",
            f->filename, f->line_num - !!(tok_flags & TOK_FLAG_BOL));
    } else if (s1->current_filename) {
        strcat_printf(buf, sizeof(buf), "%s: ", s1->current_filename);
    }

no_file:
    if (0 == buf[0])
        strcat_printf(buf, sizeof(buf), "swirl: ");
    if (mode == ERROR_WARN)
        strcat_printf(buf, sizeof(buf), "warning: ");
    else
        strcat_printf(buf, sizeof(buf), "error: ");
    strcat_vprintf(buf, sizeof(buf), fmt, ap);
    if (!s1 || !s1->error_func) {
        /* default case: stderr */
        if (s1 && s1->output_type == SWIRL_OUTPUT_PREPROCESS && s1->ppfp == stdout)
            /* print a newline during swirl -E */
            printf("\n"), fflush(stdout);
        fflush(stdout); /* flush -v output */
        fprintf(stderr, "%s\n", buf);
        fflush(stderr); /* print error/warning now (win32) */
    } else {
        s1->error_func(s1->error_opaque, buf);
    }
    if (s1) {
        if (mode != ERROR_WARN)
            s1->nb_errors++;
        if (mode != ERROR_ERROR)
            return;
        if (s1->error_set_jmp_enabled)
            longjmp(s1->error_jmp_buf, 1);
    }
    exit(1);
}

LIBSWIRLAPI void swirl_set_error_func(SwirlState *s, void *error_opaque, SWIRLErrorFunc error_func)
{
    s->error_opaque = error_opaque;
    s->error_func = error_func;
}

LIBSWIRLAPI SWIRLErrorFunc swirl_get_error_func(SwirlState *s)
{
    return s->error_func;
}

LIBSWIRLAPI void *swirl_get_error_opaque(SwirlState *s)
{
    return s->error_opaque;
}

/* error without aborting current compilation */
PUB_FUNC void _swirl_error_noabort(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    error1(ERROR_NOABORT, fmt, ap);
    va_end(ap);
}

PUB_FUNC void _swirl_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    for (;;) error1(ERROR_ERROR, fmt, ap);
}

PUB_FUNC void _swirl_warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    error1(ERROR_WARN, fmt, ap);
    va_end(ap);
}

/********************************************************/
/* I/O layer */

ST_FUNC void swirl_open_bf(SwirlState *s1, const char *filename, int initlen)
{
    BufferedFile *bf;
    int buflen = initlen ? initlen : IO_BUF_SIZE;

    bf = swirl_mallocz(sizeof(BufferedFile) + buflen);
    bf->buf_ptr = bf->buffer;
    bf->buf_end = bf->buffer + initlen;
    bf->buf_end[0] = CH_EOB; /* put eob symbol */
    pstrcpy(bf->filename, sizeof(bf->filename), filename);
#ifdef _WIN32
    normalize_slashes(bf->filename);
#endif
    bf->true_filename = bf->filename;
    bf->line_num = 1;
    bf->ifdef_stack_ptr = s1->ifdef_stack_ptr;
    bf->fd = -1;
    bf->prev = file;
    file = bf;
    tok_flags = TOK_FLAG_BOL | TOK_FLAG_BOF;
}

ST_FUNC void swirl_close(void)
{
    SwirlState *s1 = swirl_state;
    BufferedFile *bf = file;
    if (bf->fd > 0) {
        close(bf->fd);
        total_lines += bf->line_num;
    }
    if (bf->true_filename != bf->filename)
        swirl_free(bf->true_filename);
    file = bf->prev;
    swirl_free(bf);
}

static int _swirl_open(SwirlState *s1, const char *filename)
{
    int fd;
    if (strcmp(filename, "-") == 0)
        fd = 0, filename = "<stdin>";
    else
        fd = open(filename, O_RDONLY | O_BINARY);
    if ((s1->verbose == 2 && fd >= 0) || s1->verbose == 3)
        printf("%s %*s%s\n", fd < 0 ? "nf":"->",
               (int)(s1->include_stack_ptr - s1->include_stack), "", filename);
    return fd;
}

ST_FUNC int swirl_open(SwirlState *s1, const char *filename)
{
    int fd = _swirl_open(s1, filename);
    if (fd < 0)
        return -1;
    swirl_open_bf(s1, filename, 0);
    file->fd = fd;
    return 0;
}

/* compile the file opened in 'file'. Return non zero if errors. */
static int swirl_compile(SwirlState *s1, int filetype, const char *str, int fd)
{
    /* Here we enter the code section where we use the global variables for
       parsing and code generation (swirlpp.c, swirlgen.c, <target>-gen.c).
       Other threads need to wait until we're done.

       Alternatively we could use thread local storage for those global
       variables, which may or may not have advantages */

    swirl_enter_state(s1);

    if (setjmp(s1->error_jmp_buf) == 0) {
        s1->error_set_jmp_enabled = 1;
        s1->nb_errors = 0;

        if (fd == -1) {
            int len = strlen(str);
            swirl_open_bf(s1, "<string>", len);
            memcpy(file->buffer, str, len);
        } else {
            swirl_open_bf(s1, str, 0);
            file->fd = fd;
        }

        swirlelf_begin_file(s1);
        preprocess_start(s1, filetype);
        swirlgen_init(s1);
        if (s1->output_type == SWIRL_OUTPUT_PREPROCESS) {
            swirl_preprocess(s1);
        } else if (filetype & (AFF_TYPE_ASM | AFF_TYPE_ASMPP)) {
#ifdef CONFIG_SWIRL_ASM
            swirl_assemble(s1, !!(filetype & AFF_TYPE_ASMPP));
#else
            swirl_error_noabort("asm not supported");
#endif
        } else {
            swirlgen_compile(s1);
        }
    }
    s1->error_set_jmp_enabled = 0;
    swirlgen_finish(s1);
    preprocess_end(s1);
    swirl_exit_state();

    swirlelf_end_file(s1);
    return s1->nb_errors != 0 ? -1 : 0;
}

LIBSWIRLAPI int swirl_compile_string(SwirlState *s, const char *str)
{
    return swirl_compile(s, s->filetype, str, -1);
}

/* define a preprocessor symbol. value can be NULL, sym can be "sym=val" */
LIBSWIRLAPI void swirl_define_symbol(SwirlState *s1, const char *sym, const char *value)
{
    const char *eq;
    if (NULL == (eq = strchr(sym, '=')))
        eq = strchr(sym, 0);
    if (NULL == value)
        value = *eq ? eq + 1 : "1";
    cstr_printf(&s1->cmdline_defs, "#define %.*s %s\n", (int)(eq-sym), sym, value);
}

/* undefine a preprocessor symbol */
LIBSWIRLAPI void swirl_undefine_symbol(SwirlState *s1, const char *sym)
{
    cstr_printf(&s1->cmdline_defs, "#undef %s\n", sym);
}


LIBSWIRLAPI SwirlState *swirl_new(void)
{
    SwirlState *s;

    s = swirl_mallocz(sizeof(SwirlState));
    if (!s)
        return NULL;
#ifdef MEM_DEBUG
    ++nb_states;
#endif

#undef gnu_ext

    s->gnu_ext = 1;
    s->swirl_ext = 1;
    s->nocommon = 1;
    s->dollars_in_identifiers = 1; /*on by default like in gcc/clang*/
    s->cversion = 199901; /* default unless -std=c11 is supplied */
    s->warn_implicit_function_declaration = 1;
    s->ms_extensions = 1;

#ifdef CHAR_IS_UNSIGNED
    s->char_is_unsigned = 1;
#endif
#ifdef SWIRL_TARGET_I386
    s->seg_size = 32;
#endif
    /* enable this if you want symbols with leading underscore on windows: */
#if defined SWIRL_TARGET_MACHO /* || defined SWIRL_TARGET_PE */
    s->leading_underscore = 1;
#endif
#ifdef SWIRL_TARGET_ARM
    s->float_abi = ARM_FLOAT_ABI;
#endif

    s->ppfp = stdout;
    /* might be used in error() before preprocess_start() */
    s->include_stack_ptr = s->include_stack;

    swirlelf_new(s);

#ifdef _WIN32
    swirl_set_lib_path_w32(s);
#else
    swirl_set_lib_path(s, CONFIG_SWIRLDIR);
#endif
    return s;
}

LIBSWIRLAPI void swirl_delete(SwirlState *s1)
{
    /* free sections */
    swirlelf_delete(s1);

    /* free library paths */
    dynarray_reset(&s1->library_paths, &s1->nb_library_paths);
    dynarray_reset(&s1->crt_paths, &s1->nb_crt_paths);

    /* free include paths */
    dynarray_reset(&s1->include_paths, &s1->nb_include_paths);
    dynarray_reset(&s1->sysinclude_paths, &s1->nb_sysinclude_paths);

    swirl_free(s1->swirl_lib_path);
    swirl_free(s1->soname);
    swirl_free(s1->rpath);
    swirl_free(s1->init_symbol);
    swirl_free(s1->fini_symbol);
    swirl_free(s1->outfile);
    swirl_free(s1->deps_outfile);
    dynarray_reset(&s1->files, &s1->nb_files);
    dynarray_reset(&s1->target_deps, &s1->nb_target_deps);
    dynarray_reset(&s1->pragma_libs, &s1->nb_pragma_libs);
    dynarray_reset(&s1->argv, &s1->argc);
    cstr_free(&s1->cmdline_defs);
    cstr_free(&s1->cmdline_incl);
#ifdef SWIRL_IS_NATIVE
    /* free runtime memory */
    swirl_run_free(s1);
#endif

    swirl_free(s1);
#ifdef MEM_DEBUG
    if (0 == --nb_states)
        swirl_memcheck();
#endif
}

LIBSWIRLAPI int swirl_set_output_type(SwirlState *s, int output_type)
{
    s->output_type = output_type;

    /* always elf for objects */
    if (output_type == SWIRL_OUTPUT_OBJ)
        s->output_format = SWIRL_OUTPUT_FORMAT_ELF;

    if (!s->nostdinc) {
        /* default include paths */
        /* -isystem paths have already been handled */
        swirl_add_sysinclude_path(s, CONFIG_SWIRL_SYSINCLUDEPATHS);
    }

#ifdef CONFIG_SWIRL_BCHECK
    if (s->do_bounds_check) {
        /* if bound checking, then add corresponding sections */
        swirlelf_bounds_new(s);
    }
#endif
    if (s->do_debug) {
        /* add debug sections */
        swirlelf_stab_new(s);
    }

    swirl_add_library_path(s, CONFIG_SWIRL_LIBPATHS);

#ifdef SWIRL_TARGET_PE
# ifdef _WIN32
    if (!s->nostdlib && output_type != SWIRL_OUTPUT_OBJ)
        swirl_add_systemdir(s);
# endif
#else
    /* paths for crt objects */
    swirl_split_path(s, &s->crt_paths, &s->nb_crt_paths, CONFIG_SWIRL_CRTPREFIX);
    /* add libc crt1/crti objects */
    if ((output_type == SWIRL_OUTPUT_EXE || output_type == SWIRL_OUTPUT_DLL) &&
        !s->nostdlib) {
#if TARGETOS_OpenBSD || TARGETOS_FreeBSD || TARGETOS_NetBSD
#if TARGETOS_OpenBSD
        if (output_type != SWIRL_OUTPUT_DLL)
	    swirl_add_crt(s, "crt0.o");
        if (output_type == SWIRL_OUTPUT_DLL)
            swirl_add_crt(s, "crtbeginS.o");
        else
            swirl_add_crt(s, "crtbegin.o");
#elif TARGETOS_FreeBSD
        if (output_type != SWIRL_OUTPUT_DLL)
            swirl_add_crt(s, "crt1.o");
        swirl_add_crt(s, "crti.o");
        if (s->static_link)
            swirl_add_crt(s, "crtbeginT.o");
        else if (output_type == SWIRL_OUTPUT_DLL)
            swirl_add_crt(s, "crtbeginS.o");
        else
            swirl_add_crt(s, "crtbegin.o");
#elif TARGETOS_NetBSD
        if (output_type != SWIRL_OUTPUT_DLL)
            swirl_add_crt(s, "crt0.o");
        swirl_add_crt(s, "crti.o");
        if (s->static_link)
            swirl_add_crt(s, "crtbeginT.o");
        else if (output_type == SWIRL_OUTPUT_DLL)
            swirl_add_crt(s, "crtbeginS.o");
        else
            swirl_add_crt(s, "crtbegin.o");
#endif
#elif !SWIRL_TARGET_MACHO
        /* Mach-O with LC_MAIN doesn't need any crt startup code.  */
        if (output_type != SWIRL_OUTPUT_DLL)
            swirl_add_crt(s, "crt1.o");
        swirl_add_crt(s, "crti.o");
#endif
    }
#endif
    return 0;
}

LIBSWIRLAPI int swirl_add_include_path(SwirlState *s, const char *pathname)
{
    swirl_split_path(s, &s->include_paths, &s->nb_include_paths, pathname);
    return 0;
}

LIBSWIRLAPI int swirl_add_sysinclude_path(SwirlState *s, const char *pathname)
{
    swirl_split_path(s, &s->sysinclude_paths, &s->nb_sysinclude_paths, pathname);
    return 0;
}

#if !defined SWIRL_TARGET_MACHO || defined SWIRL_IS_NATIVE
ST_FUNC DLLReference *swirl_add_dllref(SwirlState *s1, const char *dllname)
{
    DLLReference *ref = swirl_mallocz(sizeof(DLLReference) + strlen(dllname));
    strcpy(ref->name, dllname);
    dynarray_add(&s1->loaded_dlls, &s1->nb_loaded_dlls, ref);
    return ref;
}
#endif

ST_FUNC int swirl_add_file_internal(SwirlState *s1, const char *filename, int flags)
{
    int fd, ret = -1;

    /* open the file */
    fd = _swirl_open(s1, filename);
    if (fd < 0) {
        if (flags & AFF_PRINT_ERROR)
            swirl_error_noabort("file '%s' not found", filename);
        return ret;
    }

    s1->current_filename = filename;
    if (flags & AFF_TYPE_BIN) {
        ElfW(Ehdr) ehdr;
        int obj_type;

        obj_type = swirl_object_type(fd, &ehdr);
        lseek(fd, 0, SEEK_SET);

#ifdef SWIRL_TARGET_MACHO
        if (0 == obj_type && 0 == strcmp(swirl_fileextension(filename), ".dylib"))
            obj_type = AFF_BINTYPE_DYN;
#endif

        switch (obj_type) {

        case AFF_BINTYPE_REL:
            ret = swirl_load_object_file(s1, fd, 0);
            break;

        case AFF_BINTYPE_AR:
            ret = swirl_load_archive(s1, fd, !(flags & AFF_WHOLE_ARCHIVE));
            break;

#ifdef SWIRL_TARGET_PE
        default:
            ret = pe_load_file(s1, fd, filename);
#else
        case AFF_BINTYPE_DYN:
            if (s1->output_type == SWIRL_OUTPUT_MEMORY) {
#ifdef SWIRL_IS_NATIVE
                void *dl = dlopen(filename, RTLD_GLOBAL | RTLD_LAZY);
                if (dl) {
                    swirl_add_dllref(s1, filename)->handle = dl;
                    ret = 0;
                }
#endif
                break;
            }
#ifdef SWIRL_TARGET_MACHO
            ret = macho_load_dll(s1, fd, filename,
                                 (flags & AFF_REFERENCED_DLL) != 0);
#else
            ret = swirl_load_dll(s1, fd, filename,
                               (flags & AFF_REFERENCED_DLL) != 0);
#endif
            break;

#ifdef SWIRL_TARGET_COFF
        case AFF_BINTYPE_C67:
            ret = swirl_load_coff(s1, fd);
            break;
#endif
        default:
#ifndef SWIRL_TARGET_MACHO
            /* as GNU ld, consider it is an ld script if not recognized */
            ret = swirl_load_ldscript(s1, fd);
#endif

#endif /* !SWIRL_TARGET_PE */
            if (ret < 0)
                swirl_error_noabort("%s: unrecognized file type", filename);
            break;
        }
        close(fd);
    } else {
        /* update target deps */
        dynarray_add(&s1->target_deps, &s1->nb_target_deps, swirl_strdup(filename));
        ret = swirl_compile(s1, flags, filename, fd);
    }
    s1->current_filename = NULL;
    return ret;
}

LIBSWIRLAPI int swirl_add_file(SwirlState *s, const char *filename)
{
    int filetype = s->filetype;
    if (0 == (filetype & AFF_TYPE_MASK)) {
        /* use a file extension to detect a filetype */
        const char *ext = swirl_fileextension(filename);
        if (ext[0]) {
            ext++;
            if (!strcmp(ext, "S"))
                filetype = AFF_TYPE_ASMPP;
            else if (!strcmp(ext, "s"))
                filetype = AFF_TYPE_ASM;
            else if (!PATHCMP(ext, "c") || !PATHCMP(ext, "i"))
                filetype = AFF_TYPE_C;
            else
                filetype |= AFF_TYPE_BIN;
        } else {
            filetype = AFF_TYPE_C;
        }
    }
    return swirl_add_file_internal(s, filename, filetype | AFF_PRINT_ERROR);
}

LIBSWIRLAPI int swirl_add_library_path(SwirlState *s, const char *pathname)
{
    swirl_split_path(s, &s->library_paths, &s->nb_library_paths, pathname);
    return 0;
}

static int swirl_add_library_internal(SwirlState *s, const char *fmt,
    const char *filename, int flags, char **paths, int nb_paths)
{
    char buf[1024];
    int i;

    for(i = 0; i < nb_paths; i++) {
        snprintf(buf, sizeof(buf), fmt, paths[i], filename);
        if (swirl_add_file_internal(s, buf, flags | AFF_TYPE_BIN) == 0)
            return 0;
    }
    return -1;
}

#ifndef SWIRL_TARGET_MACHO
/* find and load a dll. Return non zero if not found */
/* XXX: add '-rpath' option support ? */
ST_FUNC int swirl_add_dll(SwirlState *s, const char *filename, int flags)
{
    return swirl_add_library_internal(s, "%s/%s", filename, flags,
        s->library_paths, s->nb_library_paths);
}
#endif

#if !defined SWIRL_TARGET_PE && !defined SWIRL_TARGET_MACHO
ST_FUNC int swirl_add_crt(SwirlState *s1, const char *filename)
{
    if (-1 == swirl_add_library_internal(s1, "%s/%s",
        filename, 0, s1->crt_paths, s1->nb_crt_paths))
        swirl_error_noabort("file '%s' not found", filename);
    return 0;
}
#endif

/* OpenBSD only has suffixed .so files; e.g., libc.so.96.0 */
/* So we must process that */
#if defined TARGETOS_OpenBSD && !defined _WIN32/* no dirent */
#include <dirent.h>
ST_FUNC char *swirl_openbsd_library_soversion(SwirlState *s, const char *libraryname)
{
    DIR *dirp;
    struct dirent *dp;
    char *e;
    char **libpaths, *t, *u, *v;
    char soname[1024];
    long long maj, min, tmaj, tmin;
    int i;
    static char soversion[1024];

    snprintf(soname, sizeof(soname), "lib%s.so", libraryname);

    libpaths = s->library_paths;
    for (i = 0; i < s->nb_library_paths; ++i) {
        if ((dirp = opendir(libpaths[i])) == NULL)
            continue;

        maj = -1;
        min = -1;

        while ((dp = readdir(dirp)) != NULL) {
            if (!strncmp(dp->d_name, soname, strlen(soname))) {
                t = swirl_strdup(dp->d_name);
                u = strrchr(t, '.');
                *u = '\0';

                tmin = strtoll(u + 1, &e, 10);

                if (*e != 0) {
                    swirl_free(t);
                    t = NULL;
                    continue;
                }

                v = strrchr(t, '.');
                tmaj = strtoll(v + 1, &e, 10);

                if (*e != 0) {
                    swirl_free(t);
                    t = NULL;
                    continue;
                }

		swirl_free(t);
                t = NULL;

                if (maj == tmaj) {
                    if (min < tmin)
                        min = tmin;
                } else if (maj < tmaj) {
                    maj = tmaj;
                    min = tmin;
                }
            }
        }
	closedir(dirp);

        if (maj == -1 || min == -1)
            continue;

	snprintf(soversion, sizeof(soversion), "%s/%s.%lld.%lld", libpaths[i],
		 soname, maj, min);
    }

    return soversion;
}
#endif

/* the library name is the same as the argument of the '-l' option */
LIBSWIRLAPI int swirl_add_library(SwirlState *s, const char *libraryname)
{
#if defined SWIRL_TARGET_PE
    const char *libs[] = { "%s/%s.def", "%s/lib%s.def", "%s/%s.dll", "%s/lib%s.dll", "%s/lib%s.a", NULL };
    const char **pp = s->static_link ? libs + 4 : libs;
#elif defined SWIRL_TARGET_MACHO
    const char *libs[] = { "%s/lib%s.dylib", "%s/lib%s.a", NULL };
    const char **pp = s->static_link ? libs + 1 : libs;
#elif defined TARGETOS_OpenBSD && !defined _WIN32
    const char *libs[] = { s->static_link
                           ? NULL
                           /* find exact versionned .so.x.y name as no
                              symlink exists on OpenBSD. */
                           : swirl_openbsd_library_soversion(s, libraryname),
                           "%s/lib%s.a",
			   NULL
    };
    const char **pp = s->static_link ? libs + 1 : libs;
#else
    const char *libs[] = { "%s/lib%s.so", "%s/lib%s.a", NULL };
    const char **pp = s->static_link ? libs + 1 : libs;
#endif
    int flags = s->filetype & AFF_WHOLE_ARCHIVE;
    while (*pp) {
        if (0 == swirl_add_library_internal(s, *pp,
            libraryname, flags, s->library_paths, s->nb_library_paths))
            return 0;
        ++pp;
    }
    return -1;
}

PUB_FUNC int swirl_add_library_err(SwirlState *s1, const char *libname)
{
    int ret = swirl_add_library(s1, libname);
    if (ret < 0)
        swirl_error_noabort("library '%s' not found", libname);
    return ret;
}

/* handle #pragma comment(lib,) */
ST_FUNC void swirl_add_pragma_libs(SwirlState *s1)
{
    int i;
    for (i = 0; i < s1->nb_pragma_libs; i++)
        swirl_add_library_err(s1, s1->pragma_libs[i]);
}

LIBSWIRLAPI int swirl_add_symbol(SwirlState *s1, const char *name, const void *val)
{
#ifdef SWIRL_TARGET_PE
    /* On x86_64 'val' might not be reachable with a 32bit offset.
       So it is handled here as if it were in a DLL. */
    pe_putimport(s1, 0, name, (uintptr_t)val);
#else
    char buf[256];
    if (s1->leading_underscore) {
        buf[0] = '_';
        pstrcpy(buf + 1, sizeof(buf) - 1, name);
        name = buf;
    }
    set_global_sym(s1, name, NULL, (addr_t)(uintptr_t)val); /* NULL: SHN_ABS */
#endif
    return 0;
}

LIBSWIRLAPI void swirl_set_lib_path(SwirlState *s, const char *path)
{
    swirl_free(s->swirl_lib_path);
    s->swirl_lib_path = swirl_strdup(path);
}

#define WD_ALL    0x0001 /* warning is activated when using -Wall */
#define FD_INVERT 0x0002 /* invert value before storing */

typedef struct FlagDef {
    uint16_t offset;
    uint16_t flags;
    const char *name;
} FlagDef;

static int no_flag(const char **pp)
{
    const char *p = *pp;
    if (*p != 'n' || *++p != 'o' || *++p != '-')
        return 0;
    *pp = p + 1;
    return 1;
}

ST_FUNC int set_flag(SwirlState *s, const FlagDef *flags, const char *name)
{
    int value, ret;
    const FlagDef *p;
    const char *r;

    value = 1;
    r = name;
    if (no_flag(&r))
        value = 0;

    for (ret = -1, p = flags; p->name; ++p) {
        if (ret) {
            if (strcmp(r, p->name))
                continue;
        } else {
            if (0 == (p->flags & WD_ALL))
                continue;
        }
        if (p->offset) {
            *((unsigned char *)s + p->offset) =
                p->flags & FD_INVERT ? !value : value;
            if (ret)
                return 0;
        } else {
            ret = 0;
        }
    }
    return ret;
}

static int strstart(const char *val, const char **str)
{
    const char *p, *q;
    p = *str;
    q = val;
    while (*q) {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }
    *str = p;
    return 1;
}

/* Like strstart, but automatically takes into account that ld options can
 *
 * - start with double or single dash (e.g. '--soname' or '-soname')
 * - arguments can be given as separate or after '=' (e.g. '-Wl,-soname,x.so'
 *   or '-Wl,-soname=x.so')
 *
 * you provide `val` always in 'option[=]' form (no leading -)
 */
static int link_option(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    int ret;

    /* there should be 1 or 2 dashes */
    if (*str++ != '-')
        return 0;
    if (*str == '-')
        str++;

    /* then str & val should match (potentially up to '=') */
    p = str;
    q = val;

    ret = 1;
    if (q[0] == '?') {
        ++q;
        if (no_flag(&p))
            ret = -1;
    }

    while (*q != '\0' && *q != '=') {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }

    /* '=' near eos means ',' or '=' is ok */
    if (*q == '=') {
        if (*p == 0)
            *ptr = p;
        if (*p != ',' && *p != '=')
            return 0;
        p++;
    } else if (*p) {
        return 0;
    }
    *ptr = p;
    return ret;
}

static const char *skip_linker_arg(const char **str)
{
    const char *s1 = *str;
    const char *s2 = strchr(s1, ',');
    *str = s2 ? s2++ : (s2 = s1 + strlen(s1));
    return s2;
}

static void copy_linker_arg(char **pp, const char *s, int sep)
{
    const char *q = s;
    char *p = *pp;
    int l = 0;
    if (p && sep)
        p[l = strlen(p)] = sep, ++l;
    skip_linker_arg(&q);
    pstrncpy(l + (*pp = swirl_realloc(p, q - s + l + 1)), s, q - s);
}

/* set linker options */
static int swirl_set_linker(SwirlState *s, const char *option)
{
    SwirlState *s1 = s;
    while (*option) {

        const char *p = NULL;
        char *end = NULL;
        int ignoring = 0;
        int ret;

        if (link_option(option, "Bsymbolic", &p)) {
            s->symbolic = 1;
        } else if (link_option(option, "nostdlib", &p)) {
            s->nostdlib = 1;
        } else if (link_option(option, "fini=", &p)) {
            copy_linker_arg(&s->fini_symbol, p, 0);
            ignoring = 1;
        } else if (link_option(option, "image-base=", &p)
                || link_option(option, "Ttext=", &p)) {
            s->text_addr = strtoull(p, &end, 16);
            s->has_text_addr = 1;
        } else if (link_option(option, "init=", &p)) {
            copy_linker_arg(&s->init_symbol, p, 0);
            ignoring = 1;
        } else if (link_option(option, "oformat=", &p)) {
#if defined(SWIRL_TARGET_PE)
            if (strstart("pe-", &p)) {
#elif PTR_SIZE == 8
            if (strstart("elf64-", &p)) {
#else
            if (strstart("elf32-", &p)) {
#endif
                s->output_format = SWIRL_OUTPUT_FORMAT_ELF;
            } else if (!strcmp(p, "binary")) {
                s->output_format = SWIRL_OUTPUT_FORMAT_BINARY;
#ifdef SWIRL_TARGET_COFF
            } else if (!strcmp(p, "coff")) {
                s->output_format = SWIRL_OUTPUT_FORMAT_COFF;
#endif
            } else
                goto err;

        } else if (link_option(option, "as-needed", &p)) {
            ignoring = 1;
        } else if (link_option(option, "O", &p)) {
            ignoring = 1;
        } else if (link_option(option, "export-all-symbols", &p)) {
            s->rdynamic = 1;
        } else if (link_option(option, "export-dynamic", &p)) {
            s->rdynamic = 1;
        } else if (link_option(option, "rpath=", &p)) {
            copy_linker_arg(&s->rpath, p, ':');
        } else if (link_option(option, "enable-new-dtags", &p)) {
            s->enable_new_dtags = 1;
        } else if (link_option(option, "section-alignment=", &p)) {
            s->section_align = strtoul(p, &end, 16);
        } else if (link_option(option, "soname=", &p)) {
            copy_linker_arg(&s->soname, p, 0);
#ifdef SWIRL_TARGET_PE
        } else if (link_option(option, "large-address-aware", &p)) {
            s->pe_characteristics |= 0x20;
        } else if (link_option(option, "file-alignment=", &p)) {
            s->pe_file_align = strtoul(p, &end, 16);
        } else if (link_option(option, "stack=", &p)) {
            s->pe_stack_size = strtoul(p, &end, 10);
        } else if (link_option(option, "subsystem=", &p)) {
#if defined(SWIRL_TARGET_I386) || defined(SWIRL_TARGET_X86_64)
            if (!strcmp(p, "native")) {
                s->pe_subsystem = 1;
            } else if (!strcmp(p, "console")) {
                s->pe_subsystem = 3;
            } else if (!strcmp(p, "gui") || !strcmp(p, "windows")) {
                s->pe_subsystem = 2;
            } else if (!strcmp(p, "posix")) {
                s->pe_subsystem = 7;
            } else if (!strcmp(p, "efiapp")) {
                s->pe_subsystem = 10;
            } else if (!strcmp(p, "efiboot")) {
                s->pe_subsystem = 11;
            } else if (!strcmp(p, "efiruntime")) {
                s->pe_subsystem = 12;
            } else if (!strcmp(p, "efirom")) {
                s->pe_subsystem = 13;
#elif defined(SWIRL_TARGET_ARM)
            if (!strcmp(p, "wince")) {
                s->pe_subsystem = 9;
#endif
            } else
                goto err;
#endif
        } else if (ret = link_option(option, "?whole-archive", &p), ret) {
            if (ret > 0)
                s->filetype |= AFF_WHOLE_ARCHIVE;
            else
                s->filetype &= ~AFF_WHOLE_ARCHIVE;
        } else if (link_option(option, "z=", &p)) {
            ignoring = 1;
        } else if (p) {
            return 0;
        } else {
    err:
            swirl_error("unsupported linker option '%s'", option);
        }

        if (ignoring && s->warn_unsupported)
            swirl_warning("unsupported linker option '%s'", option);

        option = skip_linker_arg(&p);
    }
    return 1;
}

typedef struct SWIRLOption {
    const char *name;
    uint16_t index;
    uint16_t flags;
} SWIRLOption;

enum {
    SWIRL_OPTION_HELP,
    SWIRL_OPTION_HELP2,
    SWIRL_OPTION_v,
    SWIRL_OPTION_I,
    SWIRL_OPTION_D,
    SWIRL_OPTION_U,
    SWIRL_OPTION_P,
    SWIRL_OPTION_L,
    SWIRL_OPTION_B,
    SWIRL_OPTION_l,
    SWIRL_OPTION_bench,
    SWIRL_OPTION_bt,
    SWIRL_OPTION_b,
    SWIRL_OPTION_ba,
    SWIRL_OPTION_g,
    SWIRL_OPTION_c,
    SWIRL_OPTION_dumpversion,
    SWIRL_OPTION_d,
    SWIRL_OPTION_static,
    SWIRL_OPTION_std,
    SWIRL_OPTION_shared,
    SWIRL_OPTION_soname,
    SWIRL_OPTION_o,
    SWIRL_OPTION_r,
    SWIRL_OPTION_s,
    SWIRL_OPTION_traditional,
    SWIRL_OPTION_Wl,
    SWIRL_OPTION_Wp,
    SWIRL_OPTION_W,
    SWIRL_OPTION_O,
    SWIRL_OPTION_mfloat_abi,
    SWIRL_OPTION_m,
    SWIRL_OPTION_f,
    SWIRL_OPTION_isystem,
    SWIRL_OPTION_iwithprefix,
    SWIRL_OPTION_include,
    SWIRL_OPTION_nostdinc,
    SWIRL_OPTION_nostdlib,
    SWIRL_OPTION_print_search_dirs,
    SWIRL_OPTION_rdynamic,
    SWIRL_OPTION_param,
    SWIRL_OPTION_pedantic,
    SWIRL_OPTION_pthread,
    SWIRL_OPTION_run,
    SWIRL_OPTION_w,
    SWIRL_OPTION_pipe,
    SWIRL_OPTION_E,
    SWIRL_OPTION_MD,
    SWIRL_OPTION_MF,
    SWIRL_OPTION_x,
    SWIRL_OPTION_ar,
    SWIRL_OPTION_impdef,
    SWIRL_OPTION_C
};

#define SWIRL_OPTION_HAS_ARG 0x0001
#define SWIRL_OPTION_NOSEP   0x0002 /* cannot have space before option and arg */

static const SWIRLOption swirl_options[] = {
    { "h", SWIRL_OPTION_HELP, 0 },
    { "-help", SWIRL_OPTION_HELP, 0 },
    { "?", SWIRL_OPTION_HELP, 0 },
    { "hh", SWIRL_OPTION_HELP2, 0 },
    { "v", SWIRL_OPTION_v, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
    { "-version", SWIRL_OPTION_v, 0 }, /* handle as verbose, also prints version*/
    { "I", SWIRL_OPTION_I, SWIRL_OPTION_HAS_ARG },
    { "D", SWIRL_OPTION_D, SWIRL_OPTION_HAS_ARG },
    { "U", SWIRL_OPTION_U, SWIRL_OPTION_HAS_ARG },
    { "P", SWIRL_OPTION_P, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
    { "L", SWIRL_OPTION_L, SWIRL_OPTION_HAS_ARG },
    { "B", SWIRL_OPTION_B, SWIRL_OPTION_HAS_ARG },
    { "l", SWIRL_OPTION_l, SWIRL_OPTION_HAS_ARG },
    { "bench", SWIRL_OPTION_bench, 0 },
#ifdef CONFIG_SWIRL_BACKTRACE
    { "bt", SWIRL_OPTION_bt, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
#endif
#ifdef CONFIG_SWIRL_BCHECK
    { "b", SWIRL_OPTION_b, 0 },
#endif
    { "g", SWIRL_OPTION_g, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
    { "c", SWIRL_OPTION_c, 0 },
    { "dumpversion", SWIRL_OPTION_dumpversion, 0},
    { "d", SWIRL_OPTION_d, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
    { "static", SWIRL_OPTION_static, 0 },
    { "std", SWIRL_OPTION_std, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
    { "shared", SWIRL_OPTION_shared, 0 },
    { "soname", SWIRL_OPTION_soname, SWIRL_OPTION_HAS_ARG },
    { "o", SWIRL_OPTION_o, SWIRL_OPTION_HAS_ARG },
    { "-param", SWIRL_OPTION_param, SWIRL_OPTION_HAS_ARG },
    { "pedantic", SWIRL_OPTION_pedantic, 0},
    { "pthread", SWIRL_OPTION_pthread, 0},
    { "run", SWIRL_OPTION_run, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
    { "rdynamic", SWIRL_OPTION_rdynamic, 0 },
    { "r", SWIRL_OPTION_r, 0 },
    { "s", SWIRL_OPTION_s, 0 },
    { "traditional", SWIRL_OPTION_traditional, 0 },
    { "Wl,", SWIRL_OPTION_Wl, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
    { "Wp,", SWIRL_OPTION_Wp, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
    { "W", SWIRL_OPTION_W, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
    { "O", SWIRL_OPTION_O, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
#ifdef SWIRL_TARGET_ARM
    { "mfloat-abi", SWIRL_OPTION_mfloat_abi, SWIRL_OPTION_HAS_ARG },
#endif
    { "m", SWIRL_OPTION_m, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
    { "f", SWIRL_OPTION_f, SWIRL_OPTION_HAS_ARG | SWIRL_OPTION_NOSEP },
    { "isystem", SWIRL_OPTION_isystem, SWIRL_OPTION_HAS_ARG },
    { "include", SWIRL_OPTION_include, SWIRL_OPTION_HAS_ARG },
    { "nostdinc", SWIRL_OPTION_nostdinc, 0 },
    { "nostdlib", SWIRL_OPTION_nostdlib, 0 },
    { "print-search-dirs", SWIRL_OPTION_print_search_dirs, 0 },
    { "w", SWIRL_OPTION_w, 0 },
    { "pipe", SWIRL_OPTION_pipe, 0},
    { "E", SWIRL_OPTION_E, 0},
    { "MD", SWIRL_OPTION_MD, 0},
    { "MF", SWIRL_OPTION_MF, SWIRL_OPTION_HAS_ARG },
    { "x", SWIRL_OPTION_x, SWIRL_OPTION_HAS_ARG },
    { "ar", SWIRL_OPTION_ar, 0},
#ifdef SWIRL_TARGET_PE
    { "impdef", SWIRL_OPTION_impdef, 0},
#endif
    { "C", SWIRL_OPTION_C, 0},
    { NULL, 0, 0 },
};

static const FlagDef options_W[] = {
    { 0, 0, "all" },
    { offsetof(SwirlState, warn_unsupported), 0, "unsupported" },
    { offsetof(SwirlState, warn_write_strings), 0, "write-strings" },
    { offsetof(SwirlState, warn_error), 0, "error" },
    { offsetof(SwirlState, warn_gcc_compat), 0, "gcc-compat" },
    { offsetof(SwirlState, warn_implicit_function_declaration), WD_ALL,
      "implicit-function-declaration" },
    { 0, 0, NULL }
};

static const FlagDef options_f[] = {
    { offsetof(SwirlState, char_is_unsigned), 0, "unsigned-char" },
    { offsetof(SwirlState, char_is_unsigned), FD_INVERT, "signed-char" },
    { offsetof(SwirlState, nocommon), FD_INVERT, "common" },
    { offsetof(SwirlState, leading_underscore), 0, "leading-underscore" },
    { offsetof(SwirlState, ms_extensions), 0, "ms-extensions" },
    { offsetof(SwirlState, dollars_in_identifiers), 0, "dollars-in-identifiers" },
    { 0, 0, NULL }
};

static const FlagDef options_m[] = {
    { offsetof(SwirlState, ms_bitfields), 0, "ms-bitfields" },
#ifdef SWIRL_TARGET_X86_64
    { offsetof(SwirlState, nosse), FD_INVERT, "sse" },
#endif
    { 0, 0, NULL }
};

static void args_parser_add_file(SwirlState *s, const char* filename, int filetype)
{
    struct filespec *f = swirl_malloc(sizeof *f + strlen(filename));
    f->type = filetype;
    strcpy(f->name, filename);
    dynarray_add(&s->files, &s->nb_files, f);
}

static int args_parser_make_argv(const char *r, int *argc, char ***argv)
{
    int ret = 0, q, c;
    CString str;
    for(;;) {
        while (c = (unsigned char)*r, c && c <= ' ')
          ++r;
        if (c == 0)
            break;
        q = 0;
        cstr_new(&str);
        while (c = (unsigned char)*r, c) {
            ++r;
            if (c == '\\' && (*r == '"' || *r == '\\')) {
                c = *r++;
            } else if (c == '"') {
                q = !q;
                continue;
            } else if (q == 0 && c <= ' ') {
                break;
            }
            cstr_ccat(&str, c);
        }
        cstr_ccat(&str, 0);
        //printf("<%s>\n", str.data), fflush(stdout);
        dynarray_add(argv, argc, swirl_strdup(str.data));
        cstr_free(&str);
        ++ret;
    }
    return ret;
}

/* read list file */
static void args_parser_listfile(SwirlState *s,
    const char *filename, int optind, int *pargc, char ***pargv)
{
    SwirlState *s1 = s;
    int fd, i;
    size_t len;
    char *p;
    int argc = 0;
    char **argv = NULL;

    fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0)
        swirl_error("listfile '%s' not found", filename);

    len = lseek(fd, 0, SEEK_END);
    p = swirl_malloc(len + 1), p[len] = 0;
    lseek(fd, 0, SEEK_SET), read(fd, p, len), close(fd);

    for (i = 0; i < *pargc; ++i)
        if (i == optind)
            args_parser_make_argv(p, &argc, &argv);
        else
            dynarray_add(&argv, &argc, swirl_strdup((*pargv)[i]));

    swirl_free(p);
    dynarray_reset(&s->argv, &s->argc);
    *pargc = s->argc = argc, *pargv = s->argv = argv;
}

PUB_FUNC int swirl_parse_args(SwirlState *s, int *pargc, char ***pargv, int optind)
{
    SwirlState *s1 = s;
    const SWIRLOption *popt;
    const char *optarg, *r;
    const char *run = NULL;
    int x;
    CString linker_arg; /* collect -Wl options */
    int tool = 0, arg_start = 0, noaction = optind;
    char **argv = *pargv;
    int argc = *pargc;

    cstr_new(&linker_arg);

    while (optind < argc) {
        r = argv[optind];
        if (r[0] == '@' && r[1] != '\0') {
            args_parser_listfile(s, r + 1, optind, &argc, &argv);
            continue;
        }
        optind++;
        if (tool) {
            if (r[0] == '-' && r[1] == 'v' && r[2] == 0)
                ++s->verbose;
            continue;
        }
reparse:
        if (r[0] != '-' || r[1] == '\0') {
            if (r[0] != '@') /* allow "swirl file(s) -run @ args ..." */
                args_parser_add_file(s, r, s->filetype);
            if (run) {
                swirl_set_options(s, run);
                arg_start = optind - 1;
                break;
            }
            continue;
        }

        /* find option in table */
        for(popt = swirl_options; ; ++popt) {
            const char *p1 = popt->name;
            const char *r1 = r + 1;
            if (p1 == NULL)
                swirl_error("invalid option -- '%s'", r);
            if (!strstart(p1, &r1))
                continue;
            optarg = r1;
            if (popt->flags & SWIRL_OPTION_HAS_ARG) {
                if (*r1 == '\0' && !(popt->flags & SWIRL_OPTION_NOSEP)) {
                    if (optind >= argc)
                arg_err:
                        swirl_error("argument to '%s' is missing", r);
                    optarg = argv[optind++];
                }
            } else if (*r1 != '\0')
                continue;
            break;
        }

        switch(popt->index) {
        case SWIRL_OPTION_HELP:
            x = OPT_HELP;
            goto extra_action;
        case SWIRL_OPTION_HELP2:
            x = OPT_HELP2;
            goto extra_action;
        case SWIRL_OPTION_I:
            swirl_add_include_path(s, optarg);
            break;
        case SWIRL_OPTION_D:
            swirl_define_symbol(s, optarg, NULL);
            break;
        case SWIRL_OPTION_U:
            swirl_undefine_symbol(s, optarg);
            break;
        case SWIRL_OPTION_L:
            swirl_add_library_path(s, optarg);
            break;
        case SWIRL_OPTION_B:
            /* set swirl utilities path (mainly for swirl development) */
            swirl_set_lib_path(s, optarg);
            break;
        case SWIRL_OPTION_l:
            args_parser_add_file(s, optarg, AFF_TYPE_LIB | (s->filetype & ~AFF_TYPE_MASK));
            s->nb_libraries++;
            break;
        case SWIRL_OPTION_pthread:
            s->option_pthread = 1;
            break;
        case SWIRL_OPTION_bench:
            s->do_bench = 1;
            break;
#ifdef CONFIG_SWIRL_BACKTRACE
        case SWIRL_OPTION_bt:
            s->rt_num_callers = atoi(optarg);
            s->do_backtrace = 1;
            s->do_debug = 1;
            break;
#endif
#ifdef CONFIG_SWIRL_BCHECK
        case SWIRL_OPTION_b:
            s->do_bounds_check = 1;
            s->do_backtrace = 1;
            s->do_debug = 1;
            break;
#endif
        case SWIRL_OPTION_g:
            s->do_debug = 1;
            break;
        case SWIRL_OPTION_c:
            x = SWIRL_OUTPUT_OBJ;
        set_output_type:
            if (s->output_type)
                swirl_warning("-%s: overriding compiler action already specified", popt->name);
            s->output_type = x;
            break;
        case SWIRL_OPTION_d:
            if (*optarg == 'D')
                s->dflag = 3;
            else if (*optarg == 'M')
                s->dflag = 7;
            else if (*optarg == 't')
                s->dflag = 16;
            else if (isnum(*optarg))
                s->g_debug |= atoi(optarg);
            else
                goto unsupported_option;
            break;
        case SWIRL_OPTION_static:
            s->static_link = 1;
            break;
        case SWIRL_OPTION_std:
            if (strcmp(optarg, "=c11") == 0)
                s->cversion = 201112;
            break;
        case SWIRL_OPTION_shared:
            x = SWIRL_OUTPUT_DLL;
            goto set_output_type;
        case SWIRL_OPTION_soname:
            s->soname = swirl_strdup(optarg);
            break;
        case SWIRL_OPTION_o:
            if (s->outfile) {
                swirl_warning("multiple -o option");
                swirl_free(s->outfile);
            }
            s->outfile = swirl_strdup(optarg);
            break;
        case SWIRL_OPTION_r:
            /* generate a .o merging several output files */
            s->option_r = 1;
            x = SWIRL_OUTPUT_OBJ;
            goto set_output_type;
        case SWIRL_OPTION_isystem:
            swirl_add_sysinclude_path(s, optarg);
            break;
        case SWIRL_OPTION_include:
            cstr_printf(&s->cmdline_incl, "#include \"%s\"\n", optarg);
            break;
        case SWIRL_OPTION_nostdinc:
            s->nostdinc = 1;
            break;
        case SWIRL_OPTION_nostdlib:
            s->nostdlib = 1;
            break;
        case SWIRL_OPTION_run:
#ifndef SWIRL_IS_NATIVE
            swirl_error("-run is not available in a cross compiler");
#endif
            run = optarg;
            x = SWIRL_OUTPUT_MEMORY;
            goto set_output_type;
        case SWIRL_OPTION_v:
            do ++s->verbose; while (*optarg++ == 'v');
            ++noaction;
            break;
        case SWIRL_OPTION_f:
            if (set_flag(s, options_f, optarg) < 0)
                goto unsupported_option;
            break;
#ifdef SWIRL_TARGET_ARM
        case SWIRL_OPTION_mfloat_abi:
            /* swirl doesn't support soft float yet */
            if (!strcmp(optarg, "softfp")) {
                s->float_abi = ARM_SOFTFP_FLOAT;
            } else if (!strcmp(optarg, "hard"))
                s->float_abi = ARM_HARD_FLOAT;
            else
                swirl_error("unsupported float abi '%s'", optarg);
            break;
#endif
        case SWIRL_OPTION_m:
            if (set_flag(s, options_m, optarg) < 0) {
                if (x = atoi(optarg), x != 32 && x != 64)
                    goto unsupported_option;
                if (PTR_SIZE != x/8)
                    return x;
                ++noaction;
            }
            break;
        case SWIRL_OPTION_W:
            s->warn_none = 0;
            if (optarg[0] && set_flag(s, options_W, optarg) < 0)
                goto unsupported_option;
            break;
        case SWIRL_OPTION_w:
            s->warn_none = 1;
            break;
        case SWIRL_OPTION_rdynamic:
            s->rdynamic = 1;
            break;
        case SWIRL_OPTION_Wl:
            if (linker_arg.size)
                --linker_arg.size, cstr_ccat(&linker_arg, ',');
            cstr_cat(&linker_arg, optarg, 0);
            if (swirl_set_linker(s, linker_arg.data))
                cstr_free(&linker_arg);
            break;
        case SWIRL_OPTION_Wp:
            r = optarg;
            goto reparse;
        case SWIRL_OPTION_E:
            x = SWIRL_OUTPUT_PREPROCESS;
            goto set_output_type;
        case SWIRL_OPTION_P:
            s->Pflag = atoi(optarg) + 1;
            break;
        case SWIRL_OPTION_MD:
            s->gen_deps = 1;
            break;
        case SWIRL_OPTION_MF:
            s->deps_outfile = swirl_strdup(optarg);
            break;
        case SWIRL_OPTION_dumpversion:
            printf ("%s\n", SWIRL_VERSION);
            exit(0);
            break;
        case SWIRL_OPTION_x:
            x = 0;
            if (*optarg == 'c')
                x = AFF_TYPE_C;
            else if (*optarg == 'a')
                x = AFF_TYPE_ASMPP;
            else if (*optarg == 'b')
                x = AFF_TYPE_BIN;
            else if (*optarg == 'n')
                x = AFF_TYPE_NONE;
            else
                swirl_warning("unsupported language '%s'", optarg);
            s->filetype = x | (s->filetype & ~AFF_TYPE_MASK);
            break;
        case SWIRL_OPTION_O:
            s->optimize = atoi(optarg);
            break;
        case SWIRL_OPTION_print_search_dirs:
            x = OPT_PRINT_DIRS;
            goto extra_action;
        case SWIRL_OPTION_impdef:
            x = OPT_IMPDEF;
            goto extra_action;
        case SWIRL_OPTION_ar:
            x = OPT_AR;
        extra_action:
            arg_start = optind - 1;
            if (arg_start != noaction)
                swirl_error("cannot parse %s here", r);
            tool = x;
            break;
        case SWIRL_OPTION_traditional:
        case SWIRL_OPTION_pedantic:
        case SWIRL_OPTION_pipe:
        case SWIRL_OPTION_s:
        case SWIRL_OPTION_C:
            /* ignored */
            break;
        default:
unsupported_option:
            if (s->warn_unsupported)
                swirl_warning("unsupported option '%s'", r);
            break;
        }
    }
    if (linker_arg.size) {
        r = linker_arg.data;
        goto arg_err;
    }
    *pargc = argc - arg_start;
    *pargv = argv + arg_start;
    if (tool)
        return tool;
    if (optind != noaction)
        return 0;
    if (s->verbose == 2)
        return OPT_PRINT_DIRS;
    if (s->verbose)
        return OPT_V;
    return OPT_HELP;
}

LIBSWIRLAPI void swirl_set_options(SwirlState *s, const char *r)
{
    char **argv = NULL;
    int argc = 0;
    args_parser_make_argv(r, &argc, &argv);
    swirl_parse_args(s, &argc, &argv, 0);
    dynarray_reset(&argv, &argc);
}

PUB_FUNC void swirl_print_stats(SwirlState *s1, unsigned total_time)
{
    if (total_time < 1)
        total_time = 1;
    if (total_bytes < 1)
        total_bytes = 1;
    fprintf(stderr, "* %d idents, %d lines, %d bytes\n"
                    "* %0.3f s, %u lines/s, %0.1f MB/s\n",
           total_idents, total_lines, total_bytes,
           (double)total_time/1000,
           (unsigned)total_lines*1000/total_time,
           (double)total_bytes/1000/total_time);
    fprintf(stderr, "* text %d, data %d, bss %d bytes\n",
           s1->total_output[0], s1->total_output[1], s1->total_output[2]);
#ifdef MEM_DEBUG
    fprintf(stderr, "* %d bytes memory used\n", mem_max_size);
#endif
}
