/*
 *  Debug_extended_backtrace.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 03.04.12.
 *  code under GPL
 *
 */

/*
  backtrace_symbols in glibc looks up symbols using dladdr which is limited in
  the symbols that it sees. This code opens the executable and shared
  libraries using libbfd and will look up backtrace information using the symbol
  table and the dwarf line information.

  Derived on the work by Jeff Muizelaar, 2007.
  Which was derived from addr2line.c from GNU Binutils.

  About getting the backtrace, see GetCallstack which also works for other threads. (Debug_GetCallstack.cpp)
  
  This code is used by DumpCallstack() and DumpAllThreadsCallstack(). (Debug_DumpCallstack.cpp)
 
  Note that on MacOSX for Mach-O + DWARF support, you need a recent
  libbfd version from at least 2012. See <http://stackoverflow.com/questions/10044697>
  for current a small limitation of libbfd on MacOSX. It basically
  means that you need to call `dsymutil` to make libbfd working.
*/

/* addr2line.c -- convert addresses to line number and function name
   Copyright 1997, 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Ulrich Lauther <Ulrich.Lauther@mchp.siemens.de>

   This file was part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.  */

#ifdef HASBFD

#include "util/Result.h"
#include "FindFile.h" // GetBinaryFilename
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cxxabi.h>
#include <boost/shared_ptr.hpp>
#include "Debug.h"
#include "util/StringConv.h"

/* 2 characters for each byte, plus 1 each for 0, x, and NULL */
#define PTRSTR_LEN (sizeof(void *) * 2 + 3)

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>

//HACK: Fix compilation error caused by some (new?) versions of bfd.h
//Source: https://stackoverflow.com/questions/11748035/binutils-bfd-h-wants-config-h-now
#ifndef PACKAGE
	#define PACKAGE 1
#endif
#ifndef PACKAGE_VERSION
	#define PACKAGE_VERSION 1
#endif

#ifdef __APPLE__
#include <bfd.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#else
#include <ansidecl.h>
#include <bfd.h>
#define HAVE_DECL_BASENAME 1
#include <libiberty.h>
#include <dlfcn.h>
#include <link.h>
#endif


#if 0

void (*dbfd_init)(void);
bfd_vma (*dbfd_scan_vma)(const char *string, const char **end, int base);
bfd* (*dbfd_openr)(const char *filename, const char *target);
bfd_boolean (*dbfd_check_format)(bfd *abfd, bfd_format format);
bfd_boolean (*dbfd_check_format_matches)(bfd *abfd, bfd_format format, char ***matching);
bfd_boolean (*dbfd_close)(bfd *abfd);
bfd_boolean (*dbfd_map_over_sections)(bfd *abfd, void (*func)(bfd *abfd, asection *sect, void *obj),
		void *obj);
#define bfd_init dbfd_init

static void load_funcs(void)
{
	void * handle = dlopen("libbfd.so", RTLD_NOW);
	dbfd_init = dlsym(handle, "bfd_init");
	dbfd_scan_vma = dlsym(handle, "bfd_scan_vma");
	dbfd_openr = dlsym(handle, "bfd_openr");
	dbfd_check_format = dlsym(handle, "bfd_check_format");
	dbfd_check_format_matches = dlsym(handle, "bfd_check_format_matches");
	dbfd_close = dlsym(handle, "bfd_close");
	dbfd_map_over_sections = dlsym(handle, "bfd_map_over_sections");
}

#endif


static asymbol **syms;		/* Symbol table.  */


static Result slurp_symtab(bfd * abfd);
static void find_address_in_section(bfd *abfd, asection *section, void *data);

/* Read in the symbol table.  */

static Result slurp_symtab(bfd * abfd)
{
	if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
		return "file has no symtab";

	unsigned int size = 0;
	long symcount = bfd_read_minisymbols(abfd, false, (void**) &syms, &size);
	if (symcount == 0)
		symcount = bfd_read_minisymbols(abfd, true /* dynamic */ ,
						(void**) &syms, &size);

	if (symcount < 0)
		return "error getting minisymbols";

	if (symcount == 0)
		return "no minisymbols found";

	if(false) {
		notes << "slurp_symtab: read " << symcount << " symbols" << endl;
		for(long i = 0; i < symcount; ++i) {
			notes << " " << syms[i]->name << endl;
		}
	}

	return true;
}


static std::string handleFuncName(const std::string& funcName) {
	int status = 0;
	char* s = abi::__cxa_demangle(funcName.c_str(), 0, 0, &status);
//	char* s = bfd_demangle(NULL, funcName.c_str(), 0);
	if(!s) return funcName;
	std::string ret(s);
	free(s);
	return ret;
}

/* These global variables are used to pass information between
   translate_addresses and find_address_in_section.  */

static bfd_vma pc;
static const char *filename;
static const char *functionname;
static unsigned int line;
static bool found = false;
static bool sectionFound = false;

/* Look for an address in a section.  This is called via
   bfd_map_over_sections.  */

