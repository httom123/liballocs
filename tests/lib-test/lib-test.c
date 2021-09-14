#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <elf.h>

Elf64_Ehdr ehdr; // FIXME: giant hack! see allocators/elf-file.c
int main(void)
{
	/* The liballocs source code includes some unit tests.
	 * These are run as constructors from liballocs_test.so,
	 * so dlopening that will run them. */
	assert(getenv("LIBALLOCS_BUILD"));
	char *path = getenv("LIBALLOCS_BUILD");
	void *handle = dlopen(path, RTLD_NOW);
	assert(handle);
	printf("Successfully constructed %s\n", path);
	return 0;
}
