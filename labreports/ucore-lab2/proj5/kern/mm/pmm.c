#include <types.h>
#include <x86.h>
#include <stdio.h>
#include <string.h>
#include <mmu.h>
#include <memlayout.h>
#include <pmm.h>
#include <buddy_pmm.h>
#include <sync.h>
#include <error.h>

/* *
 * Task State Segment:
 *
 * The TSS may reside anywhere in memory. A special segment register called
 * the Task Register (TR) holds a segment selector that points a valid TSS
 * segment descriptor which resides in the GDT. Therefore, to use a TSS
 * the following must be done in function gdt_init:
 *   - create a TSS descriptor entry in GDT
 *   - add enough information to the TSS in memory as needed
 *   - load the TR register with a segment selector for that segment
 *
 * There are several fileds in TSS for specifying the new stack pointer when a
 * privilege level change happens. But only the fields SS0 and ESP0 are useful
 * in our os kernel.
 *
 * The field SS0 contains the stack segment selector for CPL = 0, and the ESP0
 * contains the new ESP value for CPL = 0. When an interrupt happens in protected
 * mode, the x86 CPU will look in the TSS for SS0 and ESP0 and load their value
 * into SS and ESP respectively.
 * */
static struct taskstate ts = {0};

// virtual address of physicall page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;

// virtual address of boot-time page directory
pde_t *boot_pgdir = NULL;
// physical address of boot-time page directory
uintptr_t boot_cr3;

// physical memory management
const struct pmm_manager *pmm_manager;

/* *
 * The page directory entry corresponding to the virtual address range
 * [VPT, VPT + PTSIZE) points to the page directory itself. Thus, the page
 * directory is treated as a page table as well as a page directory.
 *
 * One result of treating the page directory as a page table is that all PTEs
 * can be accessed though a "virtual page table" at virtual address VPT. And the
 * PTE for number n is stored in vpt[n].
 *
 * A second consequence is that the contents of the current page directory will
 * always available at a specific virtual address, to which vpd is set bellow.
 * */
pte_t * const vpt = (pte_t *)VPT; /* LAB2 PROJ5:   */ // memlayout.h 
pde_t * const vpd = (pde_t *)PGADDR(PDX(VPT), PDX(VPT), 0); /* LAB2 PROJ5:   */ // mmu.h linear address

/* *
 * Global Descriptor Table:
 *
 * The kernel and user segments are identical (except for the DPL). To load
 * the %ss register, the CPL must equal the DPL. Thus, we must duplicate the
 * segments for the user and the kernel. Defined as follows:
 *   - 0x0 :  unused (always faults -- for trapping NULL far pointers)
 *   - 0x8 :  kernel code segment
 *   - 0x10:  kernel data segment
 *   - 0x18:  user code segment
 *   - 0x20:  user data segment
 *   - 0x28:  defined for tss, initialized in gdt_init
 * */
static struct segdesc gdt[] = {
	NULL_SEG,
	[SEG_KTEXT] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_KERNEL),
	[SEG_KDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_KERNEL),
	[SEG_UTEXT] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_USER),
	[SEG_UDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_USER),
	[SEG_TSS]	= NULL_SEG,
};

static struct pseudodesc gdt_pd = {
	sizeof(gdt) - 1, (uint32_t)gdt
};

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

/* *
 * lgdt - load the global descriptor table register and reset the
 * data/code segement registers for kernel.
 * */
static inline void
lgdt(struct pseudodesc *pd) {
	asm volatile ("lgdt (%0)" :: "r" (pd));
	asm volatile ("movw %%ax, %%gs" :: "a" (USER_DS));
	asm volatile ("movw %%ax, %%fs" :: "a" (USER_DS));
	asm volatile ("movw %%ax, %%es" :: "a" (KERNEL_DS));
	asm volatile ("movw %%ax, %%ds" :: "a" (KERNEL_DS));
	asm volatile ("movw %%ax, %%ss" :: "a" (KERNEL_DS));
	// reload cs
	asm volatile ("ljmp %0, $1f\n 1:\n" :: "i" (KERNEL_CS));
}

/* *
 * load_esp0 - change the ESP0 in default task state segment,
 * so that we can use different kernel stack when we trap frame
 * user to kernel.
 * */
void
load_esp0(uintptr_t esp0) {
	ts.ts_esp0 = esp0;
}

/* gdt_init - initialize the default GDT and TSS */
static void
gdt_init(void) {
	// set boot kernel stack and default SS0
	load_esp0((uintptr_t)bootstacktop);
	ts.ts_ss0 = KERNEL_DS;

	// initialize the TSS filed of the gdt
	gdt[SEG_TSS] = SEG16(STS_T32A, (uint32_t)&ts, sizeof(ts), DPL_KERNEL);
	gdt[SEG_TSS].sd_s = 0;

	// reload all segment registers
	lgdt(&gdt_pd);

	// load the TSS
	ltr(GD_TSS);
}

