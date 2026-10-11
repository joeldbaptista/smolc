/*
 * fileio.c -- writing and reading a file.
 *
 * Shows: fopen(), fprintf(), fputs(), fgets(), fclose() and the
 * convention that a Small-C file descriptor is a small int, not a
 * FILE pointer.  fopen() returns NULL on failure.
 *
 * The library reaches the host's files through the smol "sys"
 * instruction, so TEST.TXT is created in the current directory.
 */

#include stdio.h

char *name;

int
main(void)
{
	char line[80];
	int fd, n;

	name = "TEST.TXT";

	if ((fd = fopen(name, "w")) == NULL) {
		printf("cannot create %s\n", name);
		return 1;
	}
	n = 0;
	while (++n <= 3)
		fprintf(fd, "line %d\n", n);
	fputs("done\n", fd);
	fclose(fd);

	if ((fd = fopen(name, "r")) == NULL) {
		printf("cannot open %s\n", name);
		return 1;
	}
	while (fgets(line, 80, fd) != NULL)
		printf("read: %s", line);
	fclose(fd);

	unlink(name);
	return 0;
}
