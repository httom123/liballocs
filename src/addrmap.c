#include <stdint.h>
#include <unistd.h>
#include "addrmap.h"

uintptr_t startup_brk;

static void save_startup_brk(void) __attribute__((constructor(100))); /* top user priority... */
static void save_startup_brk(void)
{
	startup_brk = (uintptr_t) sbrk(0);
}
