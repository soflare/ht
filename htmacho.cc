/* 
 *	HT Editor
 *	htmacho.cc
 *
 *	Copyright (C) 2003 Stefan Weyergraf (stefan@weyergraf.de)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "elfstruc.h"
#include "log.h"
#include "htmacho.h"
#include "htmachohd.h"
#include "htendian.h"
#include "stream.h"
#include "tools.h"

#include "machostruc.h"

#include <stdlib.h>

format_viewer_if *htmacho_ifs[] = {
	&htmachoheader_if,
/*	&htelfsectionheaders_if,
	&htelfprogramheaders_if,
	&htelfimage_if,*/
	0
};

ht_view *htmacho_init(bounds *b, ht_streamfile *file, ht_format_group *format_group)
{
	byte magic[4];
	file->seek(0);
	file->read(&magic, sizeof magic);
	if (memcmp(magic, "\xfe\xed\xfa\xce", 4) != 0) return NULL;
		
	ht_macho *g = new ht_macho();
	g->init(b, file, htmacho_ifs, format_group, 0);
	return g;
}

format_viewer_if htmacho_if = {
	htmacho_init,
	0
};

/*
 *	CLASS ht_macho
 */
void ht_macho::init(bounds *b, ht_streamfile *f, format_viewer_if **ifs, ht_format_group *format_group, FILEOFS header_ofs)
{
	ht_format_group::init(b, VO_SELECTABLE | VO_BROWSABLE | VO_RESIZE, DESC_MACHO, f, false, true, 0, format_group);
	VIEW_DEBUG_NAME("ht_macho");

	LOG("%s: Mach-O: found header at %08x", file->get_filename(), header_ofs);
	
	ht_macho_shared_data *macho_shared=(ht_macho_shared_data *)malloc(sizeof(ht_macho_shared_data));

	shared_data = macho_shared;
	macho_shared->header_ofs = header_ofs;
	macho_shared->cmds = NULL;
/*	macho_shared->shnames=NULL;
	macho_shared->symtables=0;
	macho_shared->reloctables=0;
	macho_shared->v_image=NULL;
	macho_shared->htrelocs=NULL;
	macho_shared->fake_undefined_section=0;*/

	/* read header */
	file->seek(header_ofs);
	file->read(&macho_shared->header, sizeof macho_shared->header);
	create_host_struct(&macho_shared->header, MACHO_HEADER_struct, big_endian);

	/* read commands */
	FILEOFS ofs = file->tell();
	macho_shared->cmds = (MACHO_COMMAND**)malloc(sizeof (MACHO_COMMAND*) * macho_shared->header.ncmds);
	for (int i=0; i<macho_shared->header.ncmds; i++) {
		MACHO_COMMAND cmd;
		file->seek(ofs);
		file->read(&cmd, sizeof cmd);
		create_host_struct(&cmd, MACHO_COMMAND_struct, big_endian);
		// FIXME: improve this logic
		assert(cmd.cmdsize<=1024);
		macho_shared->cmds[i] = (MACHO_COMMAND*)malloc(cmd.cmdsize);
		file->seek(ofs);
		file->read(macho_shared->cmds[i], cmd.cmdsize);
		switch (cmd.cmd) {
			case LC_SEGMENT:
				create_host_struct(macho_shared->cmds[i], MACHO_SEGMENT_COMMAND_struct, big_endian);
				break;
			default:
				create_host_struct(macho_shared->cmds[i], MACHO_COMMAND_struct, big_endian);
		}
		ofs += cmd.cmdsize;
	}
#if 0
	switch (macho_shared->ident.e_ident[ELF_EI_DATA]) {
		case ELFDATA2LSB:
			macho_shared->byte_order = little_endian;
			break;
		case ELFDATA2MSB:
			macho_shared->byte_order = big_endian;
			break;
	}

	switch (macho_shared->ident.e_ident[ELF_EI_CLASS]) {
		case ELFCLASS32: {
			file->read(&macho_shared->header32, sizeof macho_shared->header32);
			create_host_struct(&macho_shared->header32, ELF_HEADER32_struct, macho_shared->byte_order);
			/* read section headers */
			macho_shared->sheaders.count=macho_shared->header32.e_shnum;
			macho_shared->sheaders.sheaders32=(ELF_SECTION_HEADER32*)malloc(macho_shared->sheaders.count*sizeof *macho_shared->sheaders.sheaders32);
			file->seek(header_ofs+macho_shared->header32.e_shoff);
			file->read(macho_shared->sheaders.sheaders32, macho_shared->sheaders.count*sizeof *macho_shared->sheaders.sheaders32);
			for (UINT i=0; i<macho_shared->sheaders.count; i++) {
				ELF_SECTION_HEADER32 a = macho_shared->sheaders.sheaders32[i];
				create_host_struct(macho_shared->sheaders.sheaders32+i, ELF_SECTION_HEADER32_struct, macho_shared->byte_order);
			}
	
			/* read program headers */
			macho_shared->pheaders.count=macho_shared->header32.e_phnum;
			macho_shared->pheaders.pheaders32=(ELF_PROGRAM_HEADER32*)malloc(macho_shared->pheaders.count*sizeof *macho_shared->pheaders.pheaders32);
			file->seek(header_ofs+macho_shared->header32.e_phoff);
			file->read(macho_shared->pheaders.pheaders32, macho_shared->pheaders.count*sizeof *macho_shared->pheaders.pheaders32);
			for (UINT i=0; i<macho_shared->pheaders.count; i++) {
				create_host_struct(macho_shared->pheaders.pheaders32+i, ELF_PROGRAM_HEADER32_struct, macho_shared->byte_order);
			}
			/* create a fake section for undefined symbols */
//			fake_undefined_symbols();

			/* create streamfile layer for relocations */
			auto_relocate();
			break;
		}
		case ELFCLASS64: {
			file->read(&macho_shared->header64, sizeof macho_shared->header64);
			create_host_struct(&macho_shared->header64, ELF_HEADER64_struct, macho_shared->byte_order);
			/* read section headers */
			macho_shared->sheaders.count=macho_shared->header64.e_shnum;
			macho_shared->sheaders.sheaders64=(ELF_SECTION_HEADER64*)malloc(macho_shared->sheaders.count*sizeof *macho_shared->sheaders.sheaders64);
/* FIXME: 64-bit */
			file->seek(header_ofs+macho_shared->header64.e_shoff.lo);
			file->read(macho_shared->sheaders.sheaders64, macho_shared->sheaders.count*sizeof *macho_shared->sheaders.sheaders64);
			for (UINT i=0; i<macho_shared->sheaders.count; i++) {
				ELF_SECTION_HEADER64 a = macho_shared->sheaders.sheaders64[i];
				create_host_struct(macho_shared->sheaders.sheaders64+i, ELF_SECTION_HEADER64_struct, macho_shared->byte_order);
			}

			/* read program headers */
			macho_shared->pheaders.count=macho_shared->header64.e_phnum;
			macho_shared->pheaders.pheaders64=(ELF_PROGRAM_HEADER64*)malloc(macho_shared->pheaders.count*sizeof *macho_shared->pheaders.pheaders64);
/* FIXME: 64-bit */
			file->seek(header_ofs+macho_shared->header64.e_phoff.lo);
			file->read(macho_shared->pheaders.pheaders64, macho_shared->pheaders.count*sizeof *macho_shared->pheaders.pheaders64);
			for (UINT i=0; i<macho_shared->pheaders.count; i++) {
				create_host_struct(macho_shared->pheaders.pheaders64+i, ELF_PROGRAM_HEADER64_struct, macho_shared->byte_order);
			}
			/* create a fake section for undefined symbols */
//			fake_undefined_symbols();

			/* create streamfile layer for relocations */
//			auto_relocate();
			break;
		}
	}
	while (init_if(&htelfsymboltable_if)) macho_shared->symtables++;
	while (init_if(&htelfreloctable_if)) macho_shared->reloctables++;
#endif
	/* init ifs */
	ht_format_group::init_ifs(ifs);
}

