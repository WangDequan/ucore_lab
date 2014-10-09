#ifndef __LIBS_ELF_H__
#define __LIBS_ELF_H__

#include <types.h>

#define ELF_MAGIC	0x464C457FU			// "\x7FELF" in little endian

/* 
ELF���ļ�ͷ��������ִ���ļ������ݽṹelf header������������ִ���ļ�����֯�ṹ��
*/
/* file header */
struct elfhdr {
	uint32_t e_magic;     // must equal ELF_MAGIC
	uint8_t e_elf[12];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint32_t e_entry;	// ������ڵ������ַ
	uint32_t e_phoff;	// program header ���λ��ƫ��
	uint32_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;	// program header ���е������Ŀ
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

/*
����ͷ�������ݽṹ
*/
/* program section header */
struct proghdr {
	uint32_t p_type;	// ������
	uint32_t p_offset;	// ������ļ�ͷ��ƫ��ֵ
	uint32_t p_va;		// �εĵ�һ���ֽڽ����ŵ��ڴ��е������ַ
	uint32_t p_pa;
	uint32_t p_filesz;
	uint32_t p_memsz;	// �����ڴ�ӳ����ռ�õ��ֽ���
	uint32_t p_flags;
	uint32_t p_align;
};

#endif /* !__LIBS_ELF_H__ */

