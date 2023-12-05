#!/usr/bin/env bash

ORIG_FP=$(pwd)
THIS_FP=$(dirname $(readlink -f "$0"))

INP_NAME=$(basename -s .bc $1)
OPT_FP=${INP_NAME}_opt.bc
OUT_FP_S=${INP_NAME}_opt.S
OUT_FP=${INP_NAME}_opt.o
EXE_FP=${INP_NAME}_opt
#CLANG_FLAGS=-static --target=riscv64 -march=rv64gc -std=c++11 -stdlib=libc++ -Xclang -disable-O0-optnone -fno-discard-value-names -O0 -emit-llvm -I/usr/include/c++/11/ -I/usr/include/x86_64-linux-gnu/c++/11/ -I/usr/include/

cd "$THIS_FP"
make
cd "$ORIG_FP"

echo opt -mem2reg -loop-rotate -enable-new-pm=0 -load "${THIS_FP}/objects/identify-streams.so" -identify-streams "$1" -o "$OPT_FP"
opt -mem2reg -loop-rotate -enable-new-pm=0 -load "${THIS_FP}/objects/identify-streams.so" -identify-streams "$1" -o "$OPT_FP"

llc -O0 -filetype=asm -march=riscv64 "$OPT_FP" -o "$OUT_FP_S"

clang++ --target=riscv64 -march=rv64gc -std=c++11 -Xclang -disable-O0-optnone -fno-discard-value-names -O0 "$OUT_FP_S" -c -o "$OUT_FP"

riscv64-unknown-linux-gnu-g++ "$OUT_FP" "${THIS_FP}/uli/trampoline.S" "${THIS_FP}/objects/handler.o" -lpthread -latomic -O0 -static -o "$EXE_FP"

#~/riscv-gnu-toolchain/RISCV/bin/qemu-riscv64 ./linkedlist_opt

# -L/usr/lib/x86_64-linux-gnu -lpthread -latomic
#clang++ -v -static --target=riscv64 -march=rv64gc -std=c++11 -O0 -Wl,--whole-archive -lpthread -latomic -Wl,--no-whole-archive -L/usr/lib/x86_64-linux-gnu "$OUT_FP" "${THIS_FP}/objects/uli.o" "${THIS_FP}/objects/trampoline.o" -o "$EXE_FP"

#-I/usr/include/c++/11/ -I/usr/include/x86_64-linux-gnu/c++/11/ -I/usr/include/
# Simulator: ~/riscv-gnu-toolchain/RISCV/bin/qemu-riscv64

#opt -mem2reg -loop-rotate -enable-new-pm=0 -load objects/identify-streams.so -identify-streams tests/test3.bc -o test3_opt.bc

#opt -mem2reg -loop-rotate -enable-new-pm=0 -load objects/identify-streams.so -identify-streams tests/test3.bc -o tests/test3_opt.bc