void ht_macho::done()
{
	ht_format_group::done();
/*	ht_macho_shared_data *macho_shared=(ht_macho_shared_data *)shared_data;
	if (macho_shared->shnames) {
		for (UINT i=0; i < macho_shared->sheaders.count; i++)
			free(macho_shared->shnames[i]);
		free(macho_shared->shnames);
	}		
	if (macho_shared->htrelocs) free(macho_shared->htrelocs);
	switch (macho_shared->ident.e_ident[ELF_EI_CLASS]) {
		case ELFCLASS32:
			if (macho_shared->sheaders.sheaders32) free(macho_shared->sheaders.sheaders32);
			if (macho_shared->pheaders.pheaders32) free(macho_shared->pheaders.pheaders32);
			break;
		case ELFCLASS64:
			if (macho_shared->sheaders.sheaders64) free(macho_shared->sheaders.sheaders64);
			if (macho_shared->pheaders.pheaders64) free(macho_shared->pheaders.pheaders64);
			break;
	}
	free(macho_shared);*/
}
#if 0
UINT ht_elf::find_reloc_section_for(UINT si)
{
	ht_macho_shared_data *macho_shared=(ht_macho_shared_data *)shared_data;

	ELF_SECTION_HEADER32 *s=macho_shared->sheaders.sheaders32;
	for (UINT i=0; i<macho_shared->sheaders.count; i++) {
		if (((s->sh_type==ELF_SHT_REL) || (s->sh_type==ELF_SHT_RELA)) &&
		(s->sh_info==si)) {
			return i;
		}
		s++;
	}
	return 0;
}

