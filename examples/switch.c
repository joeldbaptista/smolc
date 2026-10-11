/*
 * switch.c -- switch, case, default and break.
 *
 * Small-C compiles a switch into a call to CCSWITCH, which searches a
 * table of label/value word pairs terminated by a zero word.  The table
 * is static data, and r1 holds its address.  Look for
 * "call _CCSWITCH" in the emitted assembly.
 *
 * Note that the cases are not indented past the switch itself.
 */

#include stdio.h

int
classify(int c)
{
	switch (c) {
	case '0': /* FALLTHROUGH */
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		return 'd';
	case ' ': /* FALLTHROUGH */
	case '\t':
	case '\n':
		return 's';
	default:
		return 'o';
	}
}

int
main(void)
{
	char *text;
	int i;
	int digits, spaces, other;

	text = "abc 123\tx9";
	digits = spaces = other = 0;
	i = 0;
	while (text[i]) {
		switch (classify(text[i++])) {
		case 'd':
			++digits;
			break;
		case 's':
			++spaces;
			break;
		default:
			++other;
		}
	}
	printf("digits=%d spaces=%d other=%d\n", digits, spaces, other);
	return 0;
}
