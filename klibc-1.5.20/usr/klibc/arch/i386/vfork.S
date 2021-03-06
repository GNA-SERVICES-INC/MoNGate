#
# usr/klibc/arch/i386/vfork.S
#
# vfork is nasty - there must be nothing at all on the stack above
# the stack frame of the enclosing function.
#

#include <asm/unistd.h>

        .text
        .align  4
	.globl	vfork
	.type	vfork, @function
vfork:
	popl	%edx			/* Return address */
	movl	$__NR_vfork, %eax
	int	$0x80
	pushl	%edx
	cmpl	$-4095, %eax
	jae	1f
	ret
1:
	negl	%eax
	movl	%eax, errno
	orl	$-1, %eax
	ret
