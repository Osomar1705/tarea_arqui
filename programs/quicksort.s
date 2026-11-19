main:
        li   s0, 0x1000
        li   t0, 6
        sw   t0, 0(s0)
        li   t0, 4
        sw   t0, 4(s0)
        li   t0, 3
        sw   t0, 8(s0)
        li   t0, 2
        sw   t0, 12(s0)
        li   t0, 1
        sw   t0, 16(s0)
        li   t0, 8
        sw   t0, 20(s0)
        li   t0, 9
        sw   t0, 24(s0)

        mv   a0, s0
        li   a1, 0
        li   a2, 6
        jal  ra, quicksort
done:   j    done

quicksort:
        bge  a1, a2, qs_ret
        addi sp, sp, -16
        sw   ra, 12(sp)
        sw   s1, 8(sp)
        sw   s2, 4(sp)
        sw   s3, 0(sp)
        mv   s1, a0
        mv   s2, a1
        mv   s3, a2
        jal  ra, partition
        mv   t6, a0
        mv   a0, s1
        mv   a1, s2
        addi a2, t6, -1
        addi sp, sp, -4
        sw   t6, 0(sp)
        jal  ra, quicksort
        lw   t6, 0(sp)
        addi sp, sp, 4
        mv   a0, s1
        addi a1, t6, 1
        mv   a2, s3
        jal  ra, quicksort
        lw   ra, 12(sp)
        lw   s1, 8(sp)
        lw   s2, 4(sp)
        lw   s3, 0(sp)
        addi sp, sp, 16
qs_ret: ret

partition:
        slli t0, a2, 2
        add  t0, a0, t0
        lw   t1, 0(t0)
        addi t2, a1, -1
        mv   t3, a1
p_loop: bge  t3, a2, p_end
        slli t4, t3, 2
        add  t4, a0, t4
        lw   t5, 0(t4)
        bgt  t5, t1, p_next
        addi t2, t2, 1
        slli a3, t2, 2
        add  a3, a0, a3
        lw   a4, 0(a3)
        sw   t5, 0(a3)
        sw   a4, 0(t4)
p_next: addi t3, t3, 1
        j    p_loop
p_end:  addi t2, t2, 1
        slli a3, t2, 2
        add  a3, a0, a3
        lw   a4, 0(a3)
        sw   t1, 0(a3)
        sw   a4, 0(t0)
        mv   a0, t2
        ret