#define RELOC_BASE		0x10000000
#define RELOC_STEPPING	0x100000
#define RELOC_LIMIT		0xffffffff
			
elf32_addr invent_reloc_address(UINT si, ELF_SECTION_HEADER32 *s, UINT sc)
{
	elf32_addr a=RELOC_BASE;
	while (a<RELOC_LIMIT-s[si].sh_size) {
		bool ok=true;
		for (UINT i=0; i<sc; i++) {
			if ((s[i].sh_type==ELF_SHT_PROGBITS) && (s[i].sh_addr) &&
			((a>=s[i].sh_addr) && (a<s[i].sh_addr+s[i].sh_size))) {
				ok=false;
				break;
			}
		}
		if (ok) return a;
		a+=RELOC_STEPPING;
	}
	return 0;
}
	
void ht_elf::relocate_section(ht_reloc_file *f, UINT si, UINT rsi, elf32_addr a)
{
	ht_macho_shared_data *macho_shared=(ht_macho_shared_data *)shared_data;
	ELF_SECTION_HEADER32 *s=macho_shared->sheaders.sheaders32;

	FILEOFS h=s[rsi].sh_offset;
	
	if (s[rsi].sh_type==ELF_SHT_REL) {
		UINT relnum=s[rsi].sh_size / sizeof (ELF_REL32);
		file->seek(h);
		relnum=1;
		for (UINT i=0; i<relnum; i++) {
			ELF_REL32 r;
			file->read(&r, sizeof r);
			create_host_struct(&r, ELF_REL32_struct, macho_shared->byte_order);
/* FIXME: offset only works for relocatable files */
			f->insert_reloc(r.r_offset+s[si].sh_offset, new ht_elf32_reloc_entry(s[rsi].sh_link, r.r_offset, ELF32_R_TYPE(r.r_info), ELF32_R_SYM(r.r_info), 0, (ht_macho_shared_data*)shared_data, f));
		}
	}
}

