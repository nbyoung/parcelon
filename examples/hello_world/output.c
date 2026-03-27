#pragma parcel _ { Output }

typedef struct {
    int (*init)  (void *self);
    int (*output)(void *self, char *greeting);
    int (*fini)  (void *self);
} Output;

#include "export/output/_"
