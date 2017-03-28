#ifndef __LEVOS_HEAP_H
#define __LEVOS_HEAP_H

#include <levos/types.h>

void *malloc(size_t);
/* page-aligned malloc */
void *pa_malloc(size_t);
void free(void *);
/* page-aligned free */
void pa_free(void *);

/* custom aligned malloc */
void *na_malloc(size_t, size_t);
/* custom aligned free */
void na_free(size_t, void *);

void heap_init(void);

#endif /* __LEVOS_HEAP_H */
