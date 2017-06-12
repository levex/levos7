#include <levos/string.h>
#include <levos/types.h>
#include <levos/kernel.h>
    
void *
memset (void *dst_, int value, size_t size) 
{
  unsigned char *dst = dst_;
  
  while (size-- > 0)
    *dst++ = value;

  return dst_;
}

void *
memsetl (void *dst_, uint32_t value, size_t size) 
{
  uint32_t *dst = dst_;

  size /= 4;
  
  while (size-- > 0)
    *dst++ = value;

  return dst_;
}

void *
memcpy (void *dst_, const void *src_, size_t size) 
{
  unsigned char *dst = dst_;
  const unsigned char *src = src_;

  while (size-- > 0)
    *dst++ = *src++;

  return dst_;
}

void *
memcpyl(uint32_t *dst_, uint32_t *src_, size_t size)
{
  uint32_t *dst = dst_;
  const uint32_t *src = src_;

  while (size -- > 0)
    *dst++ = *src++;

  return dst_;
}

void *
mg_memcpy(void *restrict dst, const void *restrict src, size_t n)
{
  n /= 4;
  asm (
    "rep movsd"
    : /* no output */
    : "D" (dst), "S" (src), "c" (n)
    : /* no clobbering */
    );
  return dst;
}

size_t
strlen (const char *string) 
{
    const char *p;

    for (p = string; *p != '\0'; p++)
        continue;

    return p - string;
}

size_t strncmp(char *str1, char *str2, size_t n)
{
    while (n--)
        if (*str1++ != *str2++)
            return *(unsigned char *)(str1 - 1) - *(unsigned char *)(str2 - 1);
    return 0;
}

int
strcmp (const char *a_, const char *b_) 
{
  const unsigned char *a = (const unsigned char *) a_;
  const unsigned char *b = (const unsigned char *) b_;

  while (*a != '\0' && *a == *b) 
    {
      a++;
      b++;
    }

  return *a < *b ? -1 : *a > *b;
}

char *
strdup(char *s)
{
    if (!s)
        return NULL;

    char *r = malloc(strlen(s) + 1);
    if (!r)
        return NULL;

    memset(r, 0, strlen(s) + 1);
    memcpy(r, s, strlen(s));
    return r;
}

char *
strrchr (const char *string, int c_) 
{
  char c = c_;
  const char *p = NULL;

  for (; *string != '\0'; string++)
    if (*string == c)
      p = string;
  return (char *) p;
}

char *
strchr (const char *string, int c_) 
{
  char c = c_;

  for (;;) 
    if (*string == c)
      return (char *) string;
    else if (*string == '\0')
      return NULL;
    else
      string++;
}

char *
strnchr (const char *string, size_t len, int c_) 
{
  char c = c_;
  int i;

  for (i = 0; i < len; i++) 
    if (*string == c)
      return (char *) string;
    else if (*string == '\0')
      return NULL;
    else
      string++;

  return NULL;
}

char *
strtok_r (char *s, const char *delimiters, char **save_ptr) 
{
  char *token;
  
  /* If S is nonnull, start from it.
     If S is null, start from saved position. */
  if (s == NULL)
    s = *save_ptr;

  /* Skip any DELIMITERS at our current position. */
  while (strchr (delimiters, *s) != NULL) 
    {
      /* strchr() will always return nonnull if we're searching
         for a null byte, because every string contains a null
         byte (at the end). */
      if (*s == '\0')
        {
          *save_ptr = s;
          return NULL;
        }

      s++;
    }

  /* Skip any non-DELIMITERS up to the end of the string. */
  token = s;
  while (strchr (delimiters, *s) == NULL)
    s++;
  if (*s != '\0') 
    {
      *s = '\0';
      *save_ptr = s + 1;
    }
  else 
    *save_ptr = s;
  return token;
}
