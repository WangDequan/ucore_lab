# 定义并实现了bootloader最先执行的函数start，此函数进行了一定的初始化，完成了
# 从实模式到保护模式的转换，并调用bootmain.c中的bootmain函数

#include <asm.h>

# Start the CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.set PROT_MODE_CSEG,		0x8						# kernel code segment selector
.set PROT_MODE_DSEG,		0x10					# kernel data segment selector
.set CR0_PE_ON,				0x1						# protected mode enable flag

.globl start
start:
.code16												# Assemble for 16-bit mode
	cli												# Disable interrupts
	cld												# String operations increment

	# Set up the important data segment registers (DS, ES, SS).
	xorw %ax, %ax									# Segment number zero
	movw %ax, %ds									# -> Data Segment
	movw %ax, %es									# -> Extra Segment
	movw %ax, %ss									# -> Stack Segment

	# A20地址线控制打开工作
	# Enable A20:
	#  For backwards compatibility with the earliest PCs, physical
	#  address line 20 is tied low, so that addresses higher than
	#  1MB wrap around to zero by default. This code undoes this.
seta20.1:
	inb $0x64, %al									# Wait for not busy
	testb $0x2, %al
	jnz seta20.1

	movb $0xd1, %al									# 0xd1 -> port 0x64
	outb %al, $0x64

seta20.2:
	inb $0x64, %al									# Wait for not busy
	testb $0x2, %al
	jnz seta20.2

	movb $0xdf, %al									# 0xdf -> port 0x60
	outb %al, $0x60

	# Switch from real to protected mode, using a bootstrap GDT
	# and segment translation that makes virtual addresses
	# identical to physical addresses, so that the
	# effective memory map does not change during the switch.
	lgdt gdtdesc									# 启动保护模式前建立好段描述符合段描述符表
	movl %cr0, %eax
	orl $CR0_PE_ON, %eax
	movl %eax, %cr0

	# Jump to next instruction, but in 32-bit code segment.
	# Switches processor into 32-bit mode.
	ljmp $PROT_MODE_CSEG, $protcseg

.code32												# Assemble for 32-bit mode
protcseg:
	# Set up the protected-mode data segment registers
	movw $PROT_MODE_DSEG, %ax						# Our data segment selector
	movw %ax, %ds									# -> DS: Data Segment
	movw %ax, %es									# -> ES: Extra Segment
	movw %ax, %fs									# -> FS
	movw %ax, %gs									# -> GS
	movw %ax, %ss									# -> SS: Stack Segment

	# Set up the stack pointer and call into C.
	movl $0x0, %ebp
	movl $start, %esp
	call bootmain

	# If bootmain returns (it shouldn't), loop.
spin:
	jmp spin

# 注意以下数据结构
# Bootstrap GDT
.p2align 2											# force 4 byte alignment
# 3个段描述符，每个段描述符占8字节，共24字节
# gdtdesc指出了全局描述符表（可以看成是段描述符组成的一个数组）的起始位置在gdt符号处，
# 而gdt符号处放置了三个段描述符的信息

# 第一个是NULL段描述符，没有意义，表示全局描述符表的开始
# 紧接着是代码段描述符（位于全局描述符表的0x8处的位置），具有可读（STA_R）和可执行（STA_X）的属性，
#   并且段起始地址为0，段大小为4GB；
# 接下来是数据段描述符（位于全局描述符表的0x10处的位置），具有可读（STA_R）和可写（STA_W）的属性，
#   并且段起始地址为0，段大小为4GB。
gdt:
	SEG_NULLASM										# null seg NULL段
	SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)			# code seg for bootloader and kernel CODE段
	SEG_ASM(STA_W, 0x0, 0xffffffff)					# data seg for bootloader and kernel DATA段

gdtdesc:
	.word 0x17										# sizeof(gdt) - 1
	.long gdt										# address gdt
