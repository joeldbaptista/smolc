/*
 * stdio.c -- file and console input and output.
 *
 * A file descriptor is a small int, as in the original Small-C
 * library: 0, 1 and 2 are stdin, stdout and stderr, and fopen()
 * returns the others.  The host does the buffering, so this layer only
 * keeps the end-of-file and error status and one pushed-back byte per
 * descriptor.
 */

#include stdio.h

#define MAXFILES 32
#define EOFBIT 1
#define ERRBIT 2

int Ustatus[MAXFILES]; /* EOFBIT and ERRBIT, per descriptor */
int Unextc[MAXFILES];  /* pushed-back byte plus one, else zero */

/* Return true if fd can index the tables above. */
int
Uvalid(int fd)
{
	return fd >= 0 && fd < MAXFILES;
}

/*
 * Open the file fn with mode "r", "w", "a" or "u" (update).
 * Returns the file descriptor, else NULL.
 */
int
fopen(char *fn, char *mode)
{
	int fd;

	if ((fd = Uopen(fn, mode)) < 0 || !Uvalid(fd))
		return NULL;
	Ustatus[fd] = 0;
	Unextc[fd] = 0;
	return fd;
}

/* Reopen fd on the file fn.  Returns fd, else NULL. */
int
freopen(char *fn, char *mode, int fd)
{
	fclose(fd);
	if (fopen(fn, mode) == fd)
		return fd;
	return NULL;
}

/* Close fd.  Returns 0, else EOF. */
int
fclose(int fd)
{
	if (Uclose(fd))
		return EOF;
	return 0;
}

/* Returns the next byte from fd, else EOF. */
int
fgetc(int fd)
{
	int c;

	if (!Uvalid(fd))
		return EOF;
	if (Unextc[fd]) {
		c = Unextc[fd] - 1;
		Unextc[fd] = 0;
		return c;
	}
	if ((c = Ugetc(fd)) == EOF)
		Ustatus[fd] |= EOFBIT;
	return c;
}

int
getc(int fd)
{
	return fgetc(fd);
}

int
getchar(void)
{
	return fgetc(stdin);
}

/* Push c back onto fd.  Returns c, else EOF. */
int
ungetc(int c, int fd)
{
	if (!Uvalid(fd) || Unextc[fd] || c == EOF)
		return EOF;
	Unextc[fd] = (c & 255) + 1;
	Ustatus[fd] &= ~EOFBIT;
	return c;
}

/* Write the byte c to fd.  Returns c, else EOF. */
int
fputc(int c, int fd)
{
	if (Uputc(fd, c) == EOF) {
		if (Uvalid(fd))
			Ustatus[fd] |= ERRBIT;
		return EOF;
	}
	return c;
}

int
putc(int c, int fd)
{
	return fputc(c, fd);
}

int
putchar(int c)
{
	return fputc(c, stdout);
}

/* Write string to fd.  Returns 0, else EOF. */
int
fputs(char *string, int fd)
{
	while (*string) {
		if (fputc(*string++, fd) == EOF)
			return EOF;
	}
	return 0;
}

/* Write string and a newline to stdout. */
void
puts(char *string)
{
	fputs(string, stdout);
	fputc('\n', stdout);
}

/*
 * Get an entire string, including its newline terminator, or size-1
 * characters, whichever comes first.  Returns str, else NULL.
 */
char *
fgets(char *str, int size, int fd)
{
	return Ugets(str, size, fd, 1);
}

/* Get a string from stdin, excluding its newline.  Returns str, else NULL. */
char *
gets(char *str)
{
	return Ugets(str, 32767, stdin, 0);
}

char *
Ugets(char *str, int size, int fd, int nl)
{
	char *next;
	int c;

	next = str;
	while (--size > 0) {
		if ((c = fgetc(fd)) == EOF) {
			*next = NULL;
			if (next == str)
				return NULL;
			return str;
		}
		if (c == '\n') {
			if (nl)
				*next++ = c;
			*next = NULL;
			return str;
		}
		*next++ = c;
	}
	*next = NULL;
	return str;
}

/* Read n bytes from fd into buf.  Returns the count read. */
int
read(int fd, char *buf, int n)
{
	int c, i;

	i = 0;
	while (i < n) {
		if ((c = fgetc(fd)) == EOF)
			break;
		buf[i++] = c;
	}
	return i;
}

/* Read n items of sz bytes each.  Returns the count of items read. */
int
fread(char *buf, int sz, int n, int fd)
{
	return read(fd, buf, n * sz) / sz;
}

/* Write n bytes from buf to fd.  Returns n, else -1. */
int
write(int fd, char *buf, int n)
{
	int i;

	i = 0;
	while (i < n) {
		if (fputc(buf[i++], fd) == EOF)
			return -1;
	}
	return n;
}

/* Write n items of sz bytes each.  Returns n, else zero. */
int
fwrite(char *buf, int sz, int n, int fd)
{
	if (write(fd, buf, n * sz) == -1)
		return 0;
	return n;
}

int
feof(int fd)
{
	if (!Uvalid(fd))
		return 0;
	return Ustatus[fd] & EOFBIT;
}

int
ferror(int fd)
{
	if (!Uvalid(fd))
		return 0;
	return Ustatus[fd] & ERRBIT;
}

void
clearerr(int fd)
{
	if (Uvalid(fd))
		Ustatus[fd] = 0;
}

