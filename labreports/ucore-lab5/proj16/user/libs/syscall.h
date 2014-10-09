#ifndef __USER_LIBS_SYSCALL_H__
#define __USER_LIBS_SYSCALL_H__

#include <types.h>

int sys_exit(int error_code);
int sys_fork(void);
int sys_wait(int pid, int *store);
int sys_yield(void);
int sys_sleep(unsigned int time);
int sys_kill(int pid);
size_t sys_gettime(void);
int sys_getpid(void);
int sys_brk(uintptr_t *brk_store);
int sys_mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int sys_munmap(uintptr_t addr, size_t len);
int sys_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int sys_putc(int c);
int sys_pgdir(void);

sem_t sys_sem_init(int value);
int sys_sem_post(sem_t sem_id);
int sys_sem_wait(sem_t sem_id, unsigned int timeout);
int sys_sem_free(sem_t sem_id);
int sys_sem_get_value(sem_t sem_id, int *value_store);
int sys_send_event(int pid, int event, unsigned int timeout);
int sys_recv_event(int *pid_store, int *event_store, unsigned int timeout);

cdt_t sys_cdt_init();
int sys_cdt_signal(cdt_t cdt_id);
int sys_cdt_wait(cdt_t cdt_id ,klock_t klock_id);
int sys_cdt_free(cdt_t cdt_id);

klock_t sys_klock_init();
int sys_klock_aquire(klock_t klock_id);
int sys_klock_release(klock_t klock_id);
int sys_klock_free(klock_t klock_id);


struct mboxbuf;
struct mboxinfo;

int sys_mbox_init(unsigned int max_slots);
int sys_mbox_send(int id, struct mboxbuf *buf, unsigned int timeout);
int sys_mbox_recv(int id, struct mboxbuf *buf, unsigned int timeout);
int sys_mbox_free(int id);
int sys_mbox_info(int id, struct mboxinfo *info);

#endif /* !__USER_LIBS_SYSCALL_H__ */

