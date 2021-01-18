/*
 *  Swirl - Another C Compiler
 */

#include "swirl.h"
#if ONE_SOURCE
# include "libswirl.c"
#endif
#include "swirltools.c"

static const char help[] =
    "Swirl "SWIRL_VERSION"\n"
    "Usage: swirl [options...] [-o outfile] [-c] infile(s)...\n"
    "       swirl [options...] -run infile [arguments...]\n"
    "General options:\n"
    "  -c           compile only - generate an object file\n"
    "  -o outfile   set output filename\n"
    "  -run         run compiled source\n"
    "  -fflag       set or reset (with 'no-' prefix) 'flag' (see swirl -hh)\n"
    "  -std=c99     Conform to the ISO 1999 C standard (default).\n"
    "  -std=c11     Conform to the ISO 2011 C standard.\n"
    "  -Wwarning    set or reset (with 'no-' prefix) 'warning' (see swirl -hh)\n"
    "  -w           disable all warnings\n"
    "  --version -v show version\n"
    "  -vv          show search paths or loaded files\n"
    "  -h -hh       show this, show more help\n"
    "  -bench       show compilation statistics\n"
    "  -            use stdin pipe as infile\n"
    "  @listfile    read arguments from listfile\n"
    "Preprocessor options:\n"
    "  -Idir        add include path 'dir'\n"
    "  -Dsym[=val]  define 'sym' with value 'val'\n"
    "  -Usym        undefine 'sym'\n"
    "  -E           preprocess only\n"
    "  -C           keep comments (not yet implemented)\n"
    "Linker options:\n"
    "  -Ldir        add library path 'dir'\n"
    "  -llib        link with dynamic or static library 'lib'\n"
    "  -r           generate (relocatable) object file\n"
    "  -shared      generate a shared library/dll\n"
    "  -rdynamic    export all global symbols to dynamic linker\n"
    "  -soname      set name for shared library to be used at runtime\n"
    "  -Wl,-opt[=val]  set linker option (see swirl -hh)\n"
    "Debugger options:\n"
    "  -g           generate runtime debug info\n"
#ifdef CONFIG_SWIRL_BCHECK
    "  -b           compile with built-in memory and bounds checker (implies -g)\n"
#endif
#ifdef CONFIG_SWIRL_BACKTRACE
    "  -bt[N]       link with backtrace (stack dump) support [show max N callers]\n"
#endif
    "Misc. options:\n"
    "  -x[c|a|b|n]  specify type of the next infile (C,ASM,BIN,NONE)\n"
    "  -nostdinc    do not use standard system include paths\n"
    "  -nostdlib    do not link with standard crt and libraries\n"
    "  -Bdir        set swirl's private include/library dir\n"
    "  -MD          generate dependency file for make\n"
    "  -MF file     specify dependency file name\n"
#if defined(SWIRL_TARGET_I386) || defined(SWIRL_TARGET_X86_64)
    "  -m32/64      defer to i386/x86_64 cross compiler\n"
#endif
    "Tools:\n"
    "  create library  : swirl -ar [rcsv] lib.a files\n"
#ifdef SWIRL_TARGET_PE
    "  create def file : swirl -impdef lib.dll [-v] [-o lib.def]\n"
#endif
    ;

static const char help2[] =
    "Swirl "SWIRL_VERSION" - More Options\n"
    "Special options:\n"
    "  -P -P1                        with -E: no/alternative #line output\n"
    "  -dD -dM                       with -E: output #define directives\n"
    "  -pthread                      same as -D_REENTRANT and -lpthread\n"
    "  -On                           same as -D__OPTIMIZE__ for n > 0\n"
    "  -Wp,-opt                      same as -opt\n"
    "  -include file                 include 'file' above each input file\n"
    "  -isystem dir                  add 'dir' to system include path\n"
    "  -static                       link to static libraries (not recommended)\n"
    "  -dumpversion                  print version\n"
    "  -print-search-dirs            print search paths\n"
    "  -dt                           with -run/-E: auto-define 'test_...' macros\n"
    "Ignored options:\n"
    "  --param  -pedantic  -pipe  -s  -traditional\n"
    "-W... warnings:\n"
    "  all                           turn on some (*) warnings\n"
    "  error                         stop after first warning\n"
    "  unsupported                   warn about ignored options, pragmas, etc.\n"
    "  write-strings                 strings are const\n"
    "  implicit-function-declaration warn for missing prototype (*)\n"
    "-f[no-]... flags:\n"
    "  unsigned-char                 default char is unsigned\n"
    "  signed-char                   default char is signed\n"
    "  common                        use common section instead of bss\n"
    "  leading-underscore            decorate extern symbols\n"
    "  ms-extensions                 allow anonymous struct in struct\n"
    "  dollars-in-identifiers        allow '$' in C symbols\n"
    "-m... target specific options:\n"
    "  ms-bitfields                  use MSVC bitfield layout\n"
