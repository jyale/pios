#if LAB >= 2
/*
 * System call handling.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the xv6 instructional operating system from MIT.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#include <inc/x86.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/trap.h>
#include <inc/syscall.h>
#include <inc/label.h>

#include <kern/cpu.h>
#include <kern/trap.h>
#include <kern/proc.h>
#include <kern/syscall.h>
#include <kern/label.h>
#if LAB >= 5
#include <kern/net.h>
#endif

#if LAB >= 9
#include <dev/timer.h>
#endif


#if SOL >= 3
static void gcc_noreturn do_ret(trapframe *tf);
#endif

// This bit mask defines the eflags bits user code is allowed to set.
#define FL_USER		(FL_CF|FL_PF|FL_AF|FL_ZF|FL_SF|FL_DF|FL_OF)


// During a system call, generate a specific processor trap -
// as if the user code's INT 0x30 instruction had caused it -
// and reflect the trap to the parent process as with other traps.
static void gcc_noreturn
systrap(trapframe *utf, int trapno, int err)
{
#if SOL >= 3
	//cprintf("systrap: reflect trap %d to parent process\n", trapno);
	utf->trapno = trapno;
	utf->err = err;
	proc_ret(utf, 0);	// abort syscall insn and return to parent
#else
	panic("systrap() not implemented.");
#endif
}

// Recover from a trap that occurs during a copyin or copyout,
// by aborting the system call and reflecting the trap to the parent process,
// behaving as if the user program's INT instruction had caused the trap.
// This uses the 'recover' pointer in the current cpu struct,
// and invokes systrap() above to blame the trap on the user process.
//
// Notes:
// - Be sure the parent gets the correct trapno, err, and eip values.
// - Be sure to release any spinlocks you were holding during the copyin/out.
//
static void gcc_noreturn
sysrecover(trapframe *ktf, void *recoverdata)
{
#if SOL >= 3
	trapframe *utf = (trapframe*)recoverdata;	// user trapframe

	cpu *c = cpu_cur();
	assert(c->recover == sysrecover);
	c->recover = NULL;

	// Pretend that a trap caused this process to stop.
	systrap(utf, ktf->trapno, ktf->err);
#else
	panic("sysrecover() not implemented.");
#endif
}

// Check a user virtual address block for validity:
// i.e., make sure the complete area specified lies in
// the user address space between VM_USERLO and VM_USERHI.
// If not, abort the syscall by sending a T_PGFLT to the parent,
// again as if the user program's INT instruction was to blame.
//
// Note: Be careful that your arithmetic works correctly
// even if size is very large, e.g., if uva+size wraps around!
//
static void checkva(trapframe *utf, uint32_t uva, size_t size)
{
#if SOL >= 3
	if (uva < VM_USERLO || uva >= VM_USERHI
			|| size >= VM_USERHI - uva) {

		// Outside of user address space!  Simulate a page fault.
		systrap(utf, T_PGFLT, 0);
	}
#else
	panic("checkva() not implemented.");
#endif
}

// Copy data to/from user space,
// using checkva() above to validate the address range
// and using sysrecover() to recover from any traps during the copy.
void usercopy(trapframe *utf, bool copyout,
			void *kva, intptr_t uva, size_t size)
{
	checkva(utf, uva, size);

	// Now do the copy, but recover from page faults.
#if SOL >= 3
	cpu *c = cpu_cur();
	assert(c->recover == NULL);
	c->recover = sysrecover;

	//pmap_inval(proc_cur()->pml4, VM_USERLO, VM_USERHI-VM_USERLO);

	if (copyout)
		memmove((void*)uva, kva, size);
	else
		memmove(kva, (void*)uva, size);

	assert(c->recover == sysrecover);
	c->recover = NULL;
#else
	panic("syscall_usercopy() not implemented.");
#endif
}

static void
do_cputs(trapframe *tf, uint32_t cmd)
{
	// Print the string supplied by the user: pointer in EBX
#if SOL >= 3
	char buf[CPUTS_MAX+1];
	usercopy(tf, 0, buf, tf->rbx, CPUTS_MAX);
	buf[CPUTS_MAX] = 0;	// make sure it's null-terminated
	cprintf("%s", buf);
#else	// SOL < 3
	cprintf("%s", (char*)tf->rbx);
#endif	// SOL < 3

	trap_return(tf);	// syscall completed
}
#if SOL >= 2

static void
do_put(trapframe *tf, uint32_t cmd)
{
	proc *p = proc_cur();
	assert(p->state == PROC_RUN && p->runcpu == cpu_cur());
//	cprintf("PUT proc %p rip %p rsp %p cmd %x\n", p, tf->rip, tf->rsp, cmd);

	if (cmd & SYS_REMOTE == 0) {
#if SOL >= 5
		// First migrate if we need to.
		uint8_t node = (tf->rdx >> 8) & 0xff;
		if (node == 0) node = RRNODE(p->home);		// Goin' home
		if (node != net_node)
			net_migrate(tf, node, 0);	// abort syscall and migrate
#endif // SOL >= 5
	}

	proc *cp = NULL;
	if (cmd & SYS_REMOTE) {
		// find receiver process
		uint8_t node = (tf->rdx >> 56) & 0xff;
//		cprintf("PUT node %x net_node %x\n", node, net_node);
		if (node == 0 || node == net_node) {
			tf->rdx &= (1ULL << 56) - 1;
			cp = mid_find(tf->rdx);
			if (cp == &proc_null || cp == p) {
				// no matching process
				cmd = 0;
				goto exit;
			}
		} else {
			cp = proc_net;
		}
	}

	spinlock_acquire(&p->lock);

	if (cmd & SYS_REMOTE) {
		// SYS_REMOTE = SYS_COPY | SYS_START for remote process
		cmd = SYS_COPY | SYS_START | SYS_REMOTE;
	} else {
		// Find the named child process; create if it doesn't exist
		uint32_t cn = tf->rdx & 0xff;
		cp = p->child[cn];
		if (!cp) {
			cp = proc_alloc(p, cn);
			if (!cp)	// XX handle more gracefully
				panic("sys_put: no memory for child");
		}
	}
//	cprintf("PUT cp %p(%x)\n", cp, cp->state);

	// WWY: do label pacing
	tag_t less = label_leq_hi(&p->label, &cp->clearance);
	if (p->sv.pff & PFF_REEXEC) {
		less.time = 0;
		p->sv.pff &= ~PFF_REEXEC;
	}
	uint64_t ts = 0;
	if (less.time) {
		// wait until paced
		uint64_t t = timer_read();
		t = t * 1000000000 / TIMER_FREQ;	// convert to nanoseconds
		ts = ROUNDUP(t, label_time(less.time));
	}

	// Synchronize with child if necessary.
	if (cmd & SYS_REMOTE) {
		if (cp == proc_net) {
			if (ts != 0) {
				cprintf("proc %p send wait for proc_net\n", p);
				proc_wait(p, proc_net, tf, ts);
			}
		} else if (cp->state != PROC_BLOCK || ts != 0) {
//			cprintf("proc %p send wait for proc %p non-blocked\n", p, cp);
			proc_wait(p, cp, tf, ts);
		} else if (cp->waitproc != p) {
//			cprintf("proc %p send wait for proc %p blocked for other proc\n", p, cp);
			proc_wait(p, cp, tf, ts);
		} else {
//			cprintf("proc %p don't send wait for proc %p\n", p, cp);
		}
	} else {
		if (cp->state != PROC_STOP || ts != 0) {
//			cprintf("proc %p put wait for proc %p non-stopped\n", p, cp);
			proc_wait(p, cp, tf, ts);
		} else {
//			cprintf("proc %p don't put wait for proc %p\n", p, cp);
		}
	}

	// WWY: do label checking
	if (less.level) {
		// should abort
		spinlock_release(&p->lock);
		goto exit;
	}

	if ((cmd & SYS_REMOTE) && cp == proc_net) {
		net_send(tf, tf->rdx, tf->rsi, tf->rdi, tf->rcx);
	}

	// Since the child is now stopped, it's ours to control;
	// we no longer need our process lock -
	// and we don't want to be holding it if usercopy() below aborts.
	spinlock_release(&p->lock);

	// Put child's general register state
	if (cmd & SYS_REGS) {
		int len = offsetof(procstate, fx);	// just integer regs
		if (cmd & SYS_FPU) len = sizeof(procstate); // whole shebang

		// Copy user's trapframe into child process
#if SOL >= 3
		usercopy(tf, 0, &cp->sv, tf->rbx, len);
#else
		procstate *cs = (procstate*) tf->rbx;
		memcpy(&cp->sv, cs, len);
#endif

		// Make sure process uses user-mode segments and eflag settings
#if LAB >= 9
		cp->sv.tf.gs = SEG_USER_GS_64 | 3;
		cp->sv.tf.fs = 0;
#endif
		cp->sv.tf.ds = SEG_USER_DS_64 | 3;
		cp->sv.tf.es = SEG_USER_DS_64 | 3;
		cp->sv.tf.cs = SEG_USER_CS_64 | 3;
		cp->sv.tf.ss = SEG_USER_DS_64 | 3;
		cp->sv.tf.rflags &= FL_USER;
		cp->sv.tf.rflags |= FL_IF;	// enable interrupts
#if LAB >= 9

		// Child gets to be nondeterministic only if parent is
		if (!(p->sv.pff & PFF_NONDET))
			cp->sv.pff &= ~PFF_NONDET;
#endif
	}

#if SOL >= 3
	uintptr_t sva = tf->rsi;
	uintptr_t dva = tf->rdi;
	size_t size = tf->rcx;
	switch (cmd & SYS_MEMOP) {
	case 0:	// no memory operation
		break;
	case SYS_COPY:
		// validate source region
		if (PGOFF(sva) || PGOFF(size)
				|| sva < VM_USERLO || sva > VM_USERHI
				|| size > VM_USERHI-sva)
			systrap(tf, T_GPFLT, 0);
		// fall thru...
	case SYS_ZERO:
		// validate destination region
		if (PGOFF(dva) || PGOFF(size)
				|| dva < VM_USERLO || dva > VM_USERHI
				|| size > VM_USERHI-dva)
			systrap(tf, T_GPFLT, 0);

		switch (cmd & SYS_MEMOP) {
		case SYS_ZERO:	// zero memory and clear permissions
			pmap_remove(cp->pml4, dva, size);
			break;
		case SYS_COPY:	// copy from local src to dest in child
			pmap_copy(p->pml4, sva, cp->pml4, dva, size);
			break;
		}
		break;
	default:
		systrap(tf, T_GPFLT, 0);
	}

	if (cmd & SYS_PERM) {
		// validate destination region
		if (PGOFF(dva) || PGOFF(size)
				|| dva < VM_USERLO || dva > VM_USERHI
				|| size > VM_USERHI-dva)
			systrap(tf, T_GPFLT, 0);
		if (!pmap_setperm(cp->pml4, dva, size, cmd & SYS_RW))
			panic("pmap_put: no memory to set permissions");
	}

	if (cmd & SYS_SNAP)	// Snapshot child's state
		pmap_copy(cp->pml4, VM_USERLO, cp->rpml4, VM_USERLO,
				VM_USERHI-VM_USERLO);

#endif	// SOL >= 3

exit:
//	cprintf("PUT cmd %x\n", cmd);
	// Start the child if requested
	if (cmd & SYS_START)
		proc_ready(cp);

	trap_return(tf);	// syscall completed
}

static void
do_get(trapframe *tf, uint32_t cmd)
{
	proc *p = proc_cur();
	assert(p->state == PROC_RUN && p->runcpu == cpu_cur());
//	cprintf("GET proc %p rip %p rsp %p cmd %x\n", p, tf->rip, tf->rsp, cmd);

#if SOL >= 5
	// First migrate if we need to.
	uint8_t node = (tf->rdx >> 8) & 0xff;
	if (node == 0) node = RRNODE(p->home);		// Goin' home
	if (node != net_node)
		net_migrate(tf, node, 0);	// abort syscall and migrate

#endif // SOL >= 5
	spinlock_acquire(&p->lock);

	// Find the named child process; DON'T create if it doesn't exist
	uint32_t cn = tf->rdx & 0xff;
	proc *cp = p->child[cn];
	if (!cp)
		cp = &proc_null;

	// WWY: do label pacing
	tag_t less = label_leq_hi(&cp->label, &p->clearance);
	if (p->sv.pff & PFF_REEXEC) {
		less.time = 0;
		p->sv.pff &= ~PFF_REEXEC;
	}
	uint64_t ts = 0;
	if (less.time) {
		// wait until paced
		uint64_t t = timer_read();
		t = t * 1000000000 / TIMER_FREQ;	// convert to nanoseconds
		ts = ROUNDUP(t, label_time(less.time));
	}

	// Synchronize with child if necessary.
	if (cp->state != PROC_STOP || ts != 0)
		proc_wait(p, cp, tf, ts);

	// WWY: do label checking
	if (less.level) {
		// should abort
		spinlock_release(&p->lock);
		goto exit;
	}

	// Since the child is now stopped, it's ours to control;
	// we no longer need our process lock -
	// and we don't want to be holding it if usercopy() below aborts.
	spinlock_release(&p->lock);

	// Get child's general register state
	if (cmd & SYS_REGS) {
		int len = offsetof(procstate, fx);	// just integer regs
		if (cmd & SYS_FPU) len = sizeof(procstate); // whole shebang

#if LAB >= 9
		// Hide our instruction counting from user code.
		// (XXX maintain a virtual TF for the user.)
		//cp->sv.tf.eflags &= ~FL_TF;
		assert(!(cp->sv.tf.rflags & FL_TF));

#endif
		// Copy child process's trapframe into user space
#if SOL >= 3
		usercopy(tf, 1, &cp->sv, tf->rbx, len);
#else
		procstate *cs = (procstate*) tf->rbx;
		memcpy(cs, &cp->sv, len);
#endif
	}

#if SOL >= 3
	uintptr_t sva = tf->rsi;
	uintptr_t dva = tf->rdi;
	size_t size = tf->rcx;
	switch (cmd & SYS_MEMOP) {
	case 0:	// no memory operation
		break;
	case SYS_COPY:
	case SYS_MERGE:
		// validate source region
		if (PGOFF(sva) || PGOFF(size)
				|| sva < VM_USERLO || sva > VM_USERHI
				|| size > VM_USERHI-sva)
			systrap(tf, T_GPFLT, 0);
		// fall thru...
	case SYS_ZERO:
		// validate destination region
		if (PGOFF(dva) || PGOFF(size)
				|| dva < VM_USERLO || dva > VM_USERHI
				|| size > VM_USERHI-dva)
			systrap(tf, T_GPFLT, 0);

		switch (cmd & SYS_MEMOP) {
		case SYS_ZERO:	// zero memory and clear permissions
			pmap_remove(p->pml4, dva, size);
			break;
		case SYS_COPY:	// copy from local src to dest in child
			pmap_copy(cp->pml4, sva, p->pml4, dva, size);
			break;
		case SYS_MERGE:	// merge from local src to dest in child
			pmap_merge(cp->rpml4, cp->pml4, sva,
					p->pml4, dva, size);
			break;
		}
		break;
	default:
		systrap(tf, T_GPFLT, 0);
	}

	if (cmd & SYS_PERM) {
		// validate destination region
		if (PGOFF(dva) || PGOFF(size)
				|| dva < VM_USERLO || dva > VM_USERHI
				|| size > VM_USERHI-dva)
			systrap(tf, T_GPFLT, 0);
		if (!pmap_setperm(p->pml4, dva, size, cmd & SYS_RW))
			panic("pmap_get: no memory to set permissions");
	}

	if (cmd & SYS_SNAP)
		systrap(tf, T_GPFLT, 0);	// only valid for PUT

#endif	// SOL >= 3
exit:
	trap_return(tf);	// syscall completed
}

static void gcc_noreturn
do_ret(trapframe *tf)
{
	//cprintf("RET proc %p rip %p rsp %p\n", proc_cur(), tf->rip, tf->rsp);
	if (tf->rdx == 0) {
		// return to parent
		proc_ret(tf, 1);	// Complete syscall insn and return to parent
	} else {
		// block process
		uint8_t node = (tf->rdx >> 56) & 0xff;
		proc *cp = proc_cur();
		if (node == 0 || node == net_node) {
			tf->rdx &= (1ULL << 56) - 1;
			proc *p = mid_find(tf->rdx);
			if (p == &proc_null || p == cp) {
				// no matching process
				trap_return(tf);
			}
//			cprintf("proc %p recv block for proc %p\n", cp, p);
			proc_block(p, cp ,tf);
		} else {
			net_recv(tf, tf->rdx);
		}
	}
}

#if LAB >= 9
static void gcc_noreturn
do_time(trapframe *tf)
{
	uint64_t t = timer_read();
	t = t * 1000000000 / TIMER_FREQ;	// convert to nanoseconds
	tf->rdx = t >> 32;
	tf->rax = t;
	trap_return(tf);
}

static void gcc_noreturn
do_ncpu(trapframe *tf)
{
	int newlim = tf->rcx;
	if (newlim > 0)
		cpu_limit = newlim;
	else
		warn("do_ncpu: bad CPU limit %d", newlim);
	cprintf("do_ncpu: CPU limit now %d\n", cpu_limit);
	trap_return(tf);
}
#endif

static void gcc_noreturn
do_label(trapframe *tf)
{
	tag_t tag;
	memmove(&tag, &tf->rdx, sizeof(tag_t));
	proc *p = proc_cur();
	spinlock_acquire(&p->lock);
	if (tf->rbx) {
		if (tf->rcx) {
			cprintf("original clearance: ");
			label_print(&p->clearance);
			tf->rax = proc_set_clearance(p, tag);
			cprintf("new clearance: ");
			label_print(&p->clearance);
		} else {
			cprintf("original label: ");
			label_print(&p->label);
			tf->rax = proc_set_label(p, tag);
			cprintf("new label: ");
			label_print(&p->label);
		}
	} else {
		if (tf->rcx) {
			cprintf("current clearance: ");
			label_print(&p->clearance);
		} else {
			cprintf("current label: ");
			label_print(&p->label);
		}
	}
	spinlock_release(&p->lock);
	trap_return(tf);
}

static void gcc_noreturn
do_mid(trapframe *tf)
{
	uint64_t mid = tf->rcx;
	pid_t pid = tf->rdx;

	proc *p = proc_cur();
	if (pid >= 0) {
		// valid pid, perform mid_unregister
		if (pid) {
			spinlock_acquire(&p->lock);
			proc *cp = p->child[pid];
			spinlock_release(&p->lock);
			p = cp;
		}
		mid_unregister(p);
	} else {
		// invalid pid, perform mid_register
		tf->rax = mid_register(mid, p);
	}

	trap_return(tf);
}

#endif	// SOL >= 2

// Common function to handle all system calls -
// decode the system call type and call an appropriate handler function.
// Be sure to handle undefined system calls appropriately.
void
syscall(trapframe *tf)
{
	// EAX register holds system call command/flags
	uint32_t cmd = tf->rax;
	switch (cmd & SYS_TYPE) {
	case SYS_CPUTS:	return do_cputs(tf, cmd);
#if SOL >= 2
	case SYS_PUT:	return do_put(tf, cmd);
	case SYS_GET:	return do_get(tf, cmd);
	case SYS_RET:	return do_ret(tf);
#if LAB >= 9
	case SYS_TIME:	return do_time(tf);
	case SYS_NCPU:	return do_ncpu(tf);
#endif
	case SYS_LABEL:	return do_label(tf);
	case SYS_MID:	return do_mid(tf);
#else	// not SOL >= 2
	// Your implementations of SYS_PUT, SYS_GET, SYS_RET here...
#endif	// not SOL >= 2
	default:	return;		// handle as a regular trap
	}
}

#endif	// LAB >= 2
