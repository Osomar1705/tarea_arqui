.macro CHECK reg, expected
        li   t6, \expected
        addi gp, gp, 1
        bne  \reg, t6, fail
.endm

main:   li   gp, 0
        lui  t0, 0x12345
        CHECK t0, 0x12345000
here:   auipc t0, 0
        la   t1, here
        sub  t0, t0, t1
        CHECK t0, 0
        li   s1, -7
        addi t0, s1, 10
        CHECK t0, 3
        slti t0, s1, -6
        CHECK t0, 1
        sltiu t0, s1, 5
        CHECK t0, 0
        xori t0, s1, -1
        CHECK t0, 6
        ori  t0, s1, 0x0F
        CHECK t0, -1
        andi t0, s1, 0xFF
        CHECK t0, 0xF9
        slli t0, s1, 4
        CHECK t0, -112
        srli t0, s1, 28
        CHECK t0, 0xF
        srai t0, s1, 1
        CHECK t0, -4
        li   s2, 3
        add  t0, s1, s2
        CHECK t0, -4
        sub  t0, s2, s1
        CHECK t0, 10
        sll  t0, s2, s2
        CHECK t0, 24
        slt  t0, s1, s2
        CHECK t0, 1
        sltu t0, s1, s2
        CHECK t0, 0
        xor  t0, s1, s2
        CHECK t0, -6
        srl  t0, s1, s2
        CHECK t0, 0x1FFFFFFF
        sra  t0, s1, s2
        CHECK t0, -1
        or   t0, s1, s2
        CHECK t0, -5
        and  t0, s1, s2
        CHECK t0, 1
        li   s3, 0x2000
        li   t1, 0x8081F2F3
        sw   t1, 0(s3)
        lbu  t0, 0(s3)
        CHECK t0, 0xF3
        lb   t0, 0(s3)
        CHECK t0, -13
        lh   t0, 2(s3)
        CHECK t0, -32639
        lhu  t0, 2(s3)
        CHECK t0, 0x8081
        li   t1, 0x7A
        sb   t1, 1(s3)
        lw   t0, 0(s3)
        CHECK t0, 0x80817AF3
        li   t1, 0x1234
        sh   t1, 2(s3)
        lw   t0, 0(s3)
        CHECK t0, 0x12347AF3
        li   gp, 27
        beq  s2, s2, 1f
        j    fail
1:      bne  s1, s2, 1f
        j    fail
1:      blt  s1, s2, 1f
        j    fail
1:      bge  s2, s1, 1f
        j    fail
1:      bltu s2, s1, 1f
        j    fail
1:      bgeu s1, s2, 1f
        j    fail
1:      beq  s1, s2, fail
        bne  s2, s2, fail
        blt  s2, s1, fail
        bge  s1, s2, fail
        bltu s1, s2, fail
        bgeu s2, s1, fail
        li   gp, 28
        jal  ra, func
        CHECK a0, 42
        la   t1, func
        jalr ra, 0(t1)
        CHECK a0, 42
        addi x0, x0, 5
        CHECK x0, 0

        la   a0, ok_msg
        li   a7, 4
        ecall
        li   a0, 0
        li   a7, 17
        ecall

fail:   mv   a0, gp
        li   a7, 17
        ecall

func:   li   a0, 42
        ret

ok_msg: .asciz "OK: todas las pruebas pasaron\n"