#ifdef SWIRL_TARGET_ARM
    "  float-abi                     hard/softfp on arm\n"
#endif
#ifdef SWIRL_TARGET_X86_64
    "  no-sse                        disable floats on x86_64\n"
#endif
    "-Wl,... linker options:\n"
    "  -nostdlib                     do not link with standard crt/libs\n"
    "  -[no-]whole-archive           load lib(s) fully/only as needed\n"
    "  -export-all-symbols           same as -rdynamic\n"
    "  -export-dynamic               same as -rdynamic\n"
    "  -image-base= -Ttext=          set base address of executable\n"
    "  -section-alignment=           set section alignment in executable\n"
#ifdef SWIRL_TARGET_PE
    "  -file-alignment=              set PE file alignment\n"
    "  -stack=                       set PE stack reserve\n"
    "  -large-address-aware          set related PE option\n"
    "  -subsystem=[console/windows]  set PE subsystem\n"
    "  -oformat=[pe-* binary]        set executable output format\n"
    "Predefined macros:\n"
    "  swirl -E -dM - < nul\n"
#else
    "  -rpath=                       set dynamic library search path\n"
    "  -enable-new-dtags             set DT_RUNPATH instead of DT_RPATH\n"
    "  -soname=                      set DT_SONAME elf tag\n"
    "  -Bsymbolic                    set DT_SYMBOLIC elf tag\n"
    "  -oformat=[elf32/64-* binary]  set executable output format\n"
    "  -init= -fini= -as-needed -O   (ignored)\n"
    "Predefined macros:\n"
    "  swirl -E -dM - < /dev/null\n"
#endif
    "See also the manual for more details.\n"
    ;

static const char version[] =
    "swirl version "SWIRL_VERSION" ("
#ifdef SWIRL_TARGET_I386
        "i386"
#elif defined SWIRL_TARGET_X86_64
        "x86_64"
#elif defined SWIRL_TARGET_C67
        "C67"
#elif defined SWIRL_TARGET_ARM
        "ARM"
# ifdef SWIRL_ARM_EABI
        " eabi"
#  ifdef SWIRL_ARM_HARDFLOAT
        "hf"
#  endif
# endif
#elif defined SWIRL_TARGET_ARM64
        "AArch64"
#elif defined SWIRL_TARGET_RISCV64
        "riscv64"
#endif
#ifdef SWIRL_TARGET_PE
        " Windows"
#elif defined(SWIRL_TARGET_MACHO)
        " Darwin"
#elif TARGETOS_FreeBSD || TARGETOS_FreeBSD_kernel
        " FreeBSD"
#elif TARGETOS_OpenBSD
        " OpenBSD"
#elif TARGETOS_NetBSD
        " NetBSD"
#else
        " Linux"
#endif
    ")\n"
    ;

static void print_dirs(const char *msg, char **paths, int nb_paths)
{
    int i;
    printf("%s:\n%s", msg, nb_paths ? "" : "  -\n");
    for(i = 0; i < nb_paths; i++)
        printf("  %s\n", paths[i]);
}

static void print_search_dirs(SwirlState *s)
{
    printf("install: %s\n", s->swirl_lib_path);
    /* print_dirs("programs", NULL, 0); */
    print_dirs("include", s->sysinclude_paths, s->nb_sysinclude_paths);
    print_dirs("libraries", s->library_paths, s->nb_library_paths);
#ifdef SWIRL_TARGET_PE
    printf("libswirl1:\n  %s/lib/"SWIRL_LIBSWIRL1"\n", s->swirl_lib_path);
#else
    printf("libswirl1:\n  %s/"SWIRL_LIBSWIRL1"\n", s->swirl_lib_path);
    print_dirs("crt", s->crt_paths, s->nb_crt_paths);
    printf("elfinterp:\n  %s\n",  DEFAULT_ELFINTERP(s));
#endif
}

static void set_environment(SwirlState *s)
{
    char * path;

    path = getenv("C_INCLUDE_PATH");
    if(path != NULL) {
        swirl_add_sysinclude_path(s, path);
    }
    path = getenv("CPATH");
    if(path != NULL) {
        swirl_add_include_path(s, path);
    }
    path = getenv("LIBRARY_PATH");
    if(path != NULL) {
        swirl_add_library_path(s, path);
    }
}

static char *default_outputfile(SwirlState *s, const char *first_file)
{
    char buf[1024];
    char *ext;
    const char *name = "a";

    if (first_file && strcmp(first_file, "-"))
        name = swirl_basename(first_file);
    snprintf(buf, sizeof(buf), "%s", name);
    ext = swirl_fileextension(buf);
#ifdef SWIRL_TARGET_PE
    if (s->output_type == SWIRL_OUTPUT_DLL)
        strcpy(ext, ".dll");
    else
    if (s->output_type == SWIRL_OUTPUT_EXE)
        strcpy(ext, ".exe");
    else
#endif
    if (s->output_type == SWIRL_OUTPUT_OBJ && !s->option_r && *ext)
        strcpy(ext, ".o");
    else
        strcpy(buf, "a.out");
    return swirl_strdup(buf);
}