static void
init_pmm_manager(void) {
	pmm_manager = &buddy_pmm_manager;
	cprintf("memory managment: %s\n", pmm_manager->name);
	pmm_manager->init();
}

static void
init_memmap(struct Page *base, size_t n) {
	pmm_manager->init_memmap(base, n);
}

struct Page *
alloc_pages(size_t n) {
	struct Page *page;
	bool intr_flag;
	local_intr_save(intr_flag);
	{
		page = pmm_manager->alloc_pages(n);
	}
	local_intr_restore(intr_flag);
	return page;
}

void
free_pages(struct Page *base, size_t n) {
	bool intr_flag;
	local_intr_save(intr_flag);
	{
		pmm_manager->free_pages(base, n);
	}
	local_intr_restore(intr_flag);
}

size_t
nr_free_pages(void) {
	size_t ret;
	bool intr_flag;
	local_intr_save(intr_flag);
	{
		ret = pmm_manager->nr_free_pages();
	}
	local_intr_restore(intr_flag);
	return ret;
}

/* pmm_init - initialize the physical memory management */
static void
page_init(void) {
	struct e820map *memmap = (struct e820map *)(0x8000 + KERNBASE);
	uint64_t maxpa = 0;	// 最大的物理内存地址maxpa

	cprintf("e820map:\n");
	int i;
	for (i = 0; i < memmap->nr_map; i ++) {
		uint64_t begin = memmap->map[i].addr, end = begin + memmap->map[i].size;
		cprintf("  memory: %08llx, [%08llx, %08llx], type = %d.\n",
				memmap->map[i].size, begin, end - 1, memmap->map[i].type);
		if (memmap->map[i].type == E820_ARM) {
			if (maxpa < end && begin < KMEMSIZE) {
				maxpa = end;
			}
		}
	}
	if (maxpa > KMEMSIZE) {
		maxpa = KMEMSIZE;
	}

	extern char end[];

	npage = maxpa / PGSIZE;	// 需要管理的物理页个数
	/*
	由于bootloader加载ucore的结束地址（用全局指针变量end记录）以上的空间没有被使用，
	所以我们可以把end按页大小为边界取整后，作为管理页级物理内存空间所需的Page结构的内存空间
	*/
	pages = (struct Page *)ROUNDUP((void *)end, PGSIZE); 

	// 已占用物理空间，实现占用标记
	for (i = 0; i < npage; i ++) {
		SetPageReserved(pages + i);
	}
	/*
	为了简化起见，从地址0到地址pages+sizeof(struct Page) * npage)结束的物理内存空间设定为已占用物理内存空间
	（起始0~640KB的空间是空闲的），地址pages+sizeof(struct Page) * npage)以上的空间为空闲物理内存空间，
	这时的空闲空间起始地址为
	*/
	uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * npage);

	for (i = 0; i < memmap->nr_map; i ++) {
		uint64_t begin = memmap->map[i].addr, end = begin + memmap->map[i].size;
		if (memmap->map[i].type == E820_ARM) {
			if (begin < freemem) {
				begin = freemem;
			}
			if (end > KMEMSIZE) {
				end = KMEMSIZE;
			}
			if (begin < end) {
				begin = ROUNDUP(begin, PGSIZE);
				end = ROUNDDOWN(end, PGSIZE);
				if (begin < end) {
				// 空闲物理空间，实现空闲标记
					init_memmap(pa2page(begin), (end - begin) / PGSIZE);
				}
			}
		}
	}
}

static void
enable_paging(void) {
	lcr3(boot_cr3);

	// turn on paging
	uint32_t cr0 = rcr0();
	cr0 |= CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP;
	cr0 &= ~(CR0_TS | CR0_EM);
	lcr0(cr0);
}

static void
boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size, uintptr_t pa, uint32_t perm) {
	assert(PGOFF(la) == PGOFF(pa));
	uint32_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;
	la = ROUNDDOWN(la, PGSIZE);
	pa = ROUNDDOWN(pa, PGSIZE);
	for (; n > 0; n --, la += PGSIZE, pa += PGSIZE) {
		pte_t *ptep = get_pte(pgdir, la, 1);
		assert(ptep != NULL);
		*ptep = pa | PTE_P | perm;
	}
}

static void *
boot_alloc_page(void) {
	struct Page *p = alloc_page();
	if (p == NULL) {
		panic("boot_alloc_page failed.\n");
	}
	return page2kva(p);
}

