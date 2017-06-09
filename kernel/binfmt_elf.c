#include <levos/kernel.h>
#include <levos/elf.h>
#include <levos/fs.h>
#include <levos/errno.h>
#include <levos/page.h>
#include <levos/task.h>
#include <levos/palloc.h>
#include <levos/string.h>

int
elf_probe(struct file *f)
{
    int valid;
    char magic[4];

    file_seek(f, 0);
    f->fops->read(f, magic, 4);

    valid = magic[0] == 0x7F &&
        magic[1] == 'E'  &&
        magic[2] == 'L'  &&
        magic[3] == 'F';
    if (!valid)
        return -ENOEXEC;

    return 0;
}

/*
 * load_elf - load an ELF file into the current address space
 */
int
load_elf(struct file *f, char **argvp, char **envp)
{
    int valid, pheaders_sz, i;
    char magic[4];
    elf_header_t header;
    elf_program_header_t *ph, *base_ph;
    elf_section_header_t *sh;
    uintptr_t last_page = 0;

    current_task->comm = strdup(f->full_path);

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

    base_ph = ph = malloc(pheaders_sz);
    if (!ph)
        return -ENOMEM;

    file_seek(f, header.e_phoff);
    f->fops->read(f, ph, pheaders_sz);

    for (i = 0; i < header.e_phnum; i ++, ph ++) {
        switch (ph->p_type) {
        case 0: /* NULL */
            break;
        case 1:; /* LOAD */
            uint32_t pg_offs = ph->p_vaddr % 4096;
            uint32_t file_page = PG_RND_DOWN(ph->p_offset);
            uint32_t mem_page = PG_RND_DOWN(ph->p_vaddr);
            uint32_t mem_end, read_bytes, zero_bytes;

            if (ph->p_filesz > 0) {
                read_bytes = pg_offs + ph->p_filesz;
                zero_bytes = (PG_RND_UP(pg_offs + ph->p_memsz)
                                - read_bytes);
            } else {
                read_bytes = 0;
                zero_bytes = PG_RND_UP(pg_offs + ph->p_memsz);
            }

            mem_end = mem_page + read_bytes + zero_bytes;
#define PF_W 2
            int writeable = ph->p_flags & PF_W;

#if 1
            //printk("total size 0x%x\n", t_size);
            struct vm_area *vma
                = vm_area_create_insert_curr(mem_page, pg_offs,
                                             mem_end,
                                             writeable ? VMA_WRITEABLE : 0);
            if (!vma) {
                free(base_ph);
                printk("ELF: CRITICAL: failed to create a VMA\n");
                return -EINVAL;
            }

            vma_set_mapping(vma, f, file_page, read_bytes);

            if (mem_end > last_page)
                last_page = mem_end;

            /*printk("LOAD: offset 0x%x vaddr 0x%x "
                    "paddr 0x%x filesz 0x%x memsz 0x%x\n",
                        ph->p_offset, ph->p_vaddr, ph->p_paddr,
                        ph->p_filesz, ph->p_memsz);*/
#else
            for(size_t i = 0; i < ph->p_memsz; i += 0x1000) {
                uintptr_t page = palloc_get_page();
#define PF_W 2
                map_page_curr(page, ph->p_vaddr + i, 1);
                if (ph->p_vaddr + i > last_page)
                    last_page = ph->p_vaddr + i;
            }
            __flush_tlb();

            file_seek(f, ph->p_offset);
            f->fops->read(f, ph->p_vaddr, ph->p_filesz);

            memset(ph->p_vaddr + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);

            //memcpy((void *) ph->p_vaddr, buf, ph->p_memsz);
            /* mark the pages RO */
            for(size_t i = ph->p_vaddr; i < ph->p_vaddr + ph->p_memsz; i += 0x1000) {
                if (!(ph->p_flags & PF_W)) {
                    page_t *p = get_page_from_curr(PG_RND_DOWN(i));
                    if (p && 0) /* XXX: ummm, something is broken */
                        pte_mark_read_only(p);
                }
            }
            //free(buf);
#endif
            break;
        default:
            break;
        }
    }

    //vma_dump(current_task);

#ifdef CONFIG_ELF_DO_SECTIONS
    sh = malloc(header.e_shentsize * header.e_shnum);
    if (!sh) {
        free(base_ph);
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
    free(base_ph);
    current_task->bstate.entry = header.e_entry;
    current_task->bstate.argvp = argvp;
    current_task->bstate.envp = envp;
    last_page = PG_RND_UP(last_page);
    //printk("<<<<<<<>>>>>>>>>>>>> ELF LAST PAGE 0x%x\n", last_page);
    current_task->bstate.actual_brk = last_page ;//+ 0x1000 * 1000;
    current_task->bstate.logical_brk = current_task->bstate.actual_brk;
    current_task->bstate.base_brk = current_task->bstate.actual_brk;
    current_task->bstate.brk_vma =
        vm_area_create_insert(last_page, 0, last_page + 128 * 1024 * 1024,
                current_task, VMA_WRITEABLE | VMA_ANONYMOUS | VMA_RESERVED);
    return 0;
}

int
exec_elf()
{
    uint32_t temp_stack = (uint32_t) malloc(4096);
    if (!temp_stack)
        return -ENOMEM;
    current_task->bstate.switch_stack = (void *) temp_stack;
    //printk("current switch stack %d: 0x%x\n", current_task->pid, temp_stack);

    //printk("switching stack...\n");

    DISABLE_IRQ();
    memset((void *) temp_stack, 0, 4096);
    asm volatile("movl %%ebx, %%esp; movl %%ebx, %%ebp; jmp do_exec_elf"
            ::"a"(&current_task->bstate),"b"(temp_stack + 4096));
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
    void *f0, *f1, *f2, *f3;

    __flush_tlb();

    vma_try_prefault(current_task, VIRT_BASE - 0x1000, 0x1000);
    /*uint32_t p = palloc_get_page();
    map_page_curr(p, VIRT_BASE - 4096, 1);
    __flush_tlb();*/

    //memset((void *) VIRT_BASE - 0x1000, 0, 0x1000);

    f0 = bs->argvp[0];
    f1 = bs->argvp;
    f2 = bs->envp[0];
    f3 = bs->envp;

    do_args_stack(&stack, bs->argvp, bs->envp);

    free(f0);
    free(f1);
    free(f2);
    free(f3);

    __flush_tlb();

    //printk("start of binary is at 0x%x\n", bs->entry);

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