static unsigned getclock_ms(void)
{
#ifdef _WIN32
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + (tv.tv_usec+500)/1000;
#endif
}

int main(int argc0, char **argv0)
{
    SwirlState *s, *s1;
    int ret, opt, n = 0, t = 0, done;
    unsigned start_time = 0;
    const char *first_file;
    int argc; char **argv;
    FILE *ppfp = stdout;

redo:
    argc = argc0, argv = argv0;
    s = s1 = swirl_new();
    opt = swirl_parse_args(s, &argc, &argv, 1);

    if (n == 0) {
        if (opt == OPT_HELP) {
            fputs(help, stdout);
            if (!s->verbose)
                return 0;
            ++opt;
        }
        if (opt == OPT_HELP2) {
            fputs(help2, stdout);
            return 0;
        }
        if (opt == OPT_M32 || opt == OPT_M64)
            swirl_tool_cross(s, argv, opt); /* never returns */
        if (s->verbose)
            printf(version);
        if (opt == OPT_AR)
            return swirl_tool_ar(s, argc, argv);
#ifdef SWIRL_TARGET_PE
        if (opt == OPT_IMPDEF)
            return swirl_tool_impdef(s, argc, argv);
#endif
        if (opt == OPT_V)
            return 0;
        if (opt == OPT_PRINT_DIRS) {
            /* initialize search dirs */
            set_environment(s);
            swirl_set_output_type(s, SWIRL_OUTPUT_MEMORY);
            print_search_dirs(s);
            return 0;
        }

        if (s->nb_files == 0)
            swirl_error("no input files\n");

        if (s->output_type == SWIRL_OUTPUT_PREPROCESS) {
            if (s->outfile && 0!=strcmp("-",s->outfile)) {
                ppfp = fopen(s->outfile, "w");
                if (!ppfp)
                    swirl_error("could not write '%s'", s->outfile);
            }
        } else if (s->output_type == SWIRL_OUTPUT_OBJ && !s->option_r) {
            if (s->nb_libraries)
                swirl_error("cannot specify libraries with -c");
            if (s->nb_files > 1 && s->outfile)
                swirl_error("cannot specify output file with -c many files");
        }

        if (s->do_bench)
            start_time = getclock_ms();
    }

    set_environment(s);
    if (s->output_type == 0)
        s->output_type = SWIRL_OUTPUT_EXE;
    swirl_set_output_type(s, s->output_type);
    s->ppfp = ppfp;

    if ((s->output_type == SWIRL_OUTPUT_MEMORY
      || s->output_type == SWIRL_OUTPUT_PREPROCESS)
        && (s->dflag & 16)) { /* -dt option */
        if (t)
            s->dflag |= 32;
        s->run_test = ++t;
        if (n)
            --n;
    }

    /* compile or add each files or library */
    first_file = NULL, ret = 0;
    do {
        struct filespec *f = s->files[n];
        s->filetype = f->type;
        if (f->type & AFF_TYPE_LIB) {
            if (swirl_add_library_err(s, f->name) < 0)
                ret = 1;
        } else {
            if (1 == s->verbose)
                printf("-> %s\n", f->name);
            if (!first_file)
                first_file = f->name;
            if (swirl_add_file(s, f->name) < 0)
                ret = 1;
        }
        done = ret || ++n >= s->nb_files;
    } while (!done && (s->output_type != SWIRL_OUTPUT_OBJ || s->option_r));

    if (s->run_test) {
        t = 0;
    } else if (s->output_type == SWIRL_OUTPUT_PREPROCESS) {
        ;
    } else if (0 == ret) {
        if (s->output_type == SWIRL_OUTPUT_MEMORY) {
#ifdef SWIRL_IS_NATIVE
            ret = swirl_run(s, argc, argv);
#endif
        } else {
            if (!s->outfile)
                s->outfile = default_outputfile(s, first_file);
            if (swirl_output_file(s, s->outfile))
                ret = 1;
            else if (s->gen_deps)
                gen_makedeps(s, s->outfile, s->deps_outfile);
        }
    }

    if (s->do_bench && done && !(t | ret))
        swirl_print_stats(s, getclock_ms() - start_time);
    swirl_delete(s);
    if (!done)
        goto redo; /* compile more files with -c */
    if (t)
        goto redo; /* run more tests with -dt -run */
    if (ppfp && ppfp != stdout)
        fclose(ppfp);
    return ret;
}
