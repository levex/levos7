#ifndef __LEVOS_CONSOLE_H
#define __LEVOS_CONSOLE_H

struct console {
    void (*putc)(char);
    char (*readc)(void);
};

char console_getchar(void);
void console_emit(char);
void console_puts(char *);

void console_init(void);

#endif /* __LEVOS_CONSOLE_H */
