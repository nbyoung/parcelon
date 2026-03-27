#include "output.h"

int main(void) {
    OUT_STATE state;
    OUT->output->init(&state);
    OUT->output->output(&state, "Hello, world!\n");
    OUT->output->fini(&state);
    return 0;
}
