#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include <types.h>
#include <mmu.h>
#include <memlayout.h>
#include <atomic.h>
#include <assert.h>

//类似接口，具体函数没实现，由具体的内存管理算法去实现
//如本实验中是buddy算法，则由buddy算法去实现结构体中的函数
//具体见kern\mm\buddy_pmm.c最后
struct pmm_manager {
	const char *name;
	void (*init)(void);
	void (*init_memmap)(struct Page *base, size_t n);
	struct Page *(*alloc_pages)(size_t order);
	void (*free_pages)(struct Page *base, size_t n);
	size_t (*nr_free_pages)(void);
	void (*check)(void);
};

extern const struct pmm_manager *pmm_manager;
extern pde_t *boot_pgdir;
extern uintptr_t boot_cr3;

void pmm_init(void);

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
unsigned int nr_free_pages(void);

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store);
void page_remove(pde_t *pgdir, uintptr_t la);
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);

void load_esp0(uintptr_t esp0);
void tlb_invalidate(pde_t *pgdir, uintptr_t la);
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm);

void print_pgdir(void);

/* *
 * This macro takes a kernel virtual address (an address that points above KERNBASE),
 * where the machine's maximum 256MB of physical memory is mapped and returns the
 * corresponding physical address.  It panics if you pass it a non-kernel virtual address.
 * */
#define PADDR(kva) ({													\
	uintptr_t __m_kva = (uintptr_t)(kva);								\
	if (__m_kva < KERNBASE) {											\
		panic("PADDR called with invalid kva %08lx", __m_kva);			\
	}																	\
	__m_kva - KERNBASE;													\
})

/* *
 * This macro takes a physical address and returns the corresponding kernel virtual
 * address. It panics if you pass an invalid physical address.
 * */
#define KADDR(pa) ({													\
	uintptr_t __m_pa = (pa);											\
	uint32_t __m_ppn = PPN(__m_pa);										\
	if (__m_ppn >= npage) {												\
		panic("KADDR called with invalid pa %08lx", __m_pa);			\
	}																	\
	(void*) (__m_pa + KERNBASE);										\
})

extern struct Page *pages;
extern size_t npage;

static inline ppn_t
page2ppn(struct Page *page) {
	return page - pages;
}

static inline uintptr_t
page2pa(struct Page *page) {
	return page2ppn(page) << PGSHIFT;
}

static inline struct Page *
pa2page(uintptr_t pa) {
	if (PPN(pa) >= npage) {
		panic("pa2page called with invalid pa");
	}
	return &pages[PPN(pa)];
}

static inline void *
page2kva(struct Page *page) {
	return KADDR(page2pa(page));
}

static inline struct Page *
kva2page(void *kva) {
	return pa2page(PADDR(kva));
}

static inline struct Page *
pte2page(pte_t pte) {
	if (!(pte & PTE_P)) {
		panic("pte2page called with invalid pte");
	}
	return pa2page(PTE_ADDR(pte));
}

static inline struct Page *
pde2page(pde_t pde) {
	return pa2page(PDE_ADDR(pde));
}

static inline int
page_ref(struct Page *page) {
	return atomic_read(&(page->ref));
}

static inline void
set_page_ref(struct Page *page, int val) {
	atomic_set(&(page->ref), val);
}

static inline int
page_ref_inc(struct Page *page) {
	return atomic_add_return(&(page->ref), 1);
}

static inline int
page_ref_dec(struct Page *page) {
	return atomic_sub_return(&(page->ref), 1);
}

extern char bootstack[], bootstacktop[];

#endif /* !__KERN_MM_PMM_H__ */

