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
#include <levos/packet.h>
#include <levos/spinlock.h>
#include <levos/work.h>
#include <levos/pci.h>
#include <levos/arp.h>
#include <levos/socket.h>
#ifdef CONFIG_TCP_TEST
# include <levos/tcp.h>
#endif
#ifdef CONFIG_RING_BUFFER_TEST
# include <levos/ring.h>
#endif
#include <levos/ext2.h> /* TODO remove */
#include <levos/tty.h>

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
    uint32_t stack = VIRT_BASE;
    int argc = 0, envc = 0, i;
    char **argvp = current_task->bstate.argvp;
    char **envp = current_task->bstate.envp;

    printk("starting pid %d as the init task\n", current_task->pid);

    DISABLE_IRQ();

    /* setup the task structure for exposure to userspace */
    current_task->ppid = current_task->pid;
    current_task->pgid = current_task->pid;
    current_task->sid  = current_task->pid;

    /* map a stack */
    uint32_t p = palloc_get_page();
    map_page_curr(p, VIRT_BASE - 4096, 1);

    /* map it in the kernel */
    map_page_curr(p, (unsigned long)0xD0000000 - 0x1000, 0);
    __flush_tlb();

    /* zero it */
    memset((void *) VIRT_BASE - 0x1000, 0, 0x1000);

    do_args_stack(&stack, argvp, envp);

    /* it needs a filetable */
    setup_filetable(current_task);

    /* setup to use tty0 */
    extern struct device console_device;
    struct tty_device *tty = tty_new(&console_device, NULL);
    tty->tty_fg_proc = current_task->pid;
    struct file *f_in = tty_get_file(tty);
    current_task->file_table[1] = current_task->file_table[0] = f_in;

    /* TSS */
    tss_update(current_task);

    current_task->sys_regs = (void *) VIRT_BASE;
    current_task->regs = 0;

    asm volatile (""
            //"movl $"__stringify(VIRT_BASE)", %%esp;"
            //"movl $"__stringify(VIRT_BASE)", %%ebp;"
            "movl %%eax, %%esp;"
            "movl %%eax, %%ebp;"
            "pushl %%eax;"
            "movw $0x23, %%ax;"
            "movw %%ax, %%ds;"
            "movw %%ax, %%es;"
            "movw %%ax, %%fs;"
            "movw %%ax, %%gs;"
            "popl %%eax;"
            ""
            "pushl $0x23;"
            "pushl %%eax;"
            "pushl $0x202;"
            "pushl $0x1b;"
            "pushl %%ebx;"
            "sti;"
            "iretl"::"a"(stack), "b"(current_task->bstate.entry));
}

void
init_task(void)
{
    int rc, fd;

    /* wait until the setup is finished */
    spin_lock(&setup_lock);
    sched_yield();

#ifdef CONFIG_PATH_TEST
    printk("--- path testing enabled ---\n");

    char *test_path_1 = "/bin/sh";
    char *p = __path_get_path(test_path_1);
    printk("name: %s path: %s\n", __path_get_name(test_path_1), p);
    __path_free(p);

    char *test_path_2 = "/";
    p = __path_get_path(test_path_2);
    printk("name: %s path: %s\n", __path_get_name(test_path_2), p);
    __path_free(p);

    char *test_path_3 = "/init";
    p = __path_get_path(test_path_3);
    printk("name: %s path: %s\n", __path_get_name(test_path_3), p);
    __path_free(p);

    char *test_path_4 = "/usr/bin/";
    p = __path_get_path(test_path_4);
    printk("name: %s path: %s\n", __path_get_name(test_path_4), p);
    __path_free(p);
#endif

#ifdef CONFIG_RING_BUFFER_TEST
    struct ring_buffer rb;
    char *rb_test_string = "123456789ABCDEF0";
    char rb_read[17];
    memset(rb_read, 0 , 17);
    ring_buffer_init(&rb, 10);
    ring_buffer_set_flags(&rb, RB_FLAG_NONBLOCK);
    printk("ringtest: testing default ringbuffer...\n");
    int n_wrote = ring_buffer_write(&rb, rb_test_string, 16);
    int n_read = ring_buffer_read(&rb, rb_read, 5); /* 12345 */
    printk("ringtest: wrote from \"%s\" (%d), read back \"%s\" (%d) \n",
            rb_test_string, n_wrote, rb_read, n_read);
    n_wrote = ring_buffer_write(&rb, "hello", 5);
    printk("ringtest: wrote 5 bytes\n");
    memset(rb_read, 0 , 17);
    n_read = ring_buffer_read(&rb, rb_read, 5);
    printk("ringtest: read 2nd 5 bytes: %s\n", rb_read); /* should be 6789A */
    memset(rb_read, 0 , 17);
    n_read = ring_buffer_read(&rb, rb_read, 5);
    printk("ringtest: read 3rd 5 bytes: %s\n", rb_read); /* should be hello */
#endif

    /* open the init executable */
    struct file *f = vfs_open("/init");
#ifdef CONFIG_EXT2_TEST
    struct ext2_inode inode;
    int ino = ext2_new_inode(f->fs, &inode);
    printk("created new inode %d\n", ino);
    struct ext2_dir *dirent = ext2_new_dirent(ino, "lev");
    printk("Created new dirent, now placing it in root\n");
    ext2_place_dirent(f->fs, 2, dirent);

    f = vfs_open("/lev");

    // len: 13
    char *the_data = "Hello, world!\n";
    ext2_write_file(f, the_data, strlen(the_data));
    for (int i = 0; i < 100; i ++) {
        //printk("--- i = %d ---\n", i);
        ext2_write_file(f, the_data, strlen(the_data));
    }

    f = vfs_open("/init");
#endif

    if (f == NULL || ((int) f < 0 && (int)f > -4096))
        panic("no /init found, please reboot\n");

    /* create a new page directory */
    current_task->mm = new_page_directory();
    activate_pgd(current_task->mm);

    char *init_argvp[] = {
        "/init",
        "hello",
        NULL,
    };

    char *init_envp[] = {
        "HOME=/",
        "USER=root",
        NULL,
    };

    /* load the ELF file */
    rc = load_elf(f, init_argvp, init_envp);

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

    net_init();

    struct task *pkthndlr = create_kernel_task(packet_processor_thread);
    sched_add_rq(pkthndlr);
    sched_yield();

    do_mount();

    work_init();

    pci_init();

#ifdef CONFIG_TCP_TEST
    struct net_info *ni = &net_get_default()->ndev_ni;
    test_tcp(ni);
#endif

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
