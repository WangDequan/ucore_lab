#include <proc.h>
#include <slab.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <swap.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
intro:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc, 
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:
                                            
 PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  + 
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep 
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit   
SYS_getpid      : get the process's pid

*/

// the process set's list
list_entry_t proc_list;
// the process set's mm's list
list_entry_t proc_mm_list;

#define HASH_SHIFT          10
#define HASH_LIST_SIZE      (1 << HASH_SHIFT)
#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;
// swap daemon proc
struct proc_struct *kswapd = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// alloc_proc - create a proc struct and init fields
/*
 * alloc_proc Allocate new process structure
 * You should allocate a new structure and initialize it
 * Take care of the 'cr3' and 'context' members
 *	[ hint: boot_cr3 ]
 * Remember to init 'thread_group' with list_init()
 */ 
static struct proc_struct *
alloc_proc(void) {
	/* LAB3   */
	struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
	if (proc != NULL) {
		proc->state = PROC_UNINIT;
		proc->pid = -1;
	    proc->runs = 0;
	    proc->kstack = 0;
	    proc->need_resched = 0;
	    proc->parent = NULL;
	    proc->mm = NULL;
		memset(&proc->context, 0, sizeof(struct context));
	    proc->tf = NULL;
	    proc->cr3 = boot_cr3;
	    proc->flags = 0;
	 	set_proc_name(proc, "");
		list_init(&proc->list_link);
		list_init(&proc->hash_link);
		proc->exit_code = 0;
    	proc->wait_state = 0;
    	proc->cptr = NULL;
		proc->yptr = NULL;
		proc->optr = NULL;
		list_init(&proc->thread_group);
	}
	return proc;
}

// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name) {
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc) {
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// set_links - set the relation links of process
static void
set_links(struct proc_struct *proc) {
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL) {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process ++;
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc) {
    list_del(&(proc->list_link));
    if (proc->optr != NULL) {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL) {
        proc->yptr->optr = proc->optr;
    }
    else {
       proc->parent->cptr = proc->optr;
    }
    nr_process --;
}

// alloc a unique pid for process
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++ last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list) {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                if (++ last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}

// make process running on cpu
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag);
        {
            current = proc;
            load_esp0(next->kstack + KSTACKSIZE);
            lcr3(next->cr3);
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}

static void
forkret(void) {
    forkrets(current->tf);
}

static void
hash_proc(struct proc_struct *proc) {
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

static void
unhash_proc(struct proc_struct *proc) {
    list_del(&(proc->hash_link));
}

struct proc_struct *
find_proc(int pid) {
    if (0 < pid && pid < MAX_PID) {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list) {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid) {
                return proc;
            }
        }
    }
    return NULL;
}

int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    struct trapframe tf_struct;
    memset(&tf_struct, 0, sizeof(struct trapframe));
    tf_struct.tf_cs = KERNEL_CS;
    tf_struct.tf_ds = tf_struct.tf_es = tf_struct.tf_ss = KERNEL_DS;
    tf_struct.tf_regs.reg_ebx = (uint32_t)fn;
    tf_struct.tf_regs.reg_edx = (uint32_t)arg;
    tf_struct.tf_eip = (uint32_t)kernel_thread_entry;
    return do_fork(clone_flags | CLONE_VM, 0, &tf_struct);
}