/*
 * Formatted print, as described by Kernighan & Ritchie.  The b, c, d,
 * o, s, u and x specifications are supported; b (binary) is a
 * non-standard extension.
 *
 * These functions declare one parameter and walk the stack for the
 * rest, using the argument count the caller leaves for CCARGC().
 * Arguments are pushed left to right, so the first one has the
 * highest address.
 */

/* fprintf(fd, ctlstring, arg, arg, ...) */
int
fprintf(int argc)
{
	int *nxtarg;

	nxtarg = CCARGC() + &argc;
	return Uprint(*(--nxtarg), --nxtarg);
}

/* printf(ctlstring, arg, arg, ...) */
int
printf(int argc)
{
	return Uprint(stdout, CCARGC() + &argc - 1);
}

int
Uprint(int fd, int *nxtarg)
{
	char *ctl, *sptr, str[66];
	int arg, cc, ladj, len, maxchr, pad, width;

	cc = 0;
	ctl = *nxtarg--;
	while (*ctl) {
		if (*ctl != '%') {
			fputc(*ctl++, fd);
			++cc;
			continue;
		}
		++ctl;
		if (*ctl == '%') {
			fputc(*ctl++, fd);
			++cc;
			continue;
		}
		if (*ctl == '-') {
			ladj = 1;
			++ctl;
		} else {
			ladj = 0;
		}
		if (*ctl == '0')
			pad = '0';
		else
			pad = ' ';
		if (isdigit(*ctl)) {
			width = atoi(ctl++);
			while (isdigit(*ctl))
				++ctl;
		} else {
			width = 0;
		}
		if (*ctl == '.') {
			maxchr = atoi(++ctl);
			while (isdigit(*ctl))
				++ctl;
		} else {
			maxchr = 0;
		}
		arg = *nxtarg--;
		sptr = str;
		switch (*ctl++) {
		case 'c':
			str[0] = arg;
			str[1] = NULL;
			break;
		case 's':
			sptr = arg;
			break;
		case 'd':
			itoa(arg, str);
			break;
		case 'b':
			itoab(arg, str, 2);
			break;
		case 'o':
			itoab(arg, str, 8);
			break;
		case 'u':
			itoab(arg, str, 10);
			break;
		case 'x':
			itoab(arg, str, 16);
			break;
		default:
			return cc;
		}
		len = strlen(sptr);
		if (maxchr && maxchr < len)
			len = maxchr;
		if (width > len)
			width = width - len;
		else
			width = 0;
		if (!ladj) {
			while (width--) {
				fputc(pad, fd);
				++cc;
			}
		}
		while (len--) {
			fputc(*sptr++, fd);
			++cc;
		}
		if (ladj) {
			while (width--) {
				fputc(pad, fd);
				++cc;
			}
		}
	}
	return cc;
}

/*
 * Formatted read, as described by Kernighan & Ritchie.  The b, c, d,
 * o, s, u and x specifications are supported.  Like the print
 * functions, these walk the stack for their arguments.
 */

/* fscanf(fd, ctlstring, arg, arg, ...) */
int
fscanf(int argc)
{
	int *nxtarg;

	nxtarg = CCARGC() + &argc;
	return Uscan(*(--nxtarg), --nxtarg);
}

/* scanf(ctlstring, arg, arg, ...) */
int
scanf(int argc)
{
	return Uscan(stdin, CCARGC() + &argc - 1);
}

int
Uscan(int fd, int *nxtarg)
{
	char *carg, *ctl;
	int *narg;
	int ac, base, ch, cnv, sign, uval, wast, width;

	ac = 0;
	ctl = *nxtarg--;
	while (*ctl) {
		if (isspace(*ctl)) {
			++ctl;
			continue;
		}
		if (*ctl++ != '%')
			continue;
		if (*ctl == '*') {
			narg = carg = &wast;
			++ctl;
		} else {
			narg = carg = *nxtarg--;
		}
		ctl += utoi(ctl, &width);
		if (!width)
			width = 32767;
		if (!(cnv = *ctl++))
			break;
		while (isspace(ch = fgetc(fd)))
			;
		if (ch == EOF) {
			if (ac)
				break;
			return EOF;
		}
		ungetc(ch, fd);
		switch (cnv) {
		case 'c':
			*carg = fgetc(fd);
			break;
		case 's':
			while (width--) {
				if ((ch = fgetc(fd)) == EOF)
					break;
				if (isspace(ch)) {
					ungetc(ch, fd);
					break;
				}
				*carg = ch;
				if (carg != &wast)
					++carg;
			}
			*carg = 0;
			break;
		default:
			switch (cnv) {
			case 'b':
				base = 2;
				sign = 1;
				break;
			case 'd':
				base = 10;
				sign = 0;
				break;
			case 'o':
				base = 8;
				sign = 1;
				break;
			case 'u':
				base = 10;
				sign = 1;
				break;
			case 'x':
				base = 16;
				sign = 1;
				break;
			default:
				return ac;
			}
			*narg = uval = 0;
			while (width--) {
				if ((ch = fgetc(fd)) == EOF)
					break;
				if (isspace(ch)) {
					ungetc(ch, fd);
					break;
				}
				if (!sign) {
					if (ch == '-') {
						sign = -1;
						continue;
					}
					sign = 1;
				}
				if (ch < '0')
					return ac;
				if (ch >= 'a')
					ch -= 87;
				else if (ch >= 'A')
					ch -= 55;
				else
					ch -= '0';
				if (ch >= base)
					return ac;
				uval = uval * base + ch;
			}
			*narg = sign * uval;
		}
		++ac;
	}
	return ac;
}
