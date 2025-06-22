#include "common.h"

#define PANIC(msg)                                                        \
    do {                                                                  \
        terminal_writestring("PANIC: ");                                  \
        terminal_writestring(msg);                                        \
        terminal_writestring("\n");                                       \
        while (1) {}                                                      \
    } while (0)
