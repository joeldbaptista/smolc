; computes 10! recursively into r0
main:
    mov r5, 10          ; n (first argument, System V-style convention)
    call factorial
    hlt                 ; result in r0

; input: r5 = n, output: r0 = n!
; r5 is caller-saved -- this routine is its own recursive call's
; caller, so it must save/restore r5 by hand around that call.
factorial:
    cmp r5, 1
    jg recurse

    mov r0, 1           ; base case: 0! = 1! = 1
    ret

recurse:
    push r5             ; save n (the recursive call reuses r5)

    dec r5              ; n - 1
    call factorial      ; r0 = (n-1)!

    pop r5              ; restore n
    mul r0, r5          ; r0 = n * (n-1)! = n!
    ret