static int
setup_kstack(struct proc_struct *proc) {
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL) {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

static void
put_kstack(struct proc_struct *proc) {
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

static int
setup_pgdir(struct mm_struct *mm) {
    struct Page *page;
    if ((page = alloc_page()) == NULL) {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir, PGSIZE);
    pgdir[PDX(VPT)] = PADDR(pgdir) | PTE_P | PTE_W;
    mm->pgdir = pgdir;
    return 0;
}

static void
put_pgdir(struct mm_struct *mm) {
    free_page(kva2page(mm->pgdir));
}

static void
de_thread(struct proc_struct *proc) {
    if (!list_empty(&(proc->thread_group))) {
        bool intr_flag;
        local_intr_save(intr_flag);
        {
            list_del_init(&(proc->thread_group));
        }
        local_intr_restore(intr_flag);
    }
}

static struct proc_struct *
next_thread(struct proc_struct *proc) {
    return le2proc(list_next(&(proc->thread_group)), thread_group);
}

static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc) {
    struct mm_struct *mm, *oldmm = current->mm;

    /* current is a kernel thread */
    if (oldmm == NULL) {
        return 0;
    }
    if (clone_flags & CLONE_VM) {
        mm = oldmm;
        goto good_mm;
    }

    int ret = -E_NO_MEM;
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }

    lock_mm(oldmm);
    {
        ret = dup_mmap(mm, oldmm);
    }
    unlock_mm(oldmm);

    if (ret != 0) {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    if (mm != oldmm) {
        mm->brk_start = oldmm->brk_start;
        mm->brk = oldmm->brk;
        bool intr_flag;
        local_intr_save(intr_flag);
        {
            list_add(&(proc_mm_list), &(mm->proc_mm_link));
        }
        local_intr_restore(intr_flag);
    }
    mm_count_inc(mm);
    proc->mm = mm;
    proc->cr3 = PADDR(mm->pgdir);
    return 0;
bad_dup_cleanup_mmap:
    exit_mmap(mm);
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}

static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    *(proc->tf) = *tf;
    proc->tf->tf_regs.reg_eax = 0;
    proc->tf->tf_esp = esp;
    proc->tf->tf_eflags |= FL_IF;

    proc->context.eip = (uintptr_t)forkret;
    proc->context.esp = (uintptr_t)(proc->tf);
}

// do_fork - parent process for a new child process
//    1. call alloc_proc to allocate a proc_struct
//    2. call setup_kstack to allocate a kernel stack for child process
//    3. call copy_mm to dup OR share mm according clone_flag
//    4. call wakup_proc to make the new child process RUNNABLE 
/*
 * do_fork fork syscall
 * You need to:
 * a. Check if there are too many processes ( see MAX_PROCESS )
 *		[ hint: fail out 1 ]
 * b. alloc_proc() to get a new proc_struct
 *		[ hint: fail out 1 ]
 * c. set current as its parent
 * d. setup its kstack with setup_kstack()
 *		[ hint: fail out 2 ]
 * e. copy memory management info with copy_mm()
 *		[ hint: fail out 3 ]
 * f. copy context of old process to new one with copy_thread() 
 *		[ hint: stack = esp ]
 * g. save interrupt flag
 * h. add new process to process list, add process count
 *		[ hint: set process pid to get_pid() ]
 *		[ hint: setup process hash info with hash_proc() ]
 *		[ hint: add to proccess list with set_links() ]
 *		[ hint: if we need to CLONE_THREAD ]
 *		[ hint:   then add my 'thread_group' to new proc's ]
 *		[ hint:   with list_add_before() ]
 * i. restore interrupt flag
 * j. wake up new proc with wakeup_proc()
 * k. return new proc's pid
 */
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
	/* LAB3   */
	if (nr_process > MAX_PROCESS)
		goto fork_out;
	if ((proc = alloc_proc()) == NULL)
		goto fork_out;
	proc->parent = current;
	if (setup_kstack(proc) != 0)
		goto bad_fork_cleanup_proc;
	if (copy_mm(clone_flags, proc) != 0)
		goto bad_fork_cleanup_kstack;
	copy_thread(proc, stack, tf);

	uint32_t flag;
	local_intr_save(flag);
	proc->pid = get_pid();
	hash_proc(proc);
	set_links(proc);
	if (clone_flags & CLONE_THREAD) {
		list_add_before(&current->thread_group, &proc->thread_group);
	}
	local_intr_restore(flag);
	
	wakeup_proc(proc);
	ret = proc->pid;
	
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
int
do_exit(int error_code) {
    if (current == idleproc) {
        panic("idleproc exit.\n");
    }
    if (current == initproc) {
        panic("initproc exit.\n");
    }

    struct mm_struct *mm = current->mm;
    if (mm != NULL) {
        lcr3(boot_cr3);
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pgdir(mm);
            bool intr_flag;
            local_intr_save(intr_flag);
            {
                list_del(&(mm->proc_mm_link));
            }
            local_intr_restore(intr_flag);
            mm_destroy(mm);
        }
        current->mm = NULL;
    }
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;

    bool intr_flag;
    struct proc_struct *proc, *parent;
    local_intr_save(intr_flag);
    {
        proc = parent = current->parent;
        do {
            if (proc->wait_state == WT_CHILD) {
                wakeup_proc(proc);
            }
            proc = next_thread(proc);
        } while (proc != parent);

        if ((parent = next_thread(current)) == current) {
            parent = initproc;
        }
        de_thread(current);
        while (current->cptr != NULL) {
            proc = current->cptr;
            current->cptr = proc->optr;

            proc->yptr = NULL;
            if ((proc->optr = parent->cptr) != NULL) {
                parent->cptr->yptr = proc;
            }
            proc->parent = parent;
            parent->cptr = proc;
            if (proc->state == PROC_ZOMBIE) {
                if (parent->wait_state == WT_CHILD) {
                    wakeup_proc(parent);
                }
            }
        }
    }
    local_intr_restore(intr_flag);

    schedule();
    panic("do_exit will not return!! %d.\n", current->pid);
}

