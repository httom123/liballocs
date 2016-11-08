#define _GNU_SOURCE
#include <assert.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <link.h>
#include "relf.h"
#include "liballocs_private.h"
#include "pageindex.h"

static const char *asciiz_start;
static const char *asciiz_end;

static const char **env_vector_start;
static const char **env_vector_terminator;

static const char **argv_vector_start;
static const char **argv_vector_terminator;

static ElfW(auxv_t) *auxv_array_start;
static ElfW(auxv_t) *auxv_array_terminator;

static intptr_t *p_argcount;

void *program_entry_point;

void __auxv_allocator_init(void) __attribute__((constructor(101)));
void __auxv_allocator_init(void)
{
	auxv_array_start = get_auxv((const char **) environ, environ[0]);
	if (!auxv_array_start) return;
	
	auxv_array_terminator = auxv_array_start; 
	while (auxv_array_terminator->a_type != AT_NULL) ++auxv_array_terminator;
	
	/* auxv_array_start[0] is the first word higher than envp's null terminator. */
	env_vector_terminator = ((const char**) auxv_array_start) - 1;
	assert(!*env_vector_terminator);
	env_vector_start = env_vector_terminator;
	while (*((char**) env_vector_start - 1)) --env_vector_start;
	
	/* argv_vector_terminator is the next word lower than envp's first entry. */
	argv_vector_terminator = ((const char**) env_vector_start) - 1;
	assert(!*argv_vector_terminator);
	argv_vector_start = argv_vector_terminator;
	unsigned nargs = 0;
	/* To search for the start of the array, we look for an integer that is
	 * a plausible argument count... which won't look like any pointer we're seeing. */
	#define MAX_POSSIBLE_ARGS 4194304
	while (*((uintptr_t*) argv_vector_start - 1) > MAX_POSSIBLE_ARGS)
	{
		--argv_vector_start;
		++nargs;
	}
	assert(*((uintptr_t*) argv_vector_start - 1) == nargs);
	p_argcount = (intptr_t*) argv_vector_start - 1;
	
	/* Now for the asciiz. We lump it all in one chunk. */
	char *asciiz_start = (char*) (auxv_array_terminator + 1);
	char *asciiz_end = asciiz_start;
	while (*(intptr_t *) asciiz_end != 0) asciiz_end += sizeof (void*);
	
	ElfW(auxv_t) *found_at_entry = auxv_lookup(auxv_array_start, AT_ENTRY);
	if (found_at_entry) program_entry_point = (void*) found_at_entry->a_un.a_val;
}

static liballocs_err_t get_info(void * obj, struct big_allocation *maybe_bigalloc, 
	struct uniqtype **out_type, void **out_base, 
	unsigned long *out_size, const void **out_site)
{
	/* Decide whether it falls into the asciiz, auxv_t or ptr vector parts. */
	if ((char*) obj >= (char*) auxv_array_start
			&& (char*) obj <= (char*) auxv_array_terminator)
	{
		if (out_type) *out_type = pointer_to___uniqtype__Elf64_auxv_t;
		if (out_base) *out_base = auxv_array_start +
				sizeof (Elf64_auxv_t) * (((char*) obj - (char*) auxv_array_start) / sizeof (Elf64_auxv_t));
		if (out_size) *out_size = sizeof (Elf64_auxv_t);
		if (out_site) *out_site = program_entry_point;
		return NULL;
	}
	
	if ((char*) obj >= (char*) argv_vector_start
			&& (char*) obj <= (char*) env_vector_terminator)
	{
		if (out_type) *out_type = pointer_to___uniqtype____PTR_signed_char;
		if (out_base) *out_base = (void*) ROUND_DOWN((uintptr_t) obj, sizeof (char*));
		if (out_size) *out_size = sizeof (char*);
		if (out_site) *out_site = program_entry_point;
		return NULL;
	}
	
	if ((char*) obj >= asciiz_start && (char*) obj <= asciiz_end)
	{
		if (out_type) *out_type = pointer_to___uniqtype____ARR0_signed_char; // FIXME: actually array
		if (out_base) *out_base = (char*) asciiz_start;
		if (out_size) *out_size = asciiz_end - asciiz_start;
		if (out_site) *out_site = program_entry_point;
		return NULL;
	}
	
	if ((char*) obj >= (char*) p_argcount 
		&& (char*) obj <= (char*) p_argcount + sizeof (intptr_t))
	{
		if (out_type) *out_type = pointer_to___uniqtype__intptr_t;
		if (out_base) *out_base = (void*) p_argcount;
		if (out_size) *out_size = sizeof (intptr_t);
		if (out_site) *out_site = program_entry_point;
		return NULL;
	}
	
	return &__liballocs_err_object_of_unknown_storage;
}

/* HACK: we have a special link to the stack allocator. */
void __stack_allocator_notify_init_stack_region(void *begin, void *end);

void *__top_of_initial_stack __attribute__((visibility("protected")));
static struct big_allocation *our_bigalloc;
void __auxv_allocator_notify_init_stack_mapping(void *begin, void *end)
{
	if (!auxv_array_start) __auxv_allocator_init();
	if (!p_argcount) abort();
	
	__top_of_initial_stack = end; /* i.e. the highest address */
	our_bigalloc = __liballocs_new_bigalloc(
		begin,
		(char*) end - (char*) begin,
		(struct meta_info) {
			.what = DATA_PTR,
			.un = {
				opaque_data: {
					.data_ptr = NULL,
					.free_func = NULL
				}
			}
		},
		NULL,
		&__auxv_allocator
	);
	if (!our_bigalloc) abort();
	
	/* Don't do this. Queries on auxv region are on a "crack" so we don't want
	 * to descend to the suballocator. Recording suballocators is only useful at
	 * leaf level anyway, since the child bigalloc (which can be sized precisely,
	 * leaving the auxv cracks excluded) fills this role at branch level. */
	// our_bigalloc->suballocator = &__stack_allocator;
	__stack_allocator_notify_init_stack_region(begin, p_argcount);
	/* HACK: undo the suballocation relationship created in pageindex. Ideally
	 * it wouldn't do this. But it doesn't know any better... all bigallocs
	 * are initially childless, so it's the right thing to do. */
	our_bigalloc->suballocator = NULL;
}

struct allocator __auxv_allocator = {
	.name = "auxv",
	.is_cacheable = 1,
	.get_info = get_info
};
