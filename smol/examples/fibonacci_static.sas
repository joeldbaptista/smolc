; computes fib(20) into r0, iteratively, holding the running pair
; of fibonacci values in static data instead of registers
fib0:
    dq 0                ; fib(i)
fib1:
    dq 1                ; fib(i+1)

main:
    mov r2, 20          ; n
fib_loop:
    cmp r2, 0
    je fib_end

    mov r0, [fib0]      ; r0 = fib(i)
    mov r1, [fib1]      ; r1 = fib(i+1)
    add r0, r1          ; r0 = fib(i) + fib(i+1)

    mov [fib0], r1      ; fib0 = fib(i+1)
    mov [fib1], r0      ; fib1 = fib(i) + fib(i+1)

    dec r2              ; --n
    jmp fib_loop
fib_end:
    mov r0, [fib0]      ; result: fib(20)
    hlt
