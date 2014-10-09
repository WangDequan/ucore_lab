#ifndef __KERN_SYNC_CONDITION_H__
#define __KERN_SYNC_CONDITION_H__

#include <list.h>
#include <types.h>
#include <proc.h>
#include <error.h>
#include <wait.h>
#include <slab.h>
#include <sync.h>
#include <sched.h>
#include <klock.h>


typedef struct {
    	int numWaiting;
	int valid;
	wait_queue_t wait_queue;
} condition_t;

#define cdtid2cdt(cdt_id)                       \
    ((condition_t *)((uintptr_t)(cdt_id) + KERNBASE))

#define cdt2cdtid(cdt)                          \
    ((cdt_t)((uintptr_t)(cdt) - KERNBASE))


void
condition_value_init(condition_t *cdt) {
	cdt->numWaiting=0;
	cdt->valid=1;
       	wait_queue_init(&(cdt->wait_queue));	
}

int 
condition_init(){
	condition_t *cdt; 
	if ((cdt = kmalloc(sizeof(condition_t))) != NULL) {
		condition_value_init( cdt );    
    	}
   	if (cdt != NULL) {
       		return cdt2cdtid(cdt);
    	}
	return -E_INVAL;
}

int
condition_free(cdt_t cdt_id) {    
    	condition_t *cdt = cdtid2cdt(cdt_id);
    	int ret = -E_INVAL;
    	if (cdt != NULL) {
        	bool intr_flag;
        	local_intr_save(intr_flag);
        	{
            		cdt->valid = 0, ret = 0;
            		wakeup_queue(&(cdt->wait_queue), WT_INTERRUPTED, 1);
	     		kfree(cdt);
        	}
        	local_intr_restore(intr_flag);
    	}
	return ret;
} 


/*
 * Function condition_wait is used to block the current process and add the newly formed wait struct 
 * to the waitqueue of the condition variable. The value of numWaiting of the condition should
 * be modified too. The waitstate of the process should be updated and the function should also
 * run the function schedule() to reschedule a new process. The sleeping process should release
 * the klock in order to ensure the concurrence of the processes.
 *
 * You may find help in the function __down in /kern/sem.c
 *
 */

int 
condition_wait(cdt_t cdt_id, klock_t kl_id){

	condition_t *cdt = cdtid2cdt(cdt_id);
    	bool intr_flag;
	//"LAB5: "
	local_intr_save(intr_flag);
	cdt->numWaiting++;
	wait_t w;
	wait_t *wait = &w;
	// 将进程添加到等待队列中
	wait_current_set(&(cdt->wait_queue), wait, WT_KSEM);
	// 释放目前获得的内核锁
	sys_unlock(kl_id);
	local_intr_restore(intr_flag);
	// 运行调度器
	schedule();
	return 0;
}

/*
 * condition_signal is used to wakeup the first process wating on the waitqueue of condition
 * variable. In addtion, the value of numWaitiing should be updated and the struct wait 
 * should be remove from the waitqueue.
 *
 * You can find help in the function __up in /kern/sem.c
 *
 */

int 
condition_signal(cdt_t cdt_id){
    	condition_t *cdt = cdtid2cdt(cdt_id);
	//"LAB5: "
	bool intr_flag;
	local_intr_save(intr_flag);
	if (cdt->numWaiting > 0) {
		wakeup_first(&(cdt->wait_queue), WT_INTERRUPTED, 1);
		cdt->numWaiting--;
	}
	local_intr_restore(intr_flag);
	return 0;
}

#endif /* !__KERN_SYNC_CONDITION_H__ */
