; sums three static words into a fourth
a:      dq 10
b:      dq 20
c:      dq 12
result: dq 0

main:
    mov r0, [a]
    mov r1, [b]
    add r0, r1          ; r0 = a + b
    mov r1, [c]
    add r0, r1          ; r0 = a + b + c

    mov [result], r0
    hlt
