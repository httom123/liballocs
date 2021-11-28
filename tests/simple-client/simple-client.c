#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <liballocs.h>

extern __thread void *__current_allocsite __attribute__((weak));

int main(void)
{
	char file_name[128];
    unsigned *line_num = malloc(sizeof (unsigned int));
    __liballocs_get_source_coords(main, file_name, line_num);
	printf("file name is %s", file_name);
	printf("line number is %d ", line_num);

	return 0;
}

