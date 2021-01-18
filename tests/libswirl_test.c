/*
 * simple test program for libswirl
 *
 * libswirl can be useful to use swirl as a "backend" for a code generator
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "libswirl.h"

void handle_error(void *opaque, const char *msg)
{
    fprintf(opaque, "%s\n", msg);
}

/* this function is called by the generated code */
int add(int a, int b)
{
    return a + b;
}

/* this string is referenced by the generated code */
const char hello[] = "Hello World!";

char my_program[] =
"#include <swirllib.h>\n" /* include the "simple libc header for Swirl" */
"extern int add(int a, int b);\n"
"#ifdef _WIN32\n" /* dynamically linked data needs 'dllimport' */
" __attribute__((dllimport))\n"
"#endif\n"
"extern const char hello[];\n"
"int fib(int n)\n"
"{\n"
"    if (n <= 2)\n"
"        return 1;\n"
"    else\n"
"        return fib(n-1) + fib(n-2);\n"
"}\n"
"\n"
"int foo(int n)\n"
"{\n"
"    printf(\"%s\\n\", hello);\n"
"    printf(\"fib(%d) = %d\\n\", n, fib(n));\n"
"    printf(\"add(%d, %d) = %d\\n\", n, 2 * n, add(n, 2 * n));\n"
"    return 0;\n"
"}\n";

int main(int argc, char **argv)
{
    SwirlState *s;
    int i;
    int (*func)(int);

    s = swirl_new();
    if (!s) {
        fprintf(stderr, "could not create swirl state\n");
        exit(1);
    }

    assert(swirl_get_error_func(s) == NULL);
    assert(swirl_get_error_opaque(s) == NULL);

    swirl_set_error_func(s, stderr, handle_error);

    assert(swirl_get_error_func(s) == handle_error);
    assert(swirl_get_error_opaque(s) == stderr);

    /* if swirllib.h and libswirl1.a are not installed, where can we find them */
    for (i = 1; i < argc; ++i) {
        char *a = argv[i];
        if (a[0] == '-') {
            if (a[1] == 'B')
                swirl_set_lib_path(s, a+2);
            else if (a[1] == 'I')
                swirl_add_include_path(s, a+2);
            else if (a[1] == 'L')
                swirl_add_library_path(s, a+2);
        }
    }

    /* MUST BE CALLED before any compilation */
    swirl_set_output_type(s, SWIRL_OUTPUT_MEMORY);

    if (swirl_compile_string(s, my_program) == -1)
        return 1;

    /* as a test, we add symbols that the compiled program can use.
       You may also open a dll with swirl_add_dll() and use symbols from that */
    swirl_add_symbol(s, "add", add);
    swirl_add_symbol(s, "hello", hello);

    /* relocate the code */
    if (swirl_relocate(s, SWIRL_RELOCATE_AUTO) < 0)
        return 1;

    /* get entry symbol */
    func = swirl_get_symbol(s, "foo");
    if (!func)
        return 1;

    /* run the code */
    func(32);

    /* delete the state */
    swirl_delete(s);

    return 0;
}