// load_icode -  called by sys_exec-->do_execve
// 1. create a new mm for current process
// 2. create a new PDT, and mm->pgdir= kernel virtual addr of PDT
// 3. copy TEXT/DATA/BSS parts in binary to memory space of process
// 4. call mm_map to setup user stack, and put parameters into user stack
// 5. setup trapframe for user environment	
static int
load_icode(unsigned char *binary, size_t size) {
    if (current->mm != NULL) {
        panic("load_icode: current->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;

    struct mm_struct *mm;
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }

    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }

    mm->brk_start = 0;

    struct Page *page;

    struct elfhdr *elf = (struct elfhdr *)binary;
    struct proghdr *ph = (struct proghdr *)(binary + elf->e_phoff);
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    uint32_t vm_flags, perm;
    struct proghdr *ph_end = ph + elf->e_phnum;
    for (; ph < ph_end; ph ++) {
        if (ph->p_type != ELF_PT_LOAD) {
            continue ;
        }
        if (ph->p_filesz > ph->p_memsz) {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_filesz == 0) {
            continue ;
        }
        vm_flags = 0, perm = PTE_U;
        if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;
        if (vm_flags & VM_WRITE) perm |= PTE_W;

        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
        }
        if (mm->brk_start < ph->p_va + ph->p_memsz) {
            mm->brk_start = ph->p_va + ph->p_memsz;
        }

        unsigned char *from = binary + ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

        ret = -E_NO_MEM;

        end = ph->p_va + ph->p_filesz;
        while (start < end) {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            memcpy(page2kva(page) + off, from, size);
            start += size, from += size;
        }

        end = ph->p_va + ph->p_memsz;

        if (start < la) {
            /* ph->p_memsz == ph->p_filesz */
            if (start == end) {
                continue ;
            }
            off = start + PGSIZE - la, size = PGSIZE - off;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }

        while (start < end) {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }

    mm->brk_start = mm->brk = ROUNDUP(mm->brk_start, PGSIZE);

    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }

    bool intr_flag;
    local_intr_save(intr_flag);
    {
        list_add(&(proc_mm_list), &(mm->proc_mm_link));
    }
    local_intr_restore(intr_flag);
    mm_count_inc(mm);
    current->mm = mm;
    current->cr3 = PADDR(mm->pgdir);
    lcr3(PADDR(mm->pgdir));

    struct trapframe *tf = current->tf;
    memset(tf, 0, sizeof(struct trapframe));
    tf->tf_cs = USER_CS;
    tf->tf_ds = USER_DS;
    tf->tf_es = USER_DS;
    tf->tf_ss = USER_DS;
    tf->tf_esp = USTACKTOP;
    tf->tf_eip = elf->e_entry;
    tf->tf_eflags = FL_IF;
    ret = 0;
