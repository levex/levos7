#include <stdint.h>

#include <levos/arch.h>
#include <levos/kernel.h>
#include <levos/palloc.h>
#include <levos/console.h>
#include <levos/intr.h>
#include <levos/page.h>
#include <levos/task.h>
#include <levos/device.h>
#include <levos/fs.h>
#include <levos/ata.h>
#include <levos/elf.h>
#include <levos/spinlock.h>
#include <levos/ext2.h> /* TODO remove */

void
bss_init(void)
{
    memset(&_bss_start, 0, &_bss_end - &_bss_end);
}

static spinlock_t setup_lock;

__noreturn void
kernel_main(void)
{
    char c;

    bss_init();

    spin_lock_init(&setup_lock);
    spin_lock(&setup_lock);

    arch_early_init();

    palloc_init();

    console_init();

    printk("LevOS %d.%d booting...\n", LEVOS_VERSION_MAJOR, LEVOS_VERSION_MINOR);
    printk("main: enabling interrupts\n");
    arch_preirq_init();
    ENABLE_IRQ();
    printk("main: IRQs enabled\n");

    arch_late_init();

    paging_init();

    heap_init();

    virt_kmap_init();
    
    sched_init();

    panic("Scheduler failed to start late_init\n");

    __not_reached();
}

void
do_mount(void)
{
    struct device *dev;
    int root_mounted = 0, rc = 0;

    for_each_blockdev(dev) {
        rc = vfs_mount("/", dev);
        if (!rc) {
            root_mounted = 1;
            break;
        }
    }
    if (!root_mounted)
        panic("Unable to mount root directory\n");
}

/* mirroring exec_elf */
void
do_first_init()
{
    DISABLE_IRQ();

    /* map a stack */
    uint32_t p = palloc_get_page();
    map_page_curr(p, VIRT_BASE - 4096, 1);

    /* map it in the kernel */
    map_page_curr(p, (unsigned long)0xD0000000 - 0x1000, 0);
    __flush_tlb();

    /* zero it */
    memset((void *) VIRT_BASE - 0x1000, 0, 0x1000);

    /* it needs a filetable */
    setup_filetable(current_task);

    /* TSS */
    tss_update(current_task);

    current_task->sys_regs = VIRT_BASE;
    current_task->regs = 0;

    asm volatile (""
            "movl $"__stringify(VIRT_BASE)", %%esp;"
            "movl $"__stringify(VIRT_BASE)", %%ebp;"
            "movw $0x23, %%ax;"
            "movw %%ax, %%ds;"
            "movw %%ax, %%es;"
            "movw %%ax, %%fs;"
            "movw %%ax, %%gs;"
            ""
            "pushl $0x23;"
            "pushl $"__stringify(VIRT_BASE)";"
            "pushl $0x202;"
            "pushl $0x1b;"
            "pushl %%ebx;"
            "sti;"
            "iretl"::"b"(current_task->bstate.entry));
}

void
init_task(void)
{
    int rc, fd;

    /* wait until the setup is finished */
    spin_lock(&setup_lock);
    sched_yield();

    /* open the init executable */
    struct file *f = vfs_open("/init");
    if ((int) f < 0 && (int)f > -4096)
        panic("no /init found, please reboot\n");

    /* do some @TODO testing */
    struct ext2_inode inode;
    int ino = ext2_new_inode(f->fs, &inode);
    printk("created new inode %d\n", ino);
    struct ext2_dir *dirent = ext2_new_dirent(ino, "lev");
    printk("Created new dirent, now placing it in root\n");
    ext2_place_dirent(f->fs, 2, dirent);
    /*int block = ext2_alloc_block(f->fs);
    printk("Ext2 allocated block %d\n", block);*/

    /* create a new page directory */
    current_task->mm = new_page_directory();
    activate_pgd(current_task->mm);

    /* load the ELF file */
    rc = load_elf(f);

    /* if we finished then drop to userspace */
    if (rc == 0)
        do_first_init();

    /* if we get here then we failed to setup the init task */
    panic("failed to load init task, rc = %d\n", rc);
}

void __noreturn
late_init(void)
{
    char c;

    dev_init();

    ata_init();

    vfs_init();

    do_mount();

    /* use condvar */
    spin_unlock(&setup_lock);

#if 0
    printk("main: falling through to echo\n");
    while (c = console_getchar()) {
        if (c == 'p')
            panic("End of life\n");
        console_emit(c);
    }
#endif
    /* become the idle task */
    extern struct task *current_task;
    printk("main: idle process idling from now\n");
    while (1)
        sched_yield();

    __not_reached();
}