void
pmm_init(void) {
	init_pmm_manager();

	// detect physical memory and create free page list
	page_init();

	check_alloc_page();

	// create initial page directory,
	boot_pgdir = boot_alloc_page();
	memset(boot_pgdir, 0, PGSIZE);
	boot_cr3 = PADDR(boot_pgdir);

	check_pgdir();

	static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

	// recursively insert boot_pgdir in itself
	// to form a virtual pate table at virtual address VPT
	boot_pgdir[PDX(VPT)] = PADDR(boot_pgdir) | PTE_P | PTE_W;

	// map all physical memory at KERNBASE
	boot_map_segment(boot_pgdir, KERNBASE, KMEMSIZE, 0, PTE_W);

	/* 这里存在的一个问题是，在调用enable_page函数使能分页机制后
	到执行完毕gdt_init函数重新建立好段页式映射机制的过程中，内核使用的还是旧的段表映射，
	也就是说，enable paging 之后，内核使用的是页表的低地址entry。
	如何保证此时内核依然能够正常工作呢？
	其实只需让低地址目录表项的内容等于以KERNBASE开始的高地址目录表项的内容即可。
	目前内核大小不超过4M （实际上是3M，因为内核从0x100000开始编址），
	这样就只需要让页表在0~4MB的线性地址与KERNBASE~ KERNBASE+4MB的线性地址获得相同的映射即可，
	都映射到0~4MB 的物理地址空间，具体实现在pmm.c中pmm_init函数的语句：
	
	*/
	boot_pgdir[0] = boot_pgdir[PDX(KERNBASE)];

	enable_paging();

	gdt_init();

	/*
	当执行完毕gdt_init函数后，新的段页式映射已经建立好了，
	上面的0~4MB的线性地址与0~4MB的物理地址一一映射关系已经没有用了。
	所以可以通过如下语句解除这个老的映射关系。
	*/
	boot_pgdir[0] = 0;

	check_boot_pgdir();

	print_pgdir();
}

pte_t *
get_pte(pde_t *pgdir, uintptr_t la, bool create) {
	/* LAB2 PROJ5:  
	 *
	 * if you need to visit a physical address, please use KADDR()
	 * please read pmm.h for useful macros
	 */
	// find in Page Directory
	pde_t *pdepage = &pgdir[PDX(la)]; // highest 10-bit		
	// check PTE_P flag, if no according pte page
	if ((*pdepage & PTE_P) == 0) {
		struct Page *p;
		// check create
		if (create == 0)
			return NULL;
		else {
			// allocate new page
			p = alloc_page();
		}
		// set page reference
		set_page_ref(p, 1);
		// set all zeros
		memset(KADDR(page2pa(p)), 0, PGSIZE);
		*pdepage = page2pa(p) | PTE_USER;
	}	
	// return address
	return &((pte_t*)KADDR(PDE_ADDR(*pdepage)))[PTX(la)];
}

struct Page *
get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store) {
	pte_t *ptep = get_pte(pgdir, la, 0);
	if (ptep_store != NULL) {
		*ptep_store = ptep;
	}
	if (ptep != NULL && *ptep & PTE_P) {
		return pa2page(*ptep);
	}
	return NULL;
}

static inline void
page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep) {
	/* LAB2 PROJ5:  
	 *
	 * please check if ptep is valid
	 * tlb must be manually updated if mapping is updated
	 */
	// check if ptep is valid (PTE_P)
	if (*ptep & PTE_P) {
		struct Page *p = pte2page(*ptep);
		// remove page
		if (page_ref_dec(p) == 0) {
			free_pages(p, 1);
		}
		*ptep = 0;
		// update tlb
		tlb_invalidate(pgdir, la);
	}
}

void
page_remove(pde_t *pgdir, uintptr_t la) {
	pte_t *ptep = get_pte(pgdir, la, 0);
	if (ptep != NULL) {
		page_remove_pte(pgdir, la, ptep);
	}
}

int
page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm) {
	pte_t *ptep = get_pte(pgdir, la, 1);
	if (ptep == NULL) {
		return -E_NO_MEM;
	}
	page_ref_inc(page);
	if (*ptep & PTE_P) {
		struct Page *p = pte2page(*ptep);
		if (p == page) {
			page_ref_dec(page);
		}
		else {
			page_remove_pte(pgdir, la, ptep);
		}
	}
	*ptep = page2pa(page) | PTE_P | perm;
	tlb_invalidate(pgdir, la);
	return 0;
}

// invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
void
tlb_invalidate(pde_t *pgdir, uintptr_t la) {
	if (rcr3() == PADDR(pgdir)) {
		invlpg((void *)la);
	}
}

static void
check_alloc_page(void) {
	pmm_manager->check();
	cprintf("check_alloc_page() succeeded!\n");
}

