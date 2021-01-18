#ifndef LIBSWIRL_H
#define LIBSWIRL_H

#ifndef LIBSWIRLAPI
# define LIBSWIRLAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct SwirlState;

typedef struct SwirlState SwirlState;

typedef void (*SWIRLErrorFunc)(void *opaque, const char *msg);

/* create a new SWIRL compilation context */
LIBSWIRLAPI SwirlState *swirl_new(void);

/* free a SWIRL compilation context */
LIBSWIRLAPI void swirl_delete(SwirlState *s);

/* set CONFIG_SWIRLDIR at runtime */
LIBSWIRLAPI void swirl_set_lib_path(SwirlState *s, const char *path);

/* set error/warning display callback */
LIBSWIRLAPI void swirl_set_error_func(SwirlState *s, void *error_opaque, SWIRLErrorFunc error_func);

/* return error/warning callback */
LIBSWIRLAPI SWIRLErrorFunc swirl_get_error_func(SwirlState *s);

/* return error/warning callback opaque pointer */
LIBSWIRLAPI void *swirl_get_error_opaque(SwirlState *s);

/* set options as from command line (multiple supported) */
LIBSWIRLAPI void swirl_set_options(SwirlState *s, const char *str);

/*****************************/
/* preprocessor */

/* add include path */
LIBSWIRLAPI int swirl_add_include_path(SwirlState *s, const char *pathname);

/* add in system include path */
LIBSWIRLAPI int swirl_add_sysinclude_path(SwirlState *s, const char *pathname);

/* define preprocessor symbol 'sym'. value can be NULL, sym can be "sym=val" */
LIBSWIRLAPI void swirl_define_symbol(SwirlState *s, const char *sym, const char *value);

/* undefine preprocess symbol 'sym' */
LIBSWIRLAPI void swirl_undefine_symbol(SwirlState *s, const char *sym);

/*****************************/
/* compiling */

/* add a file (C file, dll, object, library, ld script). Return -1 if error. */
LIBSWIRLAPI int swirl_add_file(SwirlState *s, const char *filename);

/* compile a string containing a C source. Return -1 if error. */
LIBSWIRLAPI int swirl_compile_string(SwirlState *s, const char *buf);

/*****************************/
/* linking commands */

/* set output type. MUST BE CALLED before any compilation */
LIBSWIRLAPI int swirl_set_output_type(SwirlState *s, int output_type);
#define SWIRL_OUTPUT_MEMORY   1 /* output will be run in memory (default) */
#define SWIRL_OUTPUT_EXE      2 /* executable file */
#define SWIRL_OUTPUT_DLL      3 /* dynamic library */
#define SWIRL_OUTPUT_OBJ      4 /* object file */
#define SWIRL_OUTPUT_PREPROCESS 5 /* only preprocess (used internally) */

/* equivalent to -Lpath option */
LIBSWIRLAPI int swirl_add_library_path(SwirlState *s, const char *pathname);

/* the library name is the same as the argument of the '-l' option */
LIBSWIRLAPI int swirl_add_library(SwirlState *s, const char *libraryname);

/* add a symbol to the compiled program */
LIBSWIRLAPI int swirl_add_symbol(SwirlState *s, const char *name, const void *val);

/* output an executable, library or object file. DO NOT call
   swirl_relocate() before. */
LIBSWIRLAPI int swirl_output_file(SwirlState *s, const char *filename);

/* link and run main() function and return its value. DO NOT call
   swirl_relocate() before. */
LIBSWIRLAPI int swirl_run(SwirlState *s, int argc, char **argv);

/* do all relocations (needed before using swirl_get_symbol()) */
LIBSWIRLAPI int swirl_relocate(SwirlState *s1, void *ptr);
/* possible values for 'ptr':
   - SWIRL_RELOCATE_AUTO : Allocate and manage memory internally
   - NULL              : return required memory size for the step below
   - memory address    : copy code to memory passed by the caller
   returns -1 if error. */
#define SWIRL_RELOCATE_AUTO (void*)1

/* return symbol value or NULL if not found */
LIBSWIRLAPI void *swirl_get_symbol(SwirlState *s, const char *name);

/* return symbol value or NULL if not found */
LIBSWIRLAPI void swirl_list_symbols(SwirlState *s, void *ctx,
    void (*symbol_cb)(void *ctx, const char *name, const void *val));

#ifdef __cplusplus
}
#endif

#endif
