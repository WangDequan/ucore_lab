#ifndef __KERN_SYNC_KLOCK_H__
#define __KERN_SYNC_KLOCK_H__

#include <types.h>
#include <atomic.h>
#include <proc.h>
#include <error.h>
#include <slab.h>
//#include <stdio.h>

#define KERNAL_INIT_LOCK	0

typedef volatile bool kernal_lock_t;

#define klockid2klock(klock_id)                       \
    ((kernal_lock_t *)((uintptr_t)(klock_id) + KERNBASE))

#define klock2klockid(klock)                          \
    ((klock_t)((uintptr_t)(klock) - KERNBASE))

static inline int
sys_lock_init() {
	kernal_lock_t *klock; 
	if ((klock = kmalloc(sizeof(kernal_lock_t))) != NULL) {		
		*klock = KERNAL_INIT_LOCK;
    	}
    	if (klock != NULL) {	
       		return klock2klockid(klock);
    	}
	return -E_INVAL;
}

static inline int
sys_lock_free(klock_t klock_id) {
	kernal_lock_t *l = klockid2klock(klock_id) ;	
    	if ( l  != NULL) {
		kfree((void *)l);	
		return 0;
    	}
	return -E_INVAL;
}

static inline bool
sys_try_lock(kernal_lock_t *l) {
	return test_and_set_bit(0, l);
}

static inline int
sys_lock(klock_t klock_id) {
    	kernal_lock_t *l = klockid2klock(klock_id) ;
    	if (sys_try_lock(l)) {
        	int step = 0;
        	do {
            		do_yield(); 
            		if (++ step == 100) {
                		step = 0;
                		do_sleep(10);
            		}
        	} while (sys_try_lock(l));
    	}
    	return 0;
}

static inline int
sys_unlock(klock_t klock_id) {
	kernal_lock_t *l = klockid2klock(klock_id) ;
	if(test_and_clear_bit(0, l)){
		return 0;
	}
	return -1;
}

#endif /* !__KERN_SYNC_KLOCK_H__ */
