#ifndef __LIBS_ELF_H__
#define __LIBS_ELF_H__

#include <types.h>

#define ELF_MAGIC	0x464C457FU			// "\x7FELF" in little endian

/* 
ELF的文件头包含整个执行文件的数据结构elf header，描述了整个执行文件的组织结构。
*/
/* file header */
struct elfhdr {
	uint32_t e_magic;     // must equal ELF_MAGIC
	uint8_t e_elf[12];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint32_t e_entry;	// 程序入口的虚拟地址
	uint32_t e_phoff;	// program header 表的位置偏移
	uint32_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;	// program header 表中的入口数目
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

/*
程序头部的数据结构
*/
/* program section header */
struct proghdr {
	uint32_t p_type;	// 段类型
	uint32_t p_offset;	// 段相对文件头的偏移值
	uint32_t p_va;		// 段的第一个字节将被放到内存中的虚拟地址
	uint32_t p_pa;
	uint32_t p_filesz;
	uint32_t p_memsz;	// 段在内存映像中占用的字节数
	uint32_t p_flags;
	uint32_t p_align;
};

#endif /* !__LIBS_ELF_H__ */