static void
check_pgdir(void) {
	assert(npage <= KMEMSIZE / PGSIZE);
	assert(boot_pgdir != NULL && (uint32_t)PGOFF(boot_pgdir) == 0);
	assert(get_page(boot_pgdir, 0x0, NULL) == NULL);

	struct Page *p1, *p2;
	p1 = alloc_page();
	assert(page_insert(boot_pgdir, p1, 0x0, 0) == 0);

	pte_t *ptep;
	assert((ptep = get_pte(boot_pgdir, 0x0, 0)) != NULL);
	assert(pa2page(*ptep) == p1);
	assert(page_ref(p1) == 1);

	ptep = &((pte_t *)KADDR(PTE_ADDR(boot_pgdir[0])))[1];
	assert(get_pte(boot_pgdir, PGSIZE, 0) == ptep);

	p2 = alloc_page();
	assert(page_insert(boot_pgdir, p2, PGSIZE, PTE_U | PTE_W) == 0);
	assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
	assert(*ptep & PTE_U);
	assert(*ptep & PTE_W);
	assert(boot_pgdir[0] & PTE_U);
	assert(page_ref(p2) == 1);

	assert(page_insert(boot_pgdir, p1, PGSIZE, 0) == 0);
	assert(page_ref(p1) == 2);
	assert(page_ref(p2) == 0);
	assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
	assert(pa2page(*ptep) == p1);
	assert((*ptep & PTE_U) == 0);

	page_remove(boot_pgdir, 0x0);
	assert(page_ref(p1) == 1);
	assert(page_ref(p2) == 0);

	page_remove(boot_pgdir, PGSIZE);
	assert(page_ref(p1) == 0);
	assert(page_ref(p2) == 0);

	assert(page_ref(pa2page(boot_pgdir[0])) == 1);
	free_page(pa2page(boot_pgdir[0]));
	boot_pgdir[0] = 0;

	cprintf("check_pgdir() succeeded!\n");
}

static void
check_boot_pgdir(void) {
	pte_t *ptep;
	int i;
	for (i = 0; i < npage; i += PGSIZE) {
		assert((ptep = get_pte(boot_pgdir, (uintptr_t)KADDR(i), 0)) != NULL);
		assert(PTE_ADDR(*ptep) == i);
	}

	assert(PTE_ADDR(boot_pgdir[PDX(VPT)]) == PADDR(boot_pgdir));

	assert(boot_pgdir[0] == 0);

	struct Page *p;
	p = alloc_page();
	assert(page_insert(boot_pgdir, p, 0x100, PTE_W) == 0);
	assert(page_ref(p) == 1);
	assert(page_insert(boot_pgdir, p, 0x100 + PGSIZE, PTE_W) == 0);
	assert(page_ref(p) == 2);

	const char *str = "ucore: Hello world!!";
	strcpy((void *)0x100, str);
	assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

	*(char *)(page2kva(p) + 0x100) = '\0';
	assert(strlen((const char *)0x100) == '\0');

	free_page(p);
	free_page(pa2page(PTE_ADDR(boot_pgdir[0])));
	boot_pgdir[0] = 0;

	cprintf("check_boot_pgdir() succeeded!\n");
}

static const char *
perm2str(int perm) {
	static char str[4];
	str[0] = (perm & PTE_U) ? 'u' : '-';
	str[1] = 'r';
	str[2] = (perm & PTE_W) ? 'w' : '-';
	str[3] = '\0';
	return str;
}

static int
get_pgtable_items(int left, int right, int start, uint32_t *table, int *left_store, int *right_store) {
	/* LAB2 PROJ5:  
	 *
	 * good luck...
	 */
	if (start >= right) 
		return 0;
	// find the first (left_store) valid page
	while (start < right && (table[start] & PTE_P) == 0)
		start++;
	if (start < right) {
		// set left
		if (left_store != NULL) 
			*left_store = start;
		int perm = table[start] & PTE_USER;
		start++;
		// find continuous pte of same permission property 
		while (start < right && (table[start] & PTE_USER) == perm)
			start++;
		// set right
		if (right_store != NULL)
			*right_store = start;
		return perm;
	}
	return 0;
}

void
print_pgdir(void) {
	cprintf("-------------------- BEGIN --------------------\n");
	int left, right = 0, perm;
	while ((perm = get_pgtable_items(0, NPDEENTRY, right, vpd, &left, &right)) != 0) {
		cprintf("PDE(%03x) %08x-%08x %08x %s\n", right - left,
				left * PTSIZE, right * PTSIZE, (right - left) * PTSIZE, perm2str(perm));
		int l, r = left * NPTEENTRY;
		while ((perm = get_pgtable_items(left * NPTEENTRY, right * NPTEENTRY, r, vpt, &l, &r)) != 0) {
			cprintf("  |-- PTE(%05x) %08x-%08x %08x %s\n", r - l,
					l * PGSIZE, r * PGSIZE, (r - l) * PGSIZE, perm2str(perm));
		} 
	}
	cprintf("--------------------- END ---------------------\n");
}

