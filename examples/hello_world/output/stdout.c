#include <stdio.h>
#include "import/output/_.out"

#pragma parcel stdout { Stdout output }

typedef struct {
} Stdout;

static int init(Stdout *self) {
    (void)self;
    return 1;
}

static int print(Stdout *self, char *greeting) {
    (void)self;
    return fputs(greeting, stdout) != EOF;
}

static int fini(Stdout *self) {
    (void)self;
    return fflush(stdout) != EOF;
}

const out_Output output = {
    .init   = (int (*)(void *))         init,
    .output = (int (*)(void *, char *)) print,
    .fini   = (int (*)(void *))         fini,
};

#include "export/output/stdout"
