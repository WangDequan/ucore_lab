#ifndef __KERN_MM_VMM_H__
#define __KERN_MM_VMM_H__

#include <types.h>
#include <list.h>
#include <memlayout.h>
#include <rb_tree.h>
#include <sync.h>

struct mm_struct;

struct vma_struct {
	struct mm_struct *vm_mm;
	uintptr_t vm_start;
	uintptr_t vm_end;
	uint32_t vm_flags;
	rb_node rb_link;
	list_entry_t list_link;
};

#define le2vma(le, member)					\
	to_struct((le), struct vma_struct, member)

#define rbn2vma(node, member)				\
	to_struct((node), struct vma_struct, member)

#define VM_READ					0x00000001
#define VM_WRITE				0x00000002
#define VM_EXEC					0x00000004

struct mm_struct {
	list_entry_t mmap_list;
	rb_tree *mmap_tree;
	struct vma_struct *mmap_cache;
	pde_t *pgdir;
	int map_count;
};

#define RB_MIN_MAP_COUNT		32

struct vma_struct *find_vma(struct mm_struct *mm, uintptr_t addr);
struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags);
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma);

struct mm_struct *mm_create(void);
void mm_destroy(struct mm_struct *mm);

void vmm_init(void);

int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr);

#endif /* !__KERN_MM_VMM_H__ */

