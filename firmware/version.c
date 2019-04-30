#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <console.h>
#include <string.h>
#include <generated/csr.h>
#include <generated/mem.h>

#include "stdio_wrap.h"
#include "uptime.h"

#include "version.h"
#include "version_data.h"

#define ALIGNMENT 4

static void print_csr_string(unsigned int addr, size_t size);
static void print_csr_string(unsigned int addr, size_t size) {
	size_t i;
	void* ptr = (void*)addr;
	for (i = 0; i < (size * ALIGNMENT); i += ALIGNMENT) {
		unsigned char c = MMPTR(ptr+i);
		if (c == '\0')
			return;
		putchar(c);
	}
}

static void print_csr_hex(unsigned int addr, size_t size);
static void print_csr_hex(unsigned int addr, size_t size) {
	size_t i = 0;
	void* ptr = (void*)addr;
	for (i = 0; i < (size * ALIGNMENT); i += ALIGNMENT) {
		unsigned char v = MMPTR(ptr+i);
		wprintf("%02x", v);
	}
}

void print_board_dna(void) {
#ifdef CSR_INFO_DNA_ID_ADDR
	print_csr_hex(CSR_INFO_DNA_ID_ADDR, CSR_INFO_DNA_ID_SIZE);
#else
	wprintf("Unknown");
#endif
}

extern unsigned char *mac_addr;
void print_board_mac(void) {
  unsigned char *mac = mac_addr;
  for (int i = 0; i < sizeof(mac); i++) {
    wprintf("%02x", mac[i]);
    if (i != (sizeof(mac)-1))
      wprintf(":");
  }
}

void print_version(void) {
	putchar('\n');
	wprintf("hardware version info\n");
	wprintf("===============================================\n");
	wprintf("           DNA: ");
	print_board_dna();
	putchar('\n');
	wprintf("           MAC: ");
	print_board_mac();
	putchar('\n');
	putchar('\n');
	wprintf("gateware version info\n");
	wprintf("===============================================\n");
#ifdef CSR_GITINFO_COMMIT_ADDR
	wprintf("      revision: ");
	print_csr_hex(CSR_GITINFO_COMMIT_ADDR, CSR_GITINFO_COMMIT_SIZE);
	putchar('\n');
#endif
//	wprintf("misoc revision: %08x\n", identifier_revision_read());
	putchar('\n');
	wprintf("firmware version info\n");
	wprintf("===============================================\n");
	wprintf("    git commit: %s\n", git_commit);
	wprintf("    git branch: %s\n", git_branch);
	wprintf("  git describe: %s\n", git_describe);
	wprintf("    git status:\n%s\n", git_status);
	wprintf("         built: "__DATE__" "__TIME__"\n");
	wprintf("        uptime: %s\n", uptime_str());
	wprintf("-----------------------------------------------\n");
}
