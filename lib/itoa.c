#include <levos/types.h>

char tbuf[32];
char bchars[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

void itoa(unsigned i,unsigned base,char* buf) {
    int pos = 0;
    int opos = 0;
    int top = 0;
    memset(tbuf, 0, 32);

    if (i == 0 || base > 16) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (i != 0) {
        tbuf[pos] = bchars[i % base];
        pos++;
        i /= base;
    }
    top = pos --;
    for (opos = 0; opos < top; pos --, opos ++)
        buf[opos] = tbuf[pos];
    buf[opos] = 0;
}

int atoi_10(char *str)
{
    int res = 0;
  
    for (int i = 0; str[i] != '\0'; i ++)
        res = res * 10 + str[i] - '0';
  
    return res;
}

int atoi_10n(char *str, int n)
{
    int res = 0;
  
    for (int i = 0; n && str[i] != '\0'; n --, i ++)
        res = res * 10 + str[i] - '0';
  
    return res;
}