static void find_address_in_section(bfd *abfd, asection *section, void *data __attribute__ ((__unused__)) )
{
	if (found)
		return;

	if ((bfd_section_flags(section) & SEC_ALLOC) == 0)
		return;

	bfd_vma vma = bfd_section_vma(section);
	bfd_size_type size = bfd_section_size(section);

	if (pc < vma)
		return;

	if (pc >= vma + size)
		return;

	sectionFound = true;
	found = bfd_find_nearest_line(abfd, section, syms, pc - vma,
				      &filename, &functionname, &line);
}



static Result translate_addresses_buf(bfd * abfd, bfd_vma addr, const std::string& prefix, std::vector<std::string>& buf)
{
	pc = addr;
	sectionFound = false;
	found = false;
	bfd_map_over_sections(abfd, find_address_in_section, (void*) NULL);

	if (!found) {
		if(sectionFound) return "function not found";
		else return "section not found";

	} else while(found) {
		std::ostringstream ret;
		ret << prefix;

		if (functionname == NULL || *functionname == '\0')
			ret << "<unknown function>";
		else
			ret << handleFuncName(functionname);

		ret << " (";
		if (filename != NULL) {
			const char *h = strrchr(filename, '/');
			if (h != NULL)
				ret << (h + 1);
			else
				ret << filename;
		}
		else
			ret << "<unknown file>";

		ret << ":" << line << ")";

		buf.push_back(ret.str());

		found = bfd_find_inliner_info(abfd, &filename, &functionname, &line);
	}

	return true;
}

struct BfdSession {
	bfd* abfd;
	asymbol** syms;
	BfdSession() : abfd(NULL), syms(NULL) {}
	~BfdSession() {
		free(syms); syms = NULL;
		if(abfd) bfd_close(abfd);
		abfd = NULL;
	}
};
typedef std::map<std::string, boost::shared_ptr<BfdSession> > BfdSessionMap;
static BfdSessionMap bfdSessions;

static bfd* abfd = NULL;

/*static void cleanupBfdSessions() {
	abfd = NULL;
	syms = NULL;
	bfdSessions.clear();
}*/

static Result get_bfd(const std::string& file_name) {
	BfdSessionMap::mapped_type& s = bfdSessions[file_name];
	if(s.get()) {
		abfd = s->abfd;
		syms = s->syms;
		return true;
	}

	abfd = bfd_openr(file_name.c_str(), NULL);

	if (abfd == NULL)
		return "can't open file";

	if (bfd_check_format(abfd, bfd_archive))
		return "invalid format";

	char **matching;
	if (!bfd_check_format_matches(abfd, bfd_object, &matching)) {
		if (bfd_get_error() == bfd_error_file_ambiguously_recognized)
			free(matching);
		return "format does not match";
	}

	if(NegResult r = slurp_symtab(abfd))
		return "slurp_symtab: " + r.res.humanErrorMsg;

	s = BfdSessionMap::mapped_type(new BfdSession);
	s->abfd = abfd;
	s->syms = syms;
	return true;
}

static Result process_file(const std::string& file_name, const std::string& prefix, bfd_vma addr, std::vector<std::string>& ret_buf)
{
	if(NegResult r = get_bfd(file_name))
		return r.res;

	if(NegResult r = translate_addresses_buf(abfd, addr, prefix, ret_buf))
		return r.res;

	return true;
}

#ifndef __APPLE__

struct file_match {
	const char *file;
	const void *address;
	void *base;
	void *hdr;
};

static int find_matching_file(struct dl_phdr_info *info,
		size_t size, void *data)
{
	struct file_match *match = (struct file_match*) data;
	/* This code is modeled from Gfind_proc_info-lsb.c:callback() from libunwind */
	long n;
	const ElfW(Phdr) *phdr;
	ElfW(Addr) load_base = info->dlpi_addr;
	phdr = info->dlpi_phdr;
	for (n = info->dlpi_phnum; --n >= 0; phdr++) {
		if (phdr->p_type == PT_LOAD) {
			ElfW(Addr) vaddr = phdr->p_vaddr + load_base;
			if ((uintptr_t)match->address >= vaddr && (uintptr_t)match->address < vaddr + phdr->p_memsz) {
				/* we found a match */
				match->file = info->dlpi_name;
				match->base = (void*)(uintptr_t)info->dlpi_addr;
			}
		}
	}
	return 0;
}
#else
static bool
ptr_is_in_exe(const void *ptr, const struct mach_header *& header, intptr_t& offset, uintptr_t& vmaddr, std::string& image_name)
{
	uint32_t i, count = _dyld_image_count();

	for (i = 0; i < count; i++) {
		header = _dyld_get_image_header(i);
		offset = _dyld_get_image_vmaddr_slide(i);
		//notes << i << "," << offset << ": " << _dyld_get_image_name(i) << endl;

		uint32_t j = 0;
		struct load_command* cmd = (struct load_command*)((char *)header + sizeof(struct mach_header));
		if(header->magic == MH_MAGIC_64)
			cmd = (struct load_command*)((char *)header + sizeof(struct mach_header_64));
		//struct load_command* cmd_end = cmd + header->sizeofcmds;

		while (j < header->ncmds) {
			if (cmd->cmd == LC_SEGMENT) {
				struct segment_command* seg = (struct segment_command*)cmd;
				if (((intptr_t)ptr >= (seg->vmaddr + offset)) && ((intptr_t)ptr < (seg->vmaddr + offset + seg->vmsize))) {
					vmaddr = seg->vmaddr;
					image_name = _dyld_get_image_name(i);
					return true;
				}
			}
			if (cmd->cmd == LC_SEGMENT_64) {
				struct segment_command_64* seg = (struct segment_command_64*)cmd;
				if (((uintptr_t)ptr >= (seg->vmaddr + offset)) && ((uintptr_t)ptr < (seg->vmaddr + offset + seg->vmsize))) {
					vmaddr = seg->vmaddr;
					image_name = _dyld_get_image_name(i);
					return true;
				}
			}

			j++;
			cmd = (struct load_command*)((char*)cmd + cmd->cmdsize);
		}
	}

	return false;
}
#endif

