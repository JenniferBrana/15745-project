#!/usr/bin/env bash

THIS_FP=$(dirname $(readlink -f "$0"))

INP_NAME=$(basename -s .bc $1)
OPT_FP=${INP_NAME}_opt.bc
OUT_FP=${INP_NAME}_opt.o
EXE_FP=${INP_NAME}_opt

opt -enable-new-pm=0 -load "${THIS_FP}/src/identify-streams.so" -identify-streams "$1" -o "$OPT_FP"
llc -O0 -filetype=obj -march=riscv64 "$OPT_FP" -o "$OUT_FP"
clang -static --target=riscv64 -march=rv64gc -std=c++11 -O0 -lpthread -latomic "$OUT_FP" -o "$EXE_FP" ./src/uli.h ./src/trampoline.S
