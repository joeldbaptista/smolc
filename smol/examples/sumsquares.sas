; computes sum(i^2, i=1..10) into r0
main:
    mov r1, 42          ; sentinel: must still be 42 after the call
    mov r2, 10          ; n = 10
    call sumsquares
    hlt

; input: r2 = n, output: r0 = sum(i^2, i=1..n)
; r1 is callee-saved -- must be preserved across this call
sumsquares:
    push r1

    mov r0, 0
    mov r1, 1
sumsquares_loop:
    cmp r1, r2
    jg sumsquares_end

    mov r3, r1          ; r3 = i
    mul r3, r1          ; r3 = i * i
    add r0, r3          ; sum += i*i

    inc r1              ; ++i
    jmp sumsquares_loop

sumsquares_end:
    pop r1
    ret