out:
    return ret;
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
}

// do_execve - call exit_mmap(mm)&pug_pgdir(mm) to reclaim memory space of current process
//           - call load_icode to setup new memory space accroding binary prog.
int
do_execve(const char *name, size_t len, unsigned char *binary, size_t size) {
    struct mm_struct *mm = current->mm;

    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    if (len > PROC_NAME_LEN) {
        len = PROC_NAME_LEN;
    }

    lock_mm(mm);
    {
        if (!copy_from_user(mm, local_name, name, len, 0)) {
            unlock_mm(mm);
            return -E_INVAL;
        }
    }
    unlock_mm(mm);

    if (mm != NULL) {
        lcr3(boot_cr3);
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pgdir(mm);
            bool intr_flag;
            local_intr_save(intr_flag);
            {
                list_del(&(mm->proc_mm_link));
            }
            local_intr_restore(intr_flag);
            mm_destroy(mm);
        }
        current->mm = NULL;
    }

    int ret;
    if ((ret = load_icode(binary, size)) != 0) {
        goto execve_exit;
    }
    de_thread(current);
    set_proc_name(current, local_name);
    return 0;

execve_exit:
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}

int
do_yield(void) {
    current->need_resched = 1;
    return 0;
}

int
do_wait(int pid, int *code_store) {
    struct mm_struct *mm = current->mm;
    if (code_store != NULL) {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1)) {
            return -E_INVAL;
        }
    }

    struct proc_struct *proc, *cproc;
    bool intr_flag, haskid;
repeat:
    cproc = current;
    haskid = 0;
    if (pid != 0) {
        proc = find_proc(pid);
        if (proc != NULL) {
            do {
                if (proc->parent == cproc) {
                    haskid = 1;
                    if (proc->state == PROC_ZOMBIE) {
                        goto found;
                    }
                    break;
                }
                cproc = next_thread(cproc);
            } while (cproc != current);
        }
    }
    else {
        do {
            proc = cproc->cptr;
            for (; proc != NULL; proc = proc->optr) {
                haskid = 1;
                if (proc->state == PROC_ZOMBIE) {
                    goto found;
                }
            }
            cproc = next_thread(cproc);
        } while (cproc != current);
    }
    if (haskid) {
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD;
        schedule();
        if (current->flags & PF_EXITING) {
            do_exit(-E_KILLED);
        }
        goto repeat;
    }
    return -E_BAD_PROC;

found:
    if (proc == idleproc || proc == initproc) {
        panic("wait idleproc or initproc.\n");
    }
    int exit_code = proc->exit_code;
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);
        remove_links(proc);
    }
    local_intr_restore(intr_flag);
    put_kstack(proc);
    kfree(proc);

    int ret = 0;
    if (code_store != NULL) {
        lock_mm(mm);
        {
            if (!copy_to_user(mm, code_store, &exit_code, sizeof(int))) {
                ret = -E_INVAL;
            }
        }
        unlock_mm(mm);
    }
    return ret;
}

/* do_kill kill system call
 * kill a process
 * Just find the process with find_proc()
 * And then set the EXITING flag
 * If the process is INTERRUPTED
 * Then wakeup_proc()
 */
int
do_kill(int pid) {
	/* LAB3   */
	struct proc_struct *proc = find_proc(pid);
	if (proc != NULL) {
		proc->flags |= PF_EXITING;
		if (proc->wait_state & WT_INTERRUPTED)
			wakeup_proc(proc);
		return 0;
	}
    return -E_INVAL;
}

