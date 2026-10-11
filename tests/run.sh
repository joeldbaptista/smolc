#!/bin/sh
# Run the smolc tests from the top directory, after "make".
#
# tests/NAME.c is run with NAME.args as its arguments and NAME.in as
# stdin, when those exist. Its stdout and exit status must match
# NAME.out. tests/asm/NAME.sas is assembled alone and run, and the
# machine state it leaves must match NAME.out.

smol=smol/smol
sas=smol/sas
tmp=${TMPDIR:-/tmp}/smolc-test.$$
fail=0
pass=0

for c in tests/*.c; do
	t=${c%.c}
	args=
	in=/dev/null
	[ -f "$t.args" ] && args=$(cat "$t.args")
	[ -f "$t.in" ] && in=$t.in
	# shellcheck disable=SC2086
	$smol "$t.smol" $args < "$in" > "$tmp" 2>&1
	echo "exit=$?" >> "$tmp"
	if cmp -s "$tmp" "$t.out"; then
		pass=$((pass + 1))
	else
		echo "FAIL $t"
		diff "$t.out" "$tmp"
		fail=$((fail + 1))
	fi
done

for s in tests/asm/*.sas; do
	t=${s%.sas}
	if $sas -o "$t.smol" "$s" && $smol -o "$tmp" "$t.smol" &&
	    cmp -s "$tmp" "$t.out"; then
		pass=$((pass + 1))
	else
		echo "FAIL $t"
		[ -f "$tmp" ] && diff "$t.out" "$tmp"
		fail=$((fail + 1))
	fi
done

rm -f "$tmp"
echo "tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
