#!/bin/sh
set -e
cd "$(dirname "$0")"
for src in *.s; do
    name="${src%.s}"
    llvm-mc -triple=riscv32 -mattr=-relax,-c -filetype=obj "$src" -o "$name.o"
    ld.lld -m elf32lriscv -Ttext=0 -e 0 --no-relax "$name.o" -o "$name.elf"
    llvm-objcopy -O binary "$name.elf" "$name.bin"
    rm "$name.o"
    echo "$src -> $name.bin, $name.elf"
done
