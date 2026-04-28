typedef struct {
    int (*init)  (void *self);
    int (*output)(void *self, char *greeting);
    int (*fini)  (void *self);
} Output;

#pragma  parcel _
#pragma      typedef: Output
#include "export/output/_"
