/*
 * asm.c -- dropping into smol assembly with #asm ... #endasm.
 *
 * The calling convention:
 *   - arguments are pushed left to right, each one 64 bits wide;
 *   - the topmost stack entry on entry is the return address;
 *   - the return value is left in r0;
 *   - the caller pops the arguments after the call;
 *   - r3 holds the argument count, for CCARGC().
 *
 * So [r7 + 8] is the last argument and [r7 + 16] the one before it.
 * A function may use r0, r1 and r2 freely.
 */

#include stdio.h

/*
 * Add a and b, returning the result.  This is the example from the
 * original BYTE documentation, in smol assembly.
 */
int
fadd2(int a, int b)
{
#asm
	mov r0, [r7 + 16]   ; a
	mov r1, [r7 + 8]    ; b
	add r0, r1          ; result in r0
#endasm
}

/*
 * Return the address of the first byte past the static data, which
 * the assembler defines as __end.  The heap starts there.
 */
int
heapbase(void)
{
#asm
	mov r0, __end
#endasm
}

int
main(void)
{
	printf("fadd2(20, 22) = %d\n", fadd2(20, 22));
	printf("heap starts at %d\n", heapbase());
	return 0;
}
