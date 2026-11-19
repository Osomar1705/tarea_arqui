main:
        li   s0, 0x1000
        li   t0, 6
        sw   t0, 0(s0)
        li   t0, 4
        sw   t0, 4(s0)
        sw   t0, 8(s0)
        li   t0, 1
        sw   t0, 12(s0)
        li   t0, 2
        sw   t0, 16(s0)
        sw   t0, 20(s0)
        li   t0, 1
        sw   t0, 24(s0)

        mv   a0, s0
        li   a1, 7
        jal  ra, is_symmetric
done:   j    done

is_symmetric:
        beqz a1, sym_true
        li   a2, 1
        li   a3, 2
        j    mirror
sym_true:
        li   a0, 1
        ret

mirror:
        slt  t0, a2, a1
        slt  t1, a3, a1
        or   t2, t0, t1
        beqz t2, m_true
        and  t2, t0, t1
        beqz t2, m_false
        slli t0, a2, 2
        add  t0, a0, t0
        lw   t0, 0(t0)
        slli t1, a3, 2
        add  t1, a0, t1
        lw   t1, 0(t1)
        bne  t0, t1, m_false

        addi sp, sp, -16
        sw   ra, 12(sp)
        sw   a0, 8(sp)
        sw   a2, 4(sp)
        sw   a3, 0(sp)
        slli a2, a2, 1
        addi a2, a2, 1
        slli a3, a3, 1
        addi a3, a3, 2
        jal  ra, mirror
        beqz a0, m_pop
        lw   a0, 8(sp)
        lw   a2, 4(sp)
        lw   a3, 0(sp)
        slli a2, a2, 1
        addi a2, a2, 2
        slli a3, a3, 1
        addi a3, a3, 1
        jal  ra, mirror
m_pop:  lw   ra, 12(sp)
        addi sp, sp, 16
        ret
m_true: li   a0, 1
        ret
m_false:
        li   a0, 0
        ret
