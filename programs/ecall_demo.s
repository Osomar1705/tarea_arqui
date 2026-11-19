main:   la   a0, prompt
        li   a7, 4
        ecall
        li   a7, 5
        ecall
        mv   s0, a0
        li   s1, 0
        li   s2, 1
loop:   blez s0, end
        mv   a0, s1
        li   a7, 1
        ecall
        li   a0, ' '
        li   a7, 11
        ecall
        add  t0, s1, s2
        mv   s1, s2
        mv   s2, t0
        addi s0, s0, -1
        j    loop
end:    li   a0, '\n'
        li   a7, 11
        ecall
        la   a0, bye
        li   a7, 4
        ecall
        li   a7, 10
        ecall

prompt: .asciz "Cuantos terminos de Fibonacci? "
bye:    .asciz "Fin.\n"
