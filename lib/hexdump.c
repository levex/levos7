#include <levos/kernel.h>


#define __isprint(x) (x >= 'a' && x <= 'Z')

/* adapted from pintos for LevOS 7 */
void
__hex_dump (uintptr_t ofs, const void *buf_, size_t size, bool ascii)
{
  const uint8_t *buf = buf_;
  const size_t per_line = 16; /* Maximum bytes per line. */

  while (size > 0)
    {
      size_t start, end, n;
      size_t i;
      
      /* Number of bytes on this line. */
      start = ofs % per_line;
      end = per_line;
      if (end - start > size)
        end = start + size;
      n = end - start;

      /* Print line. */
      printk ("%x  ", (uintmax_t) ROUND_DOWN (ofs, per_line));
      for (i = 0; i < start; i++)
        printk ("   ");
      for (; i < end; i++) 
        printk ("%X%c",
                buf[i - start], i == per_line / 2 - 1? '-' : ' ');
      if (ascii) 
        {
          for (; i < per_line; i++)
            printk ("   ");
          printk ("|");
          for (i = 0; i < start; i++)
            printk (" ");
          for (; i < end; i++)
            printk ("%c",
                    __isprint(buf[i - start]) ? buf[i - start] : '.');
          for (; i < per_line; i++)
            printk (" ");
          printk ("|");
        }
      printk ("\n");

      ofs += n;
      buf += n;
      size -= n;
    }
}

void
hex_dump_noascii(char *ptr, size_t len)
{
    __hex_dump(ptr, ptr, len, 0);
}

void
hex_dump(char *ptr, size_t len)
{
    __hex_dump(ptr, ptr, len, 1);
}
