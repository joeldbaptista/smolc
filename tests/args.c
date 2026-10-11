/*
 * args.c -- main's argc and argv.
 */

#include stdio.h

int
main(int argc, int argv[])
{
	char arg[8];
	int i;

	for (i = 1; i < argc; ++i)
		printf("argv[%d] = %s\n", i, argv[i]);
	getarg(2, arg, 4, argc, argv);
	printf("getarg = %s, argv[argc] = %d\n", arg, argv[argc]);
	exit(argc);
}
