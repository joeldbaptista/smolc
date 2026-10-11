; crt0 - smolc program start and the compiler's support routines
;
; The VM starts at main with r1 = argc and r2 = argv. The C main()
; is _main, because the compiler prefixes every C name with '_'.

main:
	mov r0, __end           ; the heap starts after the static data
	mov [_Umemptr], r0
	push r1                 ; argc
	push r2                 ; argv
	mov r3, 2
	call _main
	add r7, 16
	mov r1, r0              ; main's return value is the exit status
	sys 0

; CCARGC() returns the argument count the caller left in r3.
_CCARGC:
	mov r0, r3
	ret

; CCSWITCH matches r0 against the table at r1: pairs of words holding
; a case label and its value, ended by a zero word. On a match it
; jumps to the case, dropping its own return address. Otherwise it
; returns, and the caller goes on to the default case.
_CCSWITCH:
	mov r2, [r1]
	cmp r2, 0
	je .nomatch
	mov r3, [r1 + 8]
	cmp r3, r0
	je .match
	add r1, 16
	jmp _CCSWITCH
.match:
	pop r3
	jmp r2
.nomatch:
	ret

; CCMOVE copies r2 bytes from address r0 to address r1, a word at a
; time while 8 or more remain. It copies structs. r0 is unchanged; r1,
; r2, r4 and r5 are not.
_CCMOVE:
	mov r4, r0
.words:
	cmp r2, 8
	jl .bytes
	mov r5, [r4]
	mov [r1], r5
	add r4, 8
	add r1, 8
	sub r2, 8
	jmp .words
.bytes:
	cmp r2, 0
	jle .done
	movsx r5, byte [r4]
	mov byte [r1], r5
	inc r4
	inc r1
	dec r2
	jmp .bytes
.done:
	ret

; CCZERO clears r2 bytes from address r1. It initializes local
; aggregates. r1, r2 and r4 are not kept.
_CCZERO:
	mov r4, 0
.zwords:
	cmp r2, 8
	jl .zbytes
	mov [r1], r4
	add r1, 8
	sub r2, 8
	jmp .zwords
.zbytes:
	cmp r2, 0
	jle .zdone
	mov byte [r1], r4
	inc r1
	dec r2
	jmp .zbytes
.zdone:
	ret