void ht_elf::fake_undefined_symbols()
{
/* "resolve" undefined references (create a fake section) */
	
}

void ht_elf::auto_relocate()
{
	ht_elf32_reloc_file *rf=new ht_elf32_reloc_file();
	rf->init(file, false, (ht_macho_shared_data*)shared_data);
	
	bool reloc_needed=false;

	ht_macho_shared_data *macho_shared=(ht_macho_shared_data *)shared_data;

	ELF_SECTION_HEADER32 *s=macho_shared->sheaders.sheaders32;

	ht_elf_reloc_section *htr=(ht_elf_reloc_section*)malloc(sizeof *htr * macho_shared->sheaders.count);
	
/* relocate sections */
	for (UINT i=0; i<macho_shared->sheaders.count; i++) {
		htr[i].address=0;
		htr[i].reloc_shidx=0;
		if ((s[i].sh_type==ELF_SHT_PROGBITS) && (s[i].sh_addr==0)) {
			UINT j=find_reloc_section_for(i);
			if (j) {
				elf32_addr a=invent_reloc_address(i, s, macho_shared->sheaders.count);
				if (a) {
					reloc_needed=true;
					s[i].sh_addr=a;
					htr[i].address=a;
					htr[i].reloc_shidx=j;
				}
			}
		}
	}

/* apply the actual relocations */
/*	for (UINT i=0; i<macho_shared->sheaders.count; i++) {
		if (htr[i].address) {
			relocate_section(rf, i, htr[i].reloc_shidx, htr[i].address);
		}
	}*/

	free(htr);

	if (reloc_needed) {
		own_file=true;
		file=rf;
	} else {
		rf->done();
		delete rf;
	}
}

bool ht_elf::loc_enum_next(ht_format_loc *loc)
{
	ht_macho_shared_data *sh=(ht_macho_shared_data*)shared_data;
	if (loc_enum) {
		loc->name="elf";
		loc->start=sh->header_ofs;
		loc->length=file->get_size()-loc->start;	/* FIXME: ENOTOK */
		
		loc_enum=false;
		return true;
	}
	return false;
}

void ht_elf::loc_enum_start()
{
	loc_enum=true;
}
#endif

#if 0
/*
 *	address conversion routines
 */

bool elf_phys_and_mem_section(elf_section_header *sh, UINT elfclass)
{
	switch (elfclass) {
		case ELFCLASS32: {
			ELF_SECTION_HEADER32 *s = (ELF_SECTION_HEADER32*)sh;
			return ((s->sh_type==ELF_SHT_PROGBITS) && (s->sh_addr!=0));
		}
		case ELFCLASS64: {
			ELF_SECTION_HEADER64 *s = (ELF_SECTION_HEADER64*)sh;
			return ((s->sh_type==ELF_SHT_PROGBITS) && (s->sh_addr.lo!=0) && (s->sh_addr.hi!=0));
		}
	}
	return false;
}

bool elf_valid_section(elf_section_header *sh, UINT elfclass)
{
	switch (elfclass) {
		case ELFCLASS32: {
			ELF_SECTION_HEADER32 *s = (ELF_SECTION_HEADER32*)sh;
			return (((s->sh_type==ELF_SHT_PROGBITS) || (s->sh_type==ELF_SHT_NOBITS)) && (s->sh_addr!=0));
		}
		case ELFCLASS64: {
			ELF_SECTION_HEADER64 *s = (ELF_SECTION_HEADER64*)sh;
			return (((s->sh_type==ELF_SHT_PROGBITS) || (s->sh_type==ELF_SHT_NOBITS)) && (s->sh_addr.lo!=0) && (s->sh_addr.hi!=0));
		}
	}
	return false;
}

