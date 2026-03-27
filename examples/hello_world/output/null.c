#include "import/output/_.out"

#pragma parcel null { Null output }

typedef struct {
} Null;

static int init(Null *self) {
    (void)self;
    return 1;
}

static int discard(Null *self, char *greeting) {
    (void)self;
    (void)greeting;
    return 1;
}

static int fini(Null *self) {
    (void)self;
    return 1;
}

const out_Output output = {
    .init   = (int (*)(void *))         init,
    .output = (int (*)(void *, char *)) discard,
    .fini   = (int (*)(void *))         fini,
};

#include "export/output/null"
