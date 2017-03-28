#ifndef __LEVOS_STRING_H
#define __LEVOS_STRING_H

#include <levos/types.h>

void *memset(void *, int, size_t);
void *memcpy(void *, const void *, size_t);

int strcmp (const char *, const char *);
char *strdup(char *);
size_t strlen(const char *);
size_t strncmp(char *, char *, size_t);
char *strtok_r(char *, const char *, char **);
void itoa(unsigned, unsigned, char *);

#endif /* __LEVOS_STRING_H */
