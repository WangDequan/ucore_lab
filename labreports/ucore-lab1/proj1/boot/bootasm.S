# ���岢ʵ����bootloader����ִ�еĺ���start���˺���������һ���ĳ�ʼ���������
# ��ʵģʽ������ģʽ��ת����������bootmain.c�е�bootmain����

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

	# A20��ַ�߿��ƴ򿪹���
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
	lgdt gdtdesc									# ��������ģʽǰ�����ö��������϶���������
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

# ע���������ݽṹ
# Bootstrap GDT
.p2align 2											# force 4 byte alignment
# 3������������ÿ����������ռ8�ֽڣ���24�ֽ�
# gdtdescָ����ȫ���������������Կ����Ƕ���������ɵ�һ�����飩����ʼλ����gdt���Ŵ���
# ��gdt���Ŵ�����������������������Ϣ

# ��һ����NULL����������û�����壬��ʾȫ�����������Ŀ�ʼ
# �������Ǵ������������λ��ȫ������������0x8����λ�ã������пɶ���STA_R���Ϳ�ִ�У�STA_X�������ԣ�
#   ���Ҷ���ʼ��ַΪ0���δ�СΪ4GB��
# �����������ݶ���������λ��ȫ������������0x10����λ�ã������пɶ���STA_R���Ϳ�д��STA_W�������ԣ�
#   ���Ҷ���ʼ��ַΪ0���δ�СΪ4GB��
gdt:
	SEG_NULLASM										# null seg NULL��
	SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)			# code seg for bootloader and kernel CODE��
	SEG_ASM(STA_W, 0x0, 0xffffffff)					# data seg for bootloader and kernel DATA��

gdtdesc:
	.word 0x17										# sizeof(gdt) - 1
	.long gdt										# address gdt