int
do_brk(uintptr_t *brk_store) {
    struct mm_struct *mm = current->mm;
    if (mm == NULL) {
        panic("kernel thread call sys_brk!!.\n");
    }
    if (brk_store == NULL) {
        return -E_INVAL;
    }

    uintptr_t brk;

    lock_mm(mm);
    if (!copy_from_user(mm, &brk, brk_store, sizeof(uintptr_t), 1)) {
        unlock_mm(mm);
        return -E_INVAL;
    }

    if (brk < mm->brk_start) {
        goto out_unlock;
    }
    uintptr_t newbrk = ROUNDUP(brk, PGSIZE), oldbrk = mm->brk;
    assert(oldbrk % PGSIZE == 0);
    if (newbrk == oldbrk) {
        goto out_unlock;
    }
    if (newbrk < oldbrk) {
        if (mm_unmap(mm, newbrk, oldbrk - newbrk) != 0) {
            goto out_unlock;
        }
    }
    else {
        if (find_vma_intersection(mm, oldbrk, newbrk + PGSIZE) != NULL) {
            goto out_unlock;
        }
        if (mm_brk(mm, oldbrk, newbrk - oldbrk) != 0) {
            goto out_unlock;
        }
    }
    mm->brk = newbrk;
out_unlock:
    *brk_store = mm->brk;
    unlock_mm(mm);
    return 0;
}

/* do_sleep sleep system call
 * You should:
 * a. Save interrupt flags
 * b. Create a timer_t with timer_init
 * c. Set my state to SLEEPING
 * d. Say that I'm waiting for a TIMER
 * e. Add the timer to the system timer list with add_timer()
 * f. Restore interrupt flags
 * g. Go to sleep with schedule()
 * h. When it returns, you know it's time to wake up
 * i. Free the timer with del_timer()
 */
int
do_sleep(unsigned int time) {
	/* LAB3   */
	if (time <= 0)
		return 0;
	uint32_t flag;
	
	local_intr_save(flag);	
	timer_t t;
	timer_t *timer = timer_init(&t, current, time);
	current->state = PROC_SLEEPING;
	current->wait_state = WT_TIMER;
	add_timer(timer);		
	local_intr_restore(flag);

	schedule();
	del_timer(timer);
	//kfree(timer);
    return 0;
}

int
do_mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    struct mm_struct *mm = current->mm;
    if (mm == NULL) {
        panic("kernel thread call mmap!!.\n");
    }
    if (addr_store == NULL || len == 0) {
        return -E_INVAL;
    }

    int ret = -E_INVAL;

    uintptr_t addr;

    lock_mm(mm);
    if (!copy_from_user(mm, &addr, addr_store, sizeof(uintptr_t), 1)) {
        goto out_unlock;
    }

    uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
    addr = start, len = end - start;

    uint32_t vm_flags = VM_READ;
    if (mmap_flags & MMAP_WRITE) vm_flags |= VM_WRITE;
    if (mmap_flags & MMAP_STACK) vm_flags |= VM_STACK;

    ret = -E_NO_MEM;
    if (addr == 0) {
        if ((addr = get_unmapped_area(mm, len)) == 0) {
            goto out_unlock;
        }
    }
    if ((ret = mm_map(mm, addr, len, vm_flags, NULL)) == 0) {
        *addr_store = addr;
    }
out_unlock:
    unlock_mm(mm);
    return ret;
}

int
do_munmap(uintptr_t addr, size_t len) {
    struct mm_struct *mm = current->mm;
    if (mm == NULL) {
        panic("kernel thread call munmap!!.\n");
    }
    if (len == 0) {
        return -E_INVAL;
    }
    int ret;
    lock_mm(mm);
    {
        ret = mm_unmap(mm, addr, len);
    }
    unlock_mm(mm);
    return ret;
}

