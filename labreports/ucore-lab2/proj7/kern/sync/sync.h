#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <x86.h>
#include <intr.h>
#include <mmu.h>

//ȡ�ж�
static inline bool
__intr_save(void) {
	if (read_eflags() & FL_IF) {//�Ƿ�����жϱ�־
		intr_disable();//���ж�
		return 1;
	}
	return 0;
}

//�����ж�
static inline void
__intr_restore(bool flag) {
	if (flag) {
		intr_enable();//���ж�
	}
}

#define local_intr_save(x)		do { x = __intr_save(); } while (0)
#define local_intr_restore(x)	__intr_restore(x);

#endif /* !__KERN_SYNC_SYNC_H__ */

