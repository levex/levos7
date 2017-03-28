#include <levos/kernel.h>
#include <levos/console.h>
#include <levos/arch.h>

static struct console *console;

void
console_init(void)
{
    console = arch_get_console();
}

void
console_emit(char c)
{
    console->putc(c);
    if (c == '\r')
        console->putc('\n');
}

void console_puts(char *s)
{
    while (*s)
        console_emit(*s++);
}

char
console_getchar(void)
{
    return console->readc();
}
