/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/init.h>
#include <kern/console.h>
#include <kern/debug.h>
#include <kern/mem.h>
#include <kern/cpu.h>
#include <kern/trap.h>
#if LAB >= 2
#include <kern/mp.h>
#endif	// LAB >= 2

#if LAB >= 2
#include <dev/lapic.h>
#endif	// LAB >= 2


// Called first from entry.S on the bootstrap processor,
// and later from boot/bootother.S on all other processors.
void
init(void)
{
	if (cpu_onboot()) {
		extern char edata[], end[];

		// Before anything else, complete the ELF loading process.
		// Clear all uninitialized global data (BSS) in our program,
		// ensuring that all static/global variables start out zero.
		memset(edata, 0, end - edata);

		// Initialize the console.
		// Can't call cprintf until after we do this!
		cons_init();

		// Lab 1: test cprintf and debug_trace
		cprintf("1234 decimal is %o octal!\n", 1234);
		debug_check();
	}

	// Initialize and load the bootstrap CPU's GDT, TSS, and IDT.
	cpu_init();
	trap_init();
#if LAB >= 2
	lapic_init();		// setup this CPU's local APIC
#endif	// LAB >= 2

	// Initialize PC hardware state that is shared between CPUs.
	// We do this only once on the bootstrap processor,
	// before we start any of the other CPUs.
	if (cpu_onboot()) {

		// Physical memory detection/initialization.
		// Can't call mem_alloc until after we do this!
		mem_init();

#if LAB >= 2
		// Find and init other processors in a multiprocessor system
		mp_init();

		cpu_bootothers();	// Get other processors started
#endif	// LAB >= 2
	}

#if LAB >= 2
	cprintf("CPU %d (%s) has booted\n", cpu_cur()->id,
		cpu_onboot() ? "BP" : "AP");
#endif
	done();
}

void
done()
{
	asm volatile("hlt");	// Halt the processor
}