bool elf_addr_to_ofs(elf_section_headers *section_headers, UINT elfclass, ELFAddress addr, dword *ofs)
{
	switch (elfclass) {
		case ELFCLASS32: {
			ELF_SECTION_HEADER32 *s = section_headers->sheaders32;
			for (UINT i=0; i < section_headers->count; i++) {
				if ((elf_phys_and_mem_section((elf_section_header*)s, elfclass)) && (addr.a32 >= s->sh_addr) && (addr.a32 < s->sh_addr+s->sh_size)) {
					*ofs = addr.a32 - s->sh_addr + s->sh_offset;
					return true;
				}
				s++;
			}
			break;
		}
		case ELFCLASS64: {
			ELF_SECTION_HEADER64 *s = section_headers->sheaders64;
			for (UINT i=0; i < section_headers->count; i++) {
				if ((elf_phys_and_mem_section((elf_section_header*)s, elfclass)) && (qword_cmp(addr.a64, s->sh_addr) >= 0) && (addr.a64 < s->sh_addr + s->sh_size)) {
					qword qofs = addr.a64 - s->sh_addr + s->sh_offset;
					*ofs = qofs.lo;
					return true;
				}
				s++;
			}
			break;
		}
	}
	return false;
}

bool elf_addr_to_section(elf_section_headers *section_headers, UINT elfclass, ELFAddress addr, int *section)
{
	switch (elfclass) {
		case ELFCLASS32: {
			ELF_SECTION_HEADER32 *s = section_headers->sheaders32;
			for (UINT i = 0; i < section_headers->count; i++) {
				if ((elf_valid_section((elf_section_header*)s, elfclass)) && (addr.a32 >= s->sh_addr) && (addr.a32 < s->sh_addr + s->sh_size)) {
					*section = i;
					return true;
				}
				s++;
			}
			break;
		}
		case ELFCLASS64: {
			ELF_SECTION_HEADER64 *s = section_headers->sheaders64;
			for (UINT i = 0; i < section_headers->count; i++) {
				if ((elf_valid_section((elf_section_header*)s, elfclass)) && (qword_cmp(addr.a64, s->sh_addr) >= 0) && (addr.a64 < s->sh_addr + s->sh_size)) {
					*section = i;
					return true;
				}
				s++;
			}
			break;
		}
	}
	return false;
}

bool elf_addr_is_valid(elf_section_headers *section_headers, UINT elfclass, ELFAddress addr)
{
	switch (elfclass) {
		case ELFCLASS32: {
			ELF_SECTION_HEADER32 *s = section_headers->sheaders32;
			for (UINT i=0; i<section_headers->count; i++) {
				if ((elf_valid_section((elf_section_header*)s, elfclass)) && (addr.a32 >= s->sh_addr) && (addr.a32 < s->sh_addr + s->sh_size)) {
					return true;
				}
				s++;
			}
			break;
		}
		case ELFCLASS64: {
			ELF_SECTION_HEADER64 *s = section_headers->sheaders64;
			for (UINT i=0; i<section_headers->count; i++) {
				if ((elf_valid_section((elf_section_header*)s, elfclass)) && (qword_cmp(addr.a64, s->sh_addr) >= 0) && (addr.a64 < s->sh_addr + s->sh_size)) {
					return true;
				}
				s++;
			}
			break;
		}
	}
	return false;
}

bool elf_addr_is_physical(elf_section_headers *section_headers, UINT elfclass, ELFAddress addr)
{
	return false;
}


/*
 *	offset conversion routines
 */

