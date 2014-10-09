#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <x86.h>
#include <intr.h>
#include <mmu.h>

//取中断
static inline bool
__intr_save(void) {
	if (read_eflags() & FL_IF) {//是否存在中断标志
		intr_disable();//关中断
		return 1;
	}
	return 0;
}

//重置中断
static inline void
__intr_restore(bool flag) {
	if (flag) {
		intr_enable();//开中断
	}
}

#define local_intr_save(x)		do { x = __intr_save(); } while (0)
#define local_intr_restore(x)	__intr_restore(x);

#endif /* !__KERN_SYNC_SYNC_H__ */

