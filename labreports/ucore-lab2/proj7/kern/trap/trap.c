#include <types.h>
#include <mmu.h>
#include <memlayout.h>
#include <clock.h>
#include <trap.h>
#include <x86.h>
#include <stdio.h>
#include <assert.h>
#include <console.h>
#include <vmm.h>
#include <kdebug.h>

#define TICK_NUM 100

static void print_ticks() {
	cprintf("%d ticks\n",TICK_NUM);
#ifdef DEBUG_GRADE
	cprintf("End of Test.\n");
	panic("EOT: kernel seems ok.");
#endif
}

static struct gatedesc idt[256] = {{0}};

static struct pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t)idt
};

void
idt_init(void) {
	 /* LAB1   */
	extern uint32_t __vectors[];
	int i;
	for (i = 0; i < 256; i++) {
		SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
	}
	lidt(&idt_pd);
}

static const char *
trapname(int trapno) {
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(const char * const)) {
		return excnames[trapno];
	}
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16) {
		return "Hardware Interrupt";
	}
	return "(unknown trap)";
}

bool
trap_in_kernel(struct trapframe *tf) {
	return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

static const char *IA32flags[] = {
	"CF", NULL, "PF", NULL, "AF", NULL, "ZF", "SF",
	"TF", "IF", "DF", "OF", NULL, NULL, "NT", NULL,
	"RF", "VM", "AC", "VIF", "VIP", "ID", NULL, NULL,
};

void
print_trapframe(struct trapframe *tf) {
	cprintf("trapframe at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	cprintf("  err  0x%08x\n", tf->tf_err);
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x ", tf->tf_eflags);

	int i, j;
	for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]); i ++, j <<= 1) {
		if ((tf->tf_eflags & j) && IA32flags[i] != NULL) {
			cprintf("%s,", IA32flags[i]);
		}
	}
	cprintf("IOPL=%d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);

	if (!trap_in_kernel(tf)) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct pushregs *regs) {
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static inline void
print_pgfault(struct trapframe *tf) {
	/* error_code:
	 * bit 0 == 0 means no page found, 1 means protection fault
	 * bit 1 == 0 means read, 1 means write
	 * bit 2 == 0 means kernel, 1 means user
	 * */
	cprintf("page fault at 0x%08x: %c/%c [%s].\n", rcr2(),
			(tf->tf_err & 4) ? 'U' : 'K',
			(tf->tf_err & 2) ? 'W' : 'R',
			(tf->tf_err & 1) ? "protection fault" : "no page found");
}

static int
pgfault_handler(struct trapframe *tf) {
	extern struct mm_struct *check_mm_struct;
	print_pgfault(tf);
	if (check_mm_struct != NULL) {
		return do_pgfault(check_mm_struct, tf->tf_err, rcr2());
	}
	panic("unhandled page fault.\n");
}

static void
trap_dispatch(struct trapframe *tf) {
	char c;

	int ret;

	switch (tf->tf_trapno) {
	case T_DEBUG:
	case T_BRKPT:
		debug_monitor(tf);
		break;
	case T_PGFLT:
		/* LAB2 PROJ7:  
		 *
		 * add handler here
		 *
		 * if handler fails, it is OK to panic
		*/
		ret = pgfault_handler(tf);
		if (ret != 0)
		{
			print_trapframe(tf);
			panic("handle pgfault failed. %e\n", ret);
		}
		break;
	case IRQ_OFFSET + IRQ_TIMER:
		/* LAB1   */
		++ticks;
		if (ticks % TICK_NUM == 0)
			print_ticks();
		break;
	case IRQ_OFFSET + IRQ_COM1:
	case IRQ_OFFSET + IRQ_KBD:
		if ((c = cons_getc()) == 13) {
			debug_monitor(tf);
		}
		else {
			cprintf("%s [%03d] %c\n",
					(tf->tf_trapno != IRQ_OFFSET + IRQ_KBD) ? "serial" : "kbd", c, c);
		}
		break;
	default:
		// in kernel, it must be a mistake
		if ((tf->tf_cs & 3) == 0) {
			print_trapframe(tf);
			panic("unexpected trap in kernel.\n");
		}
	}
}

void
trap(struct trapframe *tf) {
	// dispatch based on what type of trap occurred
	trap_dispatch(tf);
}