std::vector<std::string> trans_sym(const void* xaddr) {
	std::vector<std::string> ret;

	std::string prefix = "[0x" + hex((uintptr_t)xaddr) + "] ";
	bfd_vma addr = (bfd_vma)xaddr;
	Result r = true;

#ifndef __APPLE__
	struct file_match match;
	match.address = xaddr;
	dl_iterate_phdr(find_matching_file, &match);
	addr = bfd_vma((uintptr_t)xaddr - (uintptr_t)match.base);
	if (match.file && strlen(match.file))
		r = process_file(match.file, prefix, addr, ret);
	else
		r = process_file(GetBinaryFilename(), prefix, addr, ret);

#else
	const struct mach_header* header;
	intptr_t offset;
	uintptr_t vmaddr;
	std::string image_name;
	if(ptr_is_in_exe(xaddr, header, offset, vmaddr, image_name)) {
		addr = bfd_vma((uintptr_t)xaddr - offset);
		r = process_file(image_name, prefix, addr, ret);
	}
	else r = "image not found";
#endif

	if(!r) {
		std::string s = prefix;

		// we might be able to use dladdr as fallback
		Dl_info info;
		if(dladdr(xaddr, &info))
			s += handleFuncName(info.dli_sname) + " ((" + GetBaseFilename(info.dli_fname) + "))";
		else
			s += "<no dladdr info>";

		s += " <dbg: " + r.humanErrorMsg + ">";

		ret.push_back(s);
	}

	return ret;
}

std::vector<std::string> backtrace_symbols_str(void *const *buffer, int size) {
	std::vector<std::string> ret;

	bfd_init();
	for(int x = 0; x < size; ++x) {
		const void* xaddr = buffer[x];
		std::vector<std::string> translated_sym = trans_sym(xaddr);
		ret.insert(ret.end(), translated_sym.begin(), translated_sym.end());
	}

	return ret;
}

char **backtrace_symbols(void *const *buffer, int size)
{
	int stack_depth = size - 1;
	int x,y;
	/* discard calling function */
	int total = 0;

	char **final;
	char *f_strings;

	std::vector<std::string> locations(stack_depth + 1);

	bfd_init();
	for(x=stack_depth, y=0; x>=0; x--, y++){
		const void* xaddr = buffer[x];
		std::vector<std::string> translated_sym = trans_sym(xaddr);
		assert(translated_sym.size() >= 1);
		locations[x] = translated_sym[0];
		total += locations[x].size() + 1;
	}

	/* allocate the array of char* we are going to return and extra space for
	 * all of the strings */
	final = (char**)malloc(total + (stack_depth + 1) * sizeof(char*));
	/* get a pointer to the extra space */
	f_strings = (char*)(final + stack_depth + 1);

	/* fill in all of strings and pointers */
	for(x=stack_depth; x>=0; x--){
		strcpy(f_strings, locations[x].c_str());
		final[x] = f_strings;
		f_strings += strlen(f_strings) + 1;
	}

	return final;
}

void
backtrace_symbols_fd(void *const *buffer, int size, int fd)
{
        int j;
        char **strings;

        strings = backtrace_symbols(buffer, size);
		if (strings == NULL) return;

        for (j = 0; j < size; j++)
		printf("%s\n", strings[j]);

        free(strings);
}

// Not sure on this. Linking failed on Mac.
// Found it here: http://www.mail-archive.com/uclinux-dev@uclinux.org/msg02347.html
#ifndef HAVE_LIBINTL_DGETTEXT
extern "C"
const char *libintl_dgettext (const char *domain, const char *msg)
{
  return msg;
}

// This is for the Apple-GDB version of bfd.
extern "C"
const char * dcgettext(const char * domainname, const char * msgid, int category) {
	return msgid;
}
#endif /* !HAVE_LIBINTL_DGETTEXT */

#endif // HASBFD
