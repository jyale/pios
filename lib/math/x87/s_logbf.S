/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include "asm.h"

ENTRY(logbf)
	flds	4(%esp)
	fxtract
	fstp	%st
	ret
END(logbf)
