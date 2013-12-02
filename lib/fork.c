// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.
	// LAB 4: Your code here.
 	pte_t pte = uvpt[PGNUM((uint32_t)addr)];
	addr = (void*)ROUNDDOWN((uint32_t)addr, PGSIZE);
	if (!(err & FEC_WR))
		panic("pgfault: the faulting access was not a write!");
	if (!(pte & PTE_COW))
		panic("pgfault: the faulting access was not to a copy-on-write page");
	// Allocate a new page, map it at a temporary location (PFTEMP) 
	if ((r = sys_page_alloc(0, PFTEMP, PTE_U | PTE_P | PTE_W)) < 0)
		panic("pgfault: sys_page_alloc: %e", r);
	// copy the data from the old page to the new page
	memmove(PFTEMP, addr, PGSIZE);
	// map new page it into the old page's address
	// (this will implicitly unmap old page and decrease the physical page's "ref" by one.) 
	if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_U | PTE_P | PTE_W)) < 0)
		panic("pgfault: sys_page_map: %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void *addr = (void *)(pn);
	// This is NOT what you should do in your fork.
	if ((r = sys_page_alloc(envid, addr, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
	if ((r = sys_page_map(envid, addr, 0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_map: %e", r);
	memmove(PFTEMP, addr, PGSIZE);
	if ((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("sys_page_unmap: %e", r);

	// pte_t pte = uvpt[PGNUM(pn)];

	// if (!(pte & PTE_P) || !(pte & PTE_U))
	// 	return -E_INVAL;
	// if ((pte & PTE_W) || (pte & PTE_COW)) {
	// 	// Issues in Ruizhe's implementation: missing other PTE bits
	// 	if ((r = sys_page_map(0, addr, envid, addr, PTE_U | PTE_P | PTE_COW)) < 0)
	// 		panic("duppage: sys_page_map: %e", r);
	// 	if ((r = sys_page_map(0, addr, 0, addr, PTE_U | PTE_P | PTE_COW)) < 0)
	// 		panic("duppage: sys_page_map: %e", r);
	// } else {
	// 	if ((r = sys_page_map(0, addr, envid, addr, PGOFF(pte))) < 0)
	// 		panic("duppage: sys_page_map: %e", r);
	// }
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t envid;
	uint8_t *addr;
	int r;
	extern void _pgfault_upcall(); 

	set_pgfault_handler(pgfault);
	if ((envid = sys_exofork()) < 0)
		panic("fork: sys_exofork: %e", envid);
	if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	for (addr = 0; addr < (uint8_t*)(UXSTACKTOP - PGSIZE); addr += PGSIZE) {
		if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P))
			duppage(envid, (unsigned)addr);
	}
	if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
		panic("fork: sys_env_set_pgfault_upcall: %e", r);
	if ((r = sys_page_alloc(envid, (void*) UXSTACKTOP - PGSIZE, PTE_U | PTE_W | PTE_P)) < 0)
		panic("fork: sys_page_alloc: %e", r);
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("fork: sys_env_set_status: %e", r);
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}