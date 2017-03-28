#include <levos/x86.h>

#define PIC0_CTRL	0x20
#define PIC0_DATA	0x21
#define PIC1_CTRL	0xa0
#define PIC1_DATA	0xa1

void
pic_init(void)
{
  outportb(PIC0_DATA, 0xff);
  outportb(PIC1_DATA, 0xff);

  outportb(PIC0_CTRL, 0x11);
  outportb(PIC0_DATA, 0x20);
  outportb(PIC0_DATA, 0x04);
  outportb(PIC0_DATA, 0x01);

  outportb(PIC1_CTRL, 0x11);
  outportb(PIC1_DATA, 0x28);
  outportb(PIC1_DATA, 0x02);
  outportb(PIC1_DATA, 0x01);

  outportb(PIC0_DATA, 0x00);
  outportb(PIC1_DATA, 0x00);
}

void
pic_eoi(int irq) 
{
    if (irq >= 0x28)
        outportb(0xA0, 0x20);

    outportb(0x20, 0x20);
}
