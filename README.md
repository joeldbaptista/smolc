# Smol-C

This is combination of two projects I've worked on: [reg](https://github.com/joeldbaptista/reg/tree/main) and
[small-c](https://github.com/joeldbaptista/small-c). The idea is to combine the two; that is, use
small-c C compiler, and `reg` assembly and virtual machine. The majority of the code started
in an exploration branch for `small-c`, and quickly morphed into this.

The tree holds these parts (complete list):

- `cc/`: `scc`, the BYTE Small-C 1.0 compiler, retargeted from 8088
  MASM to smol assembly.
- `smol/`: `sas`, the assembler, and `smol`, the virtual machine. Both
  are extended for C, and the assembly syntax follows NASM. The
  original reg programs in `smol/examples/` are translated to it.
- `lib/`: the runtime. `crt0.sas` starts a program, and five Small-C
  files hold the library.
- `examples/` and `tests/`: Small-C programs, and the regression tests.
- `STYLE.md` and `WRITTING-STYLE.md`: the style of the C code, and the
  style of the documentation.

The file names follow the project name. The list is complete.

- `.sas` is a smol assembly source file. `sas` assembles it.
- `.smol` is an assembled program. `smol` runs it.
- `svm.c` is the smol virtual machine, and `sas.c` is the assembler.

The sources in `smol/` are these files (complete list):

- `sas.c`: the assembler, `sas`.
- `svm.c`: the virtual machine, `smol`.
- `smol.h`: the instruction set and the binary format, which both
  programs share: the limits, the opcodes, the host services, the
  instruction and header structs, and the macro `WORDUP`.
- `common.c` and `common.h`: the definitions both programs link with.
  They are `die()`, which prints an error and exits, `argv0`, the
  table of register names `regnames`, and `encode()` and `decode()`,
  which convert an instruction to and from its form in a `.smol` file.
  `common.h` declares `die()`, `arg.h` declares `argv0`, and `smol.h`
  declares `regnames`, `encode()` and `decode()`.
- `arg.h`: the macros that parse command-line options.

## Build and run

	make            # tools, runtime and examples
	make run        # run every example
	make test       # run the regression tests

The build needs a C99 compiler. Edit `config.mk` to change `CC` or the
flags.

To build one program by hand:

	cc/scc -Ilib prog.c > prog.sas
	smol/sas -o prog.smol lib/crt0.sas prog.sas lib/sys.sas \
	    lib/stdio.sas lib/string.sas lib/ctype.sas lib/stdlib.sas
	smol/smol prog.smol arg1 arg2

Or, from the top directory, `make prog.smol` does both steps for a
`prog.c` in any subdirectory.

`scc` writes `prog.sas` itself when stdout is a terminal. Otherwise it
writes to stdout. It resolves `#include` against the current directory
first and the `-I` directory second. It exits with status 7 after an
error.

`sas` takes several source files and links them into one binary.
`crt0.sas` must come first among them, because it holds `main`, the
entry point.

`smol` passes the remaining arguments to the program as `argc` and
`argv`. Its exit status is the program's status. The option `-o file`
also writes the final machine state to `file`. The state holds the
following parts, in this order (complete list):

- every register, as `r0 = 42`, and the flags `zf`, `sf` and `of`;
- the static data as 8-byte words, as `mem[8] = 300`: the decimal
  byte address of the word, and the word read as a signed integer, low
  byte first, as the original `reg` printed it;
- the same static data after a `data:` line, as `hexdump -C` prints
  it: per line, the hexadecimal address, 16 bytes in hexadecimal, and
  the bytes as characters, with `.` for a byte that does not print. A
  `*` line stands for lines equal to the one before it, and the last
  line holds the address just past the data.

The static data starts at address 8. The heap and the stack are not
part of the state.

## Design decisions

The four decisions below are complete for the port.

1. **Word size.** An `int` is 64 bits, and a pointer is 64 bits. So
   `BPW` is 8 in `cc/ccdef.h`, and every smol register holds one `int`.
   A `char` is 8 bits and signed.
2. **Memory model.** Data memory is one byte-addressed array of 16 MiB.
   Static data starts at address 8, so address 0 is never a valid
   object and works as `NULL`. The heap follows the static data, and
   the stack grows down from the top. Code lives in a separate array,
   so a function pointer is an instruction index.
3. **Unsigned comparisons.** Small-C has no `unsigned` type. It
   applies unsigned comparisons only to pointers. An address on smol is
   never negative, so the compiler emits signed comparisons for them.
   The language stays the Small-C subset.
4. **I/O.** A `sys N` instruction asks the host for service `N`.

## Register use

| Register | Use                                              |
|----------|--------------------------------------------------|
| `r0`     | primary register and return value (8088: BX)     |
| `r1`     | secondary register (8088: DX)                    |
| `r2`     | scratch (8088: SI)                               |
| `r3`     | argument count before a call (8088: AL)          |
| `r7`     | stack pointer (8088: SP)                         |

Compiled code keeps no value in any other register. The routines in
`crt0.sas` that copy and clear structs, `CCMOVE` and `CCZERO`, use `r4`
and `r5` as scratch. So `#asm` code may use `r4` to `r15` freely, but
cannot rely on `r4` or `r5` across a struct copy or a local
initialiser. Arguments are pushed left to right, each 8 bytes wide. On
entry, `[r7]` is the return address, `[r7 + 8]` is the last argument,
and `[r7 + 16]` is the one before it. The caller pops the arguments.
See `examples/asm.c`.

## The smol assembly language

The syntax follows NASM. A line holds one instruction, a data
directive or a label, or a label and one of the others. `;` starts a
comment. Operands are separated by commas, and the destination comes
first. Each instruction line assembles to exactly one VM instruction.

An operand is one of the following (complete list):

- a register, `r0` to `r15`;
- a number, in decimal or with `0x` in hexadecimal, maybe negative;
- a label, which stands for its address, maybe with `+ k` or `- k`
  after it;
- a memory reference: `[rB]`, `[rB + k]`, `[rB - k]`, `[label]` or
  `[label + k]`, which is a word, or the same after `byte`, which is a
  byte.

The table is the complete list of instructions. `src` is a register or
a number.

| Instruction                  | Meaning                                         |
|------------------------------|-------------------------------------------------|
| `mov rA, rB`                 | copy a register                                 |
| `mov rA, 5` / `mov rA, label`| load a number, or a label's address             |
| `mov rA, [m]`                | load a word                                     |
| `movsx rA, byte [m]`         | load a byte, sign extended                      |
| `mov [m], rA`                | store a word                                    |
| `mov byte [m], rA`           | store the low byte                              |
| `add rA, src`                | also `sub`, `mul`, `div`, `rem`, `and`, `or`, `xor`, `shl`, `sar` |
| `inc rA` / `dec rA`          | add 1 / subtract 1                              |
| `neg rA` / `not rA`          | negate / complement                             |
| `cmp rA, src`                | set the flags from `rA - src`                   |
| `sete rA`                    | 1 if equal, else 0; also `setne`, `setl`, `setle`, `setg`, `setge` |
| `jmp L` / `jmp rA`           | jump                                            |
| `je L`                       | jump if equal; also `jne`, `jl`, `jle`, `jg`, `jge` |
| `call L` / `call rA`         | call                                            |
| `ret`                        | return                                          |
| `push rA` / `pop rA`         | stack operations; `r7` moves by 8               |
| `xchg rA, rB`                | exchange                                        |
| `sys N`                      | host service                                    |
| `hlt`                        | halt with status 0                              |

`div` and `rem` truncate towards zero, as C does. `sar` is an
arithmetic shift, and a shift count is taken modulo 64. The
conditional jumps and the `set` instructions compare as signed
numbers, after a `cmp`.

### Labels and data

A label names whatever item follows it: a data label if data follows,
and a code label if an instruction follows. The data directives are
(complete list):

	name:
		dq 1, -2, main, name + 8   ; 8 bytes per value
		db "hi", 10, 0             ; 1 byte per value or character
		resb 64                    ; 64 zero bytes

A string in `db` takes the escapes `\n`, `\t`, `\0`, `\\` and `\"`.
Data lines may appear between instructions. They go to the data
section and leave the code untouched.

A label that starts with `.` is local to its file. So the compiler's
internal labels (`.L1`, `.L2`, ...) and the labels of file-scope
statics (`._name`) do not clash between files. The assembler defines
`__end`, the first free address after the static data, where the heap
starts.

### Host services

`sys N` takes its arguments in `r1` and `r2` and returns its result in
`r0`. The table is the complete list.

| N | Service  | Arguments              | Result                  |
|---|----------|------------------------|-------------------------|
| 0 | exit     | `r1` = status          | does not return         |
| 1 | getc     | `r1` = fd              | byte, or -1             |
| 2 | putc     | `r1` = fd, `r2` = byte | the byte, or -1         |
| 3 | open     | `r1` = path, `r2` = mode | fd, or -1             |
| 4 | close    | `r1` = fd              | 0, or -1                |
| 5 | unlink   | `r1` = path            | 0, or -1                |
| 6 | rename   | `r1` = from, `r2` = to | 0, or -1                |

Descriptors 0, 1 and 2 are stdin, stdout and stderr. The mode is one of
`"r"`, `"w"`, `"a"` or `"u"` (update).

## The language

Programs are written in the Small-C subset. `examples/README` in the
small-c repository lists its rules. The rules that change on smol are
(complete list):

- An `int` is 64 bits. Constant expressions fold in 64 bits too.
- `#asm` blocks hold smol assembly.
- `main` receives `argc` and `argv` from the `smol` command line, and its
  return value is the exit status.
- `struct`, `union` and `sizeof` exist. See the next section.
- Local variables may have initialisers, which run where they are
  declared, as in C: `int i = 3, v[] = {i, i + 1};`. An initialiser of
  a local may be any expression.
- The conditional operator `c ? a : b` exists.
- A function's declared return type is kept. So the result of
  `char *f()` can be subscripted, as in `f()[1]`, and the result of
  `int *f()` is scaled in `f() + 1`.
- Names keep 31 significant characters, as C99 guarantees. The
  original kept 8.
- Function pointer types exist. See "Function pointers" below.
- A local may be `static`: it lives in static memory, keeps its value
  between calls, and only its block can name it. Its initialiser must
  be constant, as a global's must.
- A global variable or function may be `static`: its label is local to
  its file, so another file may use the same name. A function defined
  without `static` keeps the linkage of an earlier `static` prototype.
  A `static` declaration after a non-static one, or after a call that
  declared the function implicitly, is an error.
- `typedef` exists. See "Typedefs" below.

### Typedefs

A typedef name stands for any type a declaration can give, including
a pointer, an array, a function pointer and an anonymous struct:

	typedef int Num;
	typedef char Name[8];
	typedef struct node *Link;
	typedef int (*Binop)(int, int);
	typedef struct { int x, y; } Point;
	typedef Binop Ops[3];

A typedef name may then be used wherever `int` may: in globals,
locals, statics, parameters of either form, struct members and
`sizeof`. A typedef name is visible from its declaration to the end of
the file, even when it is declared inside a function.

Small-C pointers have one level and its arrays one dimension, and a
typedef does not change that: a pointer to a `Link`, an array of
`Link`, or a pointer to or array of `Name`, is an error.

### Function pointers

A function pointer is declared as in C, and may be a variable, an
array element, a struct member or a parameter:

	int (*op)(int, int) = add;
	int (*table[])(int, int) = {add, sub, &mul};
	struct cmd { char *name; int (*fn)(int, int); };
	struct pt (*make)(int, int);
	int apply(int (*f)(int, int), int a, int b);

A call through one, as `op(1, 2)`, `(*op)(1, 2)`, `table[i](1, 2)` or
`p->fn(1, 2)`, returns the declared type, so it may return a struct or
a pointer. A function's name, or `&` and the name, is its address. In a
global initialiser, a function's name is a constant. The parameter
types are skipped, because Small-C does not check calls against them.

### Structs and unions

Structs and unions are declared and used as in C:

	struct pt {
		int x, y;
	};

	struct pt
	add(struct pt a, struct pt b)
	{
		a.x += b.x;
		a.y += b.y;
		return a;
	}

The compiler supports the following:

- struct and union definitions, global or local, and a tag used before
  its members are known, as `struct node *next` inside `struct node`
  is;
- a struct or union definition inside another one's member list;
- global and local variables and arrays of structs and unions;
- members of type `char`, `int`, struct or union, arrays of any of
  these, and pointers, including pointers to structs;
- `.`, `->`, `&s`, `&s.m` and `*p`;
- `p + i`, `p - i`, `p[i]`, `++p`, `p - q` and comparisons, all scaled
  by the struct's size;
- `sizeof (type)` and `sizeof expression`;
- struct assignment, as `a = b`, `*p = f()` or `a = b = c`;
- struct parameters and arguments passed by value, in C99 and in K&R
  definitions;
- functions that return a struct, or a pointer to one, so `f().x` and
  `f()->x` work;
- initialisers for structs, unions and arrays of them, global or
  local, with or without inner braces.

An initialiser gives each struct in braces, with its members in order:

	struct item {
		char name[6];
		struct money { int units; char cur[4]; } price;
		char *note;
		int qty[3];
	};

	struct item shop[] = {
		{"apple", {3, "EUR"}, "red", {1, 2, 3}},
		{"pear", {5, "USD"}, 0, {4}},
	};

Members without a value are zero. A `char` array takes a string, and a
`char` pointer takes a string. A union takes one value, for its first
member. An array declared with `[]` takes its length from the
initialiser, global or local. A global's values must be constants,
because the assembler writes them into the data image. Small-C constant
expressions have no addresses, so a global pointer other than a `char`
pointer or a function pointer takes a constant only, such as 0.

The braces of an inner struct or array may be left out, as in C. Its
values then come from the enclosing list:

	struct line { struct pt a, b; char label[4]; };
	struct line l[] = {1, 2, 3, 4, "ab", 5, 6, {7, 8}, "cd"};

A local struct may also be initialised from a struct expression, as in
`struct pt q = mk(5, 6), r = q;`.

A struct or union member of type `int` or pointer is aligned to 8
bytes, and the size is rounded up to the alignment of the strictest
member. Every member of a union starts at offset 0. Tags, and each
struct's members, have their own name spaces.

Whole structs move as follows (complete list):

- an assignment copies the struct with `CCMOVE`, in `crt0.sas`;
- a by-value argument is copied onto the stack, rounded up to whole
  words;
- a function that returns a struct copies it into a static buffer of
  its own and returns the buffer's address. The caller copies or reads
  the struct before the next call to that function. So a pointer such
  as `&f().x` is valid only until then.

A function that returns a struct, or a pointer to one, must be defined
or declared with a prototype before a call to it. Otherwise the call
treats the result as an `int`, as Small-C does for any undeclared
function. A call through a function's address held in an `int`, rather
than in a function pointer, also returns an `int`.

## The runtime library

`lib/` provides these functions (complete list):

- Files: `fopen`, `freopen`, `fclose`, `fgetc`, `getc`, `getchar`,
  `ungetc`, `fputc`, `putc`, `putchar`, `fgets`, `gets`, `fputs`,
  `puts`, `read`, `fread`, `write`, `fwrite`, `feof`, `ferror`,
  `clearerr`, `unlink`, `rename`.
- Formatted I/O: `printf`, `fprintf`, `scanf`, `fscanf`. They support
  `%b`, `%c`, `%d`, `%o`, `%s`, `%u` and `%x`.
- Strings: `strcat`, `strncat`, `strchr`, `strrchr`, `strcmp`,
  `strncmp`, `strcpy`, `strncpy`, `strlen`, `reverse`, `left`, `pad`,
  `lexcmp`, `lexorder`.
- Characters: `isalnum`, `isalpha`, `isascii`, `iscntrl`, `isdigit`,
  `isgraph`, `islower`, `isprint`, `ispunct`, `isspace`, `isupper`,
  `isxdigit`, `toascii`, `tolower`, `toupper`.
- Numbers: `abs`, `sign`, `atoi`, `atoib`, `dtoi`, `otoi`, `utoi`,
  `xtoi`, `itoa`, `itoab`, `itod`, `itoo`, `itou`, `itox`.
- Memory and process: `malloc`, `calloc`, `free`, `avail`, `getarg`,
  `exit`, `abort`.

These behave as in the original Small-C library, with the following
exceptions (complete list):

- `exit(status)` ends the program with that status. The original sent
  a character to the console and always exited with status 0.
- There is no `seek`, `tell`, `rewind`, `isatty`, `iscons` or `poll`,
  because the VM offers no such host service.
- Text is not translated. The host's newline is `'\n'`.
- The heap still works like a stack. `free(p)` releases `p` and every
  block allocated after it.

## Known limits

These lists record every limitation known on 2026-10-07. Each list is
complete as far as I know.

### Language

The language is the Small-C subset with the extensions described
above. It lacks the following:

- the types `unsigned`, `long`, `short`, `float`, `double` and `enum`,
  and the qualifiers `const`, `volatile`, `register` and `auto`;
- casts;
- more than one level of pointers, and arrays of data pointers, such
  as `char *argv[]` (`argv` is declared `int argv[]` instead);
- arrays of more than one dimension;
- bit-fields;
- hexadecimal and octal integer constants: `0x10` is an error, and
  `010` is decimal 10;
- the escapes `\a`, `\r`, `\v` and `\x`. Only `\n`, `\t`, `\b`, `\f` and
  octal escapes such as `\0` are translated; any other character after
  a backslash stands for itself, so `'\\'` and `'\''` work but `'\r'`
  is `'r'`;
- variadic functions written in C: `printf()` and `scanf()` get their
  argument count from `CCARGC()`, a Small-C intrinsic;
- `//` comments.

The language also has these restrictions:

- An `int` is 64 bits and a `char` is a signed 8-bit value.
- A function that returns a struct, or a pointer to anything, must be
  defined or declared with a prototype before a call to it. Otherwise
  the call treats the result as an `int`, as C89 does for any
  undeclared function.
- Calls are not checked against the parameter types, and the
  parameter types of a function pointer are skipped.
- A call through a function's address held in an `int`, rather than in
  a function pointer, returns an `int`.
- A function may not return a function pointer by its own declarator,
  as in `int (*f(int))(int)`. It may through a typedef, but the result
  of such a call is then an `int`, so calling the result directly, as
  in `f(1)(2, 3)`, returns an `int`.
- A global's initialiser must be constant. A global pointer other than
  a `char` pointer or a function pointer can only be initialised with
  a constant, such as 0, and not with an address.
- A function that returns a struct returns the address of a static
  buffer of its own. A pointer into the result, such as `&f().x`, is
  valid only until the next call to that function.
- A typedef name is visible from its declaration to the end of the
  file, even when it is declared inside a block.
- Local declarations must come first in their block, and a `switch`
  body may not declare locals.
- A function cannot use both `goto` and locals declared in an inner
  block: whichever of them comes second is an error.
- A source line holds at most 510 characters, also after macro
  expansion. A longer line is an error.

### Preprocessor

The preprocessor handles `#include`, `#define`, `#ifdef`, `#ifndef`,
`#else` and `#endif`, and `#asm` with `#endasm`. It lacks the
following:

- macro parameters: `#define SQ(x) ((x)*(x))` gives no error, but it
  defines `SQ` as the text `(x) ((x)*(x))`;
- `#if`, `#elif`, `#undef`, `#error`, `#line` and `#pragma`;
- the `#` and `##` operators;
- a search path beyond the current directory and one `-I` directory;
  `#include` takes a name with or without quotes or angle brackets.

### Compiler tables

The compiler keeps each kind of item below in a table of fixed size.
"Most" is the largest number of such items that one source file may
hold. A file with more stops the compiler with an error.

| Item                                            | Most |
|-------------------------------------------------|-----:|
| global names, functions included                | 200 |
| local symbol table, in bytes                    | 1080 |
| struct and union tags                           | 99 |
| struct and union members, all tags together     | 400 |
| typedef names                                   | 100 |
| macros                                          | 130 |
| macro text, all macros together, in bytes       | 910 |
| cases in one `switch`                           | 60 |
| loops and switches nested in each other         | 9 |
| literal strings in one function, in bytes       | 4000 |
| code for one expression or local initialiser, in bytes | 65536 |
| strings and function addresses in one global's initialiser | 64 |
| loads of other locals in an unsized local array's initialiser | 200 |

A local symbol takes 11 bytes plus the length of its name, so the
local table holds about 60 names of 6 characters at a time.

### Runtime library

- The heap works like a stack: `free(p)` releases `p` and every block
  allocated after it.
- There is no `seek`, `tell`, `rewind`, `isatty`, `iscons` or `poll`,
  because the VM offers no such host service.
- Text is not translated: the host's newline is `'\n'`.
- `exit(status)` ends the program with that status, unlike the
  original, which printed a character and always exited with 0.
- At most 32 files are open at a time, the 3 standard ones included,
  and a path passed to the host holds at most 4095 characters.

### Assembler and virtual machine

- The VM has 16 MiB of data memory, shared by the static data, the
  heap and the stack. It stops with an error when a push or call would
  take the stack into the static data, or a program touches an address
  below 8 or beyond the end of memory. It does not check the space a
  function reserves for its locals, nor the heap against the stack;
  `avail()` and `malloc()` do the latter.
- A program has at most 262144 instructions, 16384 labels, 256 source
  files and 255 arguments.
- An assembly source line holds at most 510 characters and 64 tokens,
  a label at most 55 characters, and a `db` string at most 256 bytes.
- Integer overflow wraps around. Division by zero, and the most
  negative number divided by -1, stop the VM with an error.

`scc -o` turns on a peephole optimiser, and the Makefile uses it. It
makes two kinds of change (complete list):

- it merges the three instructions of a local variable access into
  one, and folds member offsets into it;
- it merges a comparison that only decides a jump, as in `if (a < b)`,
  from `cmp`, `setl`, `cmp` and `je` into `cmp` and `jge`.

On the examples and tests it removes 11 to 21 percent of the
instructions.
