#!/usr/bin/env bash

INP_NAME=$(basename -s .cpp $1)
BC_FP=${INP_NAME}.bc
OPT_FP=${INP_NAME}_opt.bc
OUT_FP_S=${INP_NAME}_opt.S
OUT_FP=${INP_NAME}_opt.o
EXE_FP=${INP_NAME}_opt

LIB_FLAGS="-I/usr/include/c++/11/ -I/usr/include/ -I/usr/include/x86_64-linux-gnu/c++/11/"
CLANG_FLAGS="-static --target=riscv64 -march=rv64gc -std=c++11 -stdlib=libc++ -Xclang -disable-O0-optnone -fno-discard-value-names -O0 -emit-llvm $LIB_FLAGS"


ORIG_FP=$(pwd)
THIS_FP=$(dirname $(readlink -f "$0"))
cd "$THIS_FP"
make --quiet
cd "$ORIG_FP"

clang++ -pthread $CLANG_FLAGS -c "$1" -o "$BC_FP"

opt -mem2reg -loop-rotate -enable-new-pm=0 -load "${THIS_FP}/objects/identify-streams.so" -identify-streams "$BC_FP" -o "$OPT_FP"

llc -O0 -filetype=asm -march=riscv64 "$OPT_FP" -o "$OUT_FP_S"

clang++ --target=riscv64 -march=rv64gc -std=c++11 -Xclang -disable-O0-optnone -fno-discard-value-names -pthread -O0 "$OUT_FP_S" -c -o "$OUT_FP"

riscv64-unknown-linux-gnu-g++ -pthread "$OUT_FP" "${THIS_FP}/objects/handler.o" "${THIS_FP}/uli/trampoline.S" -std=c++11 -Wl,--whole-archive -lpthread -latomic -Wl,--no-whole-archive -O0 -static -o "$EXE_FP"
