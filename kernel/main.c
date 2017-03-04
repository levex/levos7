#include <stdint.h>

#include <levos/arch.h>

int
kernel_main(void)
{
    arch_early_init();

    while (1)
        ;
}
