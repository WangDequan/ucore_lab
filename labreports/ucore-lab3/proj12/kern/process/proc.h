#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <types.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>
#include <unistd.h>

enum proc_state {
    PROC_UNINIT = 0,
    PROC_SLEEPING,
    PROC_RUNNABLE,
    PROC_ZOMBIE,
};

// Saved registers for kernel context switches.
// Don't need to save all the %fs etc. segment registers,
// because they are constant across kernel contexts.
// Save all the regular registers so we don't need to care
// which are caller save, but not the return register %eax.
// (Not saving %eax just simplifies the switching code.)
// The layout of context must match code in switch.S.
struct context {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
};

#define PROC_NAME_LEN               15
#define MAX_PROCESS                 4096
#define MAX_PID                     (MAX_PROCESS * 2)

extern list_entry_t proc_list;
extern list_entry_t proc_mm_list;

struct proc_struct {
    enum proc_state state;
    int pid;
    int runs;
    uintptr_t kstack;
    volatile bool need_resched;
    struct proc_struct *parent;
    struct mm_struct *mm;
    struct context context;
    struct trapframe *tf;
    uintptr_t cr3;
    uint32_t flags;
    char name[PROC_NAME_LEN + 1];
    list_entry_t list_link;
    list_entry_t hash_link;
    int exit_code;
    uint32_t wait_state;
    struct proc_struct *cptr, *yptr, *optr;
    list_entry_t thread_group;
};

#define PF_EXITING                  0x00000001      // getting shutdown

#define WT_CHILD                    (0x00000001 | WT_INTERRUPTED)
#define WT_TIMER                    (0x00000002 | WT_INTERRUPTED)
#define WT_KSWAPD                    0x00000003
#define WT_INTERRUPTED               0x80000000

#define le2proc(le, member)         \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;
extern struct proc_struct *kswapd;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_exit(int error_code);
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size);
int do_yield(void);
int do_wait(int pid, int *code_store);
int do_kill(int pid);
int do_brk(uintptr_t *brk_store);
int do_sleep(unsigned int time);
int do_mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int do_munmap(uintptr_t addr, size_t len);
int do_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);

#endif /* !__KERN_PROCESS_PROC_H__ */

