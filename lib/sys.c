/*
 * sys.c -- the host services, reached through the smol sys instruction.
 *
 * Each function loads its arguments from the stack into r1 and r2 and
 * leaves the result in r0, which is where Small-C expects it.  On
 * entry [r7] is the return address, [r7 + 8] the last argument and
 * [r7 + 16] the one before it.
 */

#include stdio.h

void
Uexit(int status)
{
#asm
	mov r1, [r7 + 8]
	sys 0
#endasm
}

int
Ugetc(int fd)
{
#asm
	mov r1, [r7 + 8]
	sys 1
#endasm
}

int
Uputc(int fd, int c)
{
#asm
	mov r1, [r7 + 16]
	mov r2, [r7 + 8]
	sys 2
#endasm
}

int
Uopen(char *name, char *mode)
{
#asm
	mov r1, [r7 + 16]
	mov r2, [r7 + 8]
	sys 3
#endasm
}

int
Uclose(int fd)
{
#asm
	mov r1, [r7 + 8]
	sys 4
#endasm
}

/* Remove the file name.  Returns 0, else ERR. */
int
unlink(char *name)
{
	if (Uunlink(name))
		return ERR;
	return 0;
}

int
Uunlink(char *name)
{
#asm
	mov r1, [r7 + 8]
	sys 5
#endasm
}

/* Rename the file from to to.  Returns 0, else ERR. */
int
rename(char *from, char *to)
{
	if (Urename(from, to))
		return ERR;
	return 0;
}

int
Urename(char *from, char *to)
{
#asm
	mov r1, [r7 + 16]
	mov r2, [r7 + 8]
	sys 6
#endasm
}

/* Exit with status, which goes to the host. */
void
exit(int status)
{
	Uexit(status);
}

void
abort(void)
{
	Uexit(1);
}
