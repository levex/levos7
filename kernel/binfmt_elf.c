#include <levos/kernel.h>
#include <levos/elf.h>
#include <levos/fs.h>
#include <levos/errno.h>
#include <levos/page.h>
#include <levos/task.h>
#include <levos/palloc.h>
#include <levos/string.h>

/*
 * load_elf - load an ELF file into the current address space
 */
int
load_elf(struct file *f, char **argvp, char **envp)
{
    int valid, pheaders_sz, i;
    char magic[4];
    elf_header_t header;
    elf_program_header_t *ph;
    elf_section_header_t *sh;

    /* validate ELF file */
    file_seek(f, 0);
    f->fops->read(f, magic, 4);

    valid = magic[0] == 0x7F &&
           magic[1] == 'E'  &&
           magic[2] == 'L'  &&
           magic[3] == 'F';
    if (!valid)
        return -ENOEXEC;

    /* read in the header */
    file_seek(f, 0);
    f->fops->read(f, &header, sizeof(header));

    /* check if executable file */
    if (header.e_type != 2)
        return -ENOEXEC;

    pheaders_sz = sizeof(elf_program_header_t) * header.e_phnum;

    ph = malloc(pheaders_sz);
    if (!ph)
        return -ENOMEM;

    file_seek(f, header.e_phoff);
    f->fops->read(f, ph, pheaders_sz);

    for (i = 0; i < header.e_phnum; i ++, ph ++) {
        switch (ph->p_type) {
        case 0: /* NULL */
            break;
        case 1:; /* LOAD */
            /*printk("LOAD: offset 0x%x vaddr 0x%x"
                    "paddr 0x%x filesz 0x%x memsz 0x%x\n",
                        ph->p_offset, ph->p_vaddr, ph->p_paddr,
                        ph->p_filesz, ph->p_memsz);*/
            for(size_t i = 0; i < ph->p_memsz; i += 0x1000) {
                uintptr_t page = palloc_get_page();
#define PF_W 2
                map_page_curr(page, ph->p_vaddr + i, 1);
            }
            __flush_tlb();

            char *buf = malloc(ph->p_memsz);
            memset(buf, 0, ph->p_memsz);

            file_seek(f, ph->p_offset);
            f->fops->read(f, buf, ph->p_filesz);

            memcpy((void *) ph->p_vaddr, buf, ph->p_memsz);
            /* mark the pages RO */
            for(size_t i = ph->p_vaddr; i < ph->p_vaddr + ph->p_memsz; i += 0x1000) {
                if (!(ph->p_flags & PF_W)) {
                    page_t *p = get_page_from_curr(i);
                    if (p)
                        pte_mark_read_only(p);
                }
            }
            free(buf);
            break;
        default:
            break;
        }
    }

#ifdef CONFIG_ELF_DO_SECTIONS
    sh = malloc(header.e_shentsize * header.e_shnum);
    if (!sh) {
        free(ph);
        return -ENOMEM;
    }

    file_seek(f, header.e_shoff);
    f->fops->read(f, sh, sizeof(elf_section_header_t) * header.e_shnum);

    char *strtab = NULL;

    // find the string table
    elf_section_header_t *_sh = &sh[header.e_shstrndx];
    //printk("header.e_shoff = %d header.e_shstrndx = %d\n", header.e_shoff, header.e_shstrndx);

    if (_sh->sh_type == SHT_STRTAB) {
        if (strtab) {
            printk("FATAL: overwriting string table\n");
            return -EINVAL;
        }
        strtab = malloc(_sh->sh_size);
        if (!strtab)
            return -ENOMEM;
        //printk("SHSTRTAB: reading off 0x%x sz 0x%x\n", _sh->sh_offset, _sh->sh_size);
        file_seek(f, _sh->sh_offset);
        f->fops->read(f, strtab, _sh->sh_size);
    } 

    if (strtab == NULL)
        goto finish;

    // parse section headers
    for (i = 0; i < header.e_shnum; i ++){
        char *name = &strtab[sh[i].sh_name];
#ifdef CONFIG_ELF_SECTION_DEBUG
        printk("%d Section %s addr %x off %x size %x\n", i, name,
                    sh[i].sh_addr, sh[i].sh_offset, sh[i].sh_size);
#endif
        if (sh[i].sh_type == SHT_NOBITS &&
                sh[i].sh_flags & SHF_ALLOC &&
                sh[i].sh_flags & SHF_WRITE)
        {
            memset((void *) sh[i].sh_addr, 0, sh[i].sh_size);
        }
    }
#endif

finish:
    current_task->bstate.entry = header.e_entry;
    current_task->bstate.argvp = argvp;
    current_task->bstate.envp = envp;
    return 0;
}

