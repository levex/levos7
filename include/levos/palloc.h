#ifndef __LEVOS_PALLOC_H
#define __LEVOS_PALLOC_H

void palloc_init(void);

uintptr_t palloc_get_page(void);
uintptr_t palloc_get_pages(int num);

void palloc_mark_address(uintptr_t);

#endif /* __LEVOS_PALLOC_H */
