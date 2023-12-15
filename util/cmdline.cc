
#include <cstdint>
#include <cstdlib>

#include <windows.h>

void *xmalloc(size_t nbytes) {
    void *mem;

    mem = malloc(nbytes);

    if (mem == nullptr) {
        return nullptr;
    }

    return mem;
}

void *xrealloc(void *mem, size_t nbytes) {
    void *newmem;

    newmem = realloc(mem, nbytes);

    if (newmem == nullptr) {
        return nullptr;
    }

    return newmem;
}

static void push_argv(int *argc, char ***argv, const char *begin, const char *end) {
    size_t nchars;
    char *str;

    (*argc)++;
    *argv = (char **)xrealloc(*argv, *argc * sizeof(char **));

    nchars = end - begin;
    str = (char *)xmalloc(nchars + 1);
    memcpy(str, begin, nchars);
    str[nchars] = '\0';

    (*argv)[*argc - 1] = str;
}

void args_recover(int *argc_out, char ***argv_out) {
    int argc;
    char **argv;
    char *begin;
    char *pos;
    bool quote;

    argc = 0;
    argv = nullptr;
    quote = false;

    for (begin = pos = GetCommandLine(); *pos; pos++) {
        switch (*pos) {
        case '"':
            if (!quote) {
                quote = true;
                begin = pos + 1;
            } else {
                push_argv(&argc, &argv, begin, pos);

                quote = false;
                begin = nullptr;
            }

            break;

        case ' ':
            if (!quote && begin != nullptr) {
                push_argv(&argc, &argv, begin, pos);
                begin = nullptr;
            }

            break;

        default:
            if (begin == nullptr) {
                begin = pos;
            }

            break;
        }
    }

    if (begin != nullptr && !quote) {
        push_argv(&argc, &argv, begin, pos);
    }

    *argc_out = argc;
    *argv_out = argv;
}