int
exec_elf()
{
    uint32_t temp_stack = (uint32_t) na_malloc(4096, 4096);
    if (!temp_stack)
        return -ENOMEM;
    current_task->bstate.switch_stack = (void *) temp_stack;

    DISABLE_IRQ();
    memset((void *) temp_stack, 0, sizeof(temp_stack));
    asm volatile("movl %%ebx, %%esp; movl %%ebx, %%ebp; jmp do_exec_elf"
            ::"a"(&current_task->bstate),"b"(temp_stack + sizeof(temp_stack)));
}

void
do_args_stack(uint32_t *stackptr, char **argvp, char **envp)
{
    int envc, argc, i;
    uint32_t stack = *stackptr;

    /* count the env vars */
    for (envc = 0; envp[envc] != NULL; envc ++)
        ;

    /* count the arg vars */
    for (argc = 0; argvp[argc] != NULL; argc ++)
        ;

    //printk("this has %d env vars and %d args\n", envc, argc);

    /* push the env vars */
    for (i = envc - 1; i >= 0; i --) {
        char *curr = envp[i];
        stack -= strlen(curr) + 1;
        //printk("pushing curr \"%s\" to [0x%x - 0x%x]\n", curr, stack, stack + strlen(curr) + 1);
        memcpy((void *) stack, curr, strlen(curr) + 1);
        envp[i] = (char *) stack;
    }

    /* align to 4 bytes */
    while (stack % 4 != 0)
        *(uint8_t *)(-- stack) = 0;

    /* push the args */
    for (i = argc - 1; i >= 0; i --) {
        char *curr = argvp[i];
        stack -= strlen(curr) + 1;
        //printk("pushing curr \"%s\" to [0x%x - 0x%x]\n", curr, stack, stack + strlen(curr) + 1);
        memcpy((void *) stack, curr, strlen(curr) + 1);
        argvp[i] = (char *) stack;
    }

    /* align again to 4 bytes */
    while (stack % 4 != 0)
        *(uint8_t *)(-- stack) = 0;

    /* push the env locs */
    *(uint32_t *)(stack -= 4) = NULL;
    for (i = envc - 1; i >= 0; i --) {
        //printk("pushing value 0x%x (ct. \"%s\") to 0x%x\n", envp[i], envp[i], stack);
        *(uint32_t *)(stack -= 4) = (uint32_t) envp[i];
    }
    envp = (char **) stack;
    //printk("envp is now 0x%x\n", envp);

    /* push the arg locs */
    *(uint32_t *)(stack -= 4) = NULL;
    for (i = argc - 1; i >= 0; i --) {
        //printk("pushing value 0x%x (ct. \"%s\") to 0x%x\n", argvp[i], argvp[i], stack);
        *(uint32_t *)(stack -= 4) = (uint32_t) argvp[i];
    }
    argvp = (char **) stack;
    //printk("argvp is now 0x%x\n", argvp);

    /* push envp */
    *(uint32_t *)(stack -= 4) = (uint32_t) envp;

    /* push argvp */
    *(uint32_t *)(stack -= 4) = (uint32_t) argvp;

    /* push argc */
    *(uint32_t *)(stack -= 4) = argc;

    /* push ret addr */
    *(uint32_t *)(stack -= 4) = 0xBADC0FEE;

    *stackptr = (uint32_t) stack;
}

/* POINT OF NO RETURN */
void
do_exec_elf(void)
{
    struct bin_state *bs;
    asm volatile("movl %%eax, %0":"=r"(bs));
    uint32_t stack = VIRT_BASE;

    uint32_t p = palloc_get_page();
    map_page_curr(p, VIRT_BASE - 4096, 1);
    __flush_tlb();

    memset((void *) VIRT_BASE - 0x1000, 0, 0x1000);

    do_args_stack(&stack, bs->argvp, bs->envp);

    asm volatile ( "movl %%eax, %%esp;"
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
                   "iretl;"
                    ::"b"(bs->entry),"a"(stack));

    panic("impossible occured\n");
}