int
do_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    struct mm_struct *mm = current->mm;
    if (mm == NULL) {
        panic("kernel thread call mmap!!.\n");
    }
    if (addr_store == NULL || len == 0) {
        return -E_INVAL;
    }

    int ret = -E_INVAL;

    uintptr_t addr;

    lock_mm(mm);
    if (!copy_from_user(mm, &addr, addr_store, sizeof(uintptr_t), 1)) {
        goto out_unlock;
    }

    uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
    addr = start, len = end - start;

    uint32_t vm_flags = VM_READ;
    if (mmap_flags & MMAP_WRITE) vm_flags |= VM_WRITE;
    if (mmap_flags & MMAP_STACK) vm_flags |= VM_STACK;

    ret = -E_NO_MEM;
    if (addr == 0) {
        if ((addr = get_unmapped_area(mm, len)) == 0) {
            goto out_unlock;
        }
    }
    struct shmem_struct *shmem;
    if ((shmem = shmem_create(len)) == NULL) {
        goto out_unlock;
    }
    if ((ret = mm_map_shmem(mm, addr, vm_flags, shmem, NULL)) != 0) {
        assert(shmem_ref(shmem) == 0);
        shmem_destroy(shmem);
        goto out_unlock;
    }
    *addr_store = addr;
out_unlock:
    unlock_mm(mm);
    return ret;
}

static int
kernel_execve(const char *name, unsigned char *binary, size_t size) {
    int ret, len = strlen(name);
    asm volatile (
        "int %1;"
        : "=a" (ret)
        : "i" (T_SYSCALL), "0" (SYS_exec), "d" (name), "c" (len), "b" (binary), "D" (size)
        : "memory");
    return ret;
}

#define __KERNEL_EXECVE(name, binary, size) ({                          \
            cprintf("kernel_execve: pid = %d, name = \"%s\".\n",        \
                    current->pid, name);                                \
            kernel_execve(name, binary, (size_t)(size));                \
        })

#define KERNEL_EXECVE(x) ({                                             \
            extern unsigned char _binary_obj___user_##x##_out_start[],  \
                _binary_obj___user_##x##_out_size[];                    \
            __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,     \
                            _binary_obj___user_##x##_out_size);         \
        })

#define __KERNEL_EXECVE2(x, xstart, xsize) ({                           \
            extern unsigned char xstart[], xsize[];                     \
            __KERNEL_EXECVE(#x, xstart, (size_t)xsize);                 \
        })

#define KERNEL_EXECVE2(x, xstart, xsize)        __KERNEL_EXECVE2(x, xstart, xsize)

static int
user_main(void *arg) {
#ifdef TEST
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    KERNEL_EXECVE(mmaptest);
#endif
    panic("user_main execve failed.\n");
}

static int
init_main(void *arg) {
    int pid;
    if ((pid = kernel_thread(kswapd_main, NULL, 0)) <= 0) {
        panic("kswapd init failed.\n");
    }
    kswapd = find_proc(pid);
    set_proc_name(kswapd, "kswapd");

    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();

    unsigned int nr_process_store = nr_process;

    pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0) {
        panic("create user_main failed.\n");
    }

    while (do_wait(0, NULL) == 0) {
        if (nr_process_store == nr_process) {
            break;
        }
        schedule();
    }

    assert(kswapd != NULL);

    int i;
    for (i = 0; i < 10; i ++) {
        if (kswapd->wait_state == WT_TIMER) {
            wakeup_proc(kswapd);
        }
        schedule();
    }

    cprintf("all user-mode processes have quit.\n");
    assert(initproc->cptr == kswapd && initproc->yptr == NULL && initproc->optr == NULL);
    assert(kswapd->cptr == NULL && kswapd->yptr == NULL && kswapd->optr == NULL);
    assert(nr_process == 3);
    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());
    cprintf("init check memory pass.\n");
    return 0;
}

void
proc_init(void) {
    int i;

    list_init(&proc_list);
    list_init(&proc_mm_list);
    for (i = 0; i < HASH_LIST_SIZE; i ++) {
        list_init(hash_list + i);
    }

    if ((idleproc = alloc_proc()) == NULL) {
        panic("cannot alloc idleproc.\n");
    }

    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    nr_process ++;

    current = idleproc;

    int pid = kernel_thread(init_main, NULL, 0);
    if (pid <= 0) {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid);
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

void
cpu_idle(void) {
    while (1) {
        if (current->need_resched) {
            schedule();
        }
    }
}

