; computes fib(10) into r0, iteratively
main:
    mov r2, 10          ; n
    mov r0, 0           ; fib(i)
    mov r1, 1           ; fib(i+1)
fib_loop:
    cmp r2, 0
    je fib_end

    mov r3, r0          ; r3 = fib(i)
    add r3, r1          ; r3 = fib(i) + fib(i+1)

    mov r0, r1          ; fib(i)   = fib(i+1)
    mov r1, r3          ; fib(i+1) = r3

    dec r2              ; --n
    jmp fib_loop
fib_end:
    hlt