bool elf_ofs_to_addr(elf_section_headers *section_headers, UINT elfclass, dword ofs, ELFAddress *addr)
{
	switch (elfclass) {
		case ELFCLASS32: {
			ELF_SECTION_HEADER32 *s = section_headers->sheaders32;
			for (UINT i = 0; i < section_headers->count; i++) {
				if ((elf_phys_and_mem_section((elf_section_header*)s, elfclass)) && (ofs>=s->sh_offset) && (ofs<s->sh_offset+s->sh_size)) {
					addr->a32 = ofs - s->sh_offset + s->sh_addr;
					return true;
				}
				s++;
			}
			break;
		}
		case ELFCLASS64: {
			ELF_SECTION_HEADER64 *s = section_headers->sheaders64;
			qword qofs = to_qword(ofs);
			for (UINT i = 0; i < section_headers->count; i++) {
				if ((elf_phys_and_mem_section((elf_section_header*)s, elfclass)) && (qword_cmp(qofs, s->sh_offset)>=0) && (qofs < s->sh_offset + s->sh_size)) {
					addr->a64 = qofs - s->sh_offset + s->sh_addr;
					return true;
				}
				s++;
			}
			break;
		}
	}
	return false;
}

bool elf_ofs_to_section(elf_section_headers *section_headers, UINT elfclass, dword ofs, int *section)
{
	switch (elfclass) {
		case ELFCLASS32: {
			ELF_SECTION_HEADER32 *s=section_headers->sheaders32;
			for (UINT i=0; i<section_headers->count; i++) {
				if ((elf_valid_section((elf_section_header*)s, elfclass)) && (ofs >= s->sh_offset) && (ofs<s->sh_offset+s->sh_size)) {
					*section = i;
					return true;
				}
				s++;
			}
			break;
		}
		case ELFCLASS64: {
			ELF_SECTION_HEADER64 *s = section_headers->sheaders64;
			qword qofs;
			qofs.hi = 0; qofs.lo = ofs;
			for (UINT i=0; i < section_headers->count; i++) {
				if ((elf_valid_section((elf_section_header*)s, elfclass)) && (qword_cmp(qofs, s->sh_offset)>=0) && (qofs < s->sh_offset + s->sh_size)) {
					*section = i;
					return true;
				}
				s++;
			}
			break;
		}
	}
	return false;
}

bool elf_ofs_to_addr_and_section(elf_section_headers *section_headers, UINT elfclass, dword ofs, ELFAddress *addr, int *section)
{
	return false;
}
#endif
/*
 *	CLASS ht_elf32_reloc_entry
 */
/*
ht_elf32_reloc_entry::ht_elf32_reloc_entry(UINT symtabidx, elf32_addr offset, UINT t, UINT symbolidx, elf32_addr addend, ht_macho_shared_data *data, ht_streamfile *file)
{
	type=t;
	
	FILEOFS h=data->sheaders.sheaders32[symtabidx].sh_offset;
	
	ELF_SYMBOL32 sym;
	file->seek(h+symbolidx*sizeof (ELF_SYMBOL32));
	file->read(&sym, sizeof sym);
	create_host_struct(&sym, ELF_SYMBOL32_struct, data->byte_order);
	
	dword A=addend;
	dword P=offset;
	dword S=sym.st_value;
	switch (type) {
		case ELF_R_386_32:
			relocs.r_32=S+A;
			break;
		case ELF_R_386_PC32:
			relocs.r_pc32=S+A-P;
			break;
	}
}
*/
/*
 *	CLASS ht_elf32_reloc_file
 */
/*
void	ht_elf32_reloc_file::init(ht_streamfile *s, bool os, ht_macho_shared_data *d)
{
	ht_reloc_file::init(s, os);
	data=d;
}

void	ht_elf32_reloc_file::reloc_apply(ht_data *reloc, byte *buf)
{
	ht_elf32_reloc_entry *e=(ht_elf32_reloc_entry*)reloc;
	
	dword *X=(dword*)buf;
	switch (e->type) {
		case ELF_R_386_32:
			*X=e->relocs.r_32;
			break;
		case ELF_R_386_PC32:
			*X=e->relocs.r_pc32;
			break;
	}
}

bool	ht_elf32_reloc_file::reloc_unapply(ht_data *reloc, byte *data)
{
	return false;
//	ht_elf32_reloc_entry *e=(ht_elf32_reloc_entry*)reloc;
}

*/