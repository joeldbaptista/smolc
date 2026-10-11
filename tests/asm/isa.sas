; isa.sas -- every instruction and directive of the smol assembly.
; Each result lands in its own register or data word. The numbers on
; the right are instruction indexes, which code addresses are.

buf:
	resb 16
msg:
	db "h", -1, 0, 0, 0, 0, 0, 0
tab:
	dq main, buf + 8, -5
cnt:
	dq 7
out:
	dq 0

sub1:
	mov r0, 42              ; 0
	ret                     ; 1

main:
	mov r7, 65536           ; 2: a fixed stack, so the dump ignores argv
	mov r1, buf             ; 3: 8, the first data address
	mov r2, 300             ; 4
	mov [r1], r2            ; 5: mem[8] = 300
	mov [r1 + 8], r2        ; 6
	mov r4, 0xff            ; 7
	mov byte [r1 + 8], r4   ; 8: mem[16] = 256 + 255
	movsx r3, byte [r1 + 8] ; 9: sign extended to -1
	mov r4, msg             ; 10: 24
	movsx r5, byte [r4]     ; 11: 104
	mov r2, [r4 + 8]        ; 12: main = 2
	mov r6, [r4 + 16]       ; 13: buf + 8 = 16
	mov r14, [r4 + 24]      ; 14: -5
	mov r8, 6               ; 15
	and r8, 3               ; 16: 2
	or r8, 8                ; 17: 10
	xor r8, 15              ; 18: 5
	shl r8, 4               ; 19: 80
	neg r8                  ; 20: -80
	sar r8, 2               ; 21: -20
	not r8                  ; 22: 19
	mov [cnt], r8           ; 23: mem[56] = 19
	cmp r8, 19              ; 24
	sete r9                 ; 25: 1
	cmp r8, r9              ; 26: 19 against 1
	setl r10                ; 27: 0
	setge r11               ; 28: 1
	setg r12                ; 29: 1
	setle r13               ; 30: 0
	setne r15               ; 31: 1
	add r13, r15            ; 32: 1
	xchg r9, r10            ; 33: r9 = 0, r10 = 1
	mov r15, 47             ; 34
	rem r15, 10             ; 35: 7
	mul r15, 6              ; 36: 42
	sub r15, 2              ; 37: 40
	div r15, 8              ; 38: 5
	inc r15                 ; 39: 6
	mov [out], r15          ; 40: mem[64] = 6
	push r14                ; 41
	mov r14, 0              ; 42
	pop r14                 ; 43: -5 again
	mov r15, sub1           ; 44
	call r15                ; 45: r0 = 42
	mov r15, skip           ; 46: 49
	jmp r15                 ; 47
	mov r0, 0               ; 48: skipped
skip:
	cmp r0, 42              ; 49
	jne bad                 ; 50
	jge good                ; 51
bad:
	mov r0, -1              ; 52: not reached
good:
	hlt                     ; 53
