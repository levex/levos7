#include <levos/kernel.h>
#include <levos/console.h>
#include <levos/ip.h>
#include <levos/eth.h>

#include <stdarg.h>

void vprintk(char *fmt, va_list ap)
{
    char *s = 0;
    int i;

    for (i = 0; i < strlen(fmt); i++)
    {
        if (fmt[i] == '%') {
            switch(fmt[i + 1]) {
                case 's':
                    s = va_arg(ap, char *);
                    if (s == NULL)
                        console_puts("(null)");
                    else if ((unsigned int) s < 0x1000)
                        console_puts("(weird null)");
                    else
                        console_puts(s);
                    i ++;
                    break;
                case 'd': {
                    int c = va_arg(ap, int);
                    char str[32] = {0};
                    itoa(c, 10, str);
                    console_puts(str);
                    i ++;
                    break;
                }
                case 'x': {
                    int c = va_arg(ap, int);
                    char str[32] = {0};
                    itoa(c, 16, str);
                    console_puts(str);
                    i++;
                    break;
                }
                case 'X': {
                    int c = va_arg(ap, int);
                    char str[32] = {0};
                    itoa(c, 16, str);
                    if (strlen(str) < 2)
                        console_emit('0');
                    console_puts(str);
                    i++;
                    break;
                }
                case 'p': {
                    int c = fmt[i + 2];
                    switch (c) {
                        case 'E': {/* eth addr */
                            char *ptr = va_arg(ap, char *);
                            printk_print_eth_addr(ptr);
                            i += 2;
                            break;
                        }
                        case 'I': { /* ip addr */
                            uint32_t ptr = va_arg(ap, uint32_t);
                            printk_print_ip_addr(ptr);
                            i += 2;
                            break;
                        }
                        case 'i': { /* le ip addr */
                            uint32_t ptr = va_arg(ap, uint32_t);
                            printk_print_le_ip_addr(ptr);
                            i += 2;
                            break;
                        }
                    }
                    break;
                }
                case 'c': {
                    char c = (char)(va_arg(ap, int) & ~0xFFFFFF00);
                    console_emit(c);
                    i ++;
                    break;
                }
            }
        } else {
            console_emit(fmt[i]);
        }
    }
}


void
printk(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    vprintk(fmt, ap);

    va_end(ap);
}
