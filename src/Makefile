all: identify-streams.so

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) -g -O0 -fPIC

identify-streams.o: identify-streams.cpp loop-exposed-vars.o

loop-exposed-vars.o: loop-exposed-vars.cpp loop-exposed-vars.h dataflow.o

dataflow.o: dataflow.cpp dataflow.h

%.so: %.o dataflow.o loop-exposed-vars.o
	$(CXX) -dylib -shared $^ -o $@

clean:
	rm -f *.o *~ *.so out ../tests/*.ll ../tests/*.bc

.PHONY: clean all test-identify-streams

../tests/%.bc: ../tests/%.cpp
	clang++ -static --target=riscv64 -march=rv64gc -std=c++14 -stdlib=libc++ -Xclang -disable-O0-optnone -fno-discard-value-names -O0 -emit-llvm -include ./uli.h -I/usr/include/c++/11/ -I/usr/include/x86_64-linux-gnu/c++/11/ -I/usr/include/ -c $< -o $@.no-m2r
	opt -mem2reg -loop-rotate $@.no-m2r -o $@
	rm $@.no-m2r

test-identify-streams: identify-streams.so ../tests/test3.bc
	@echo "========== Identify Streams =========="
	@opt -enable-new-pm=0 -load ./identify-streams.so -identify-streams ../tests/test3.bc -o /dev/null


# compile uli.h:
# clang++ -Xclang -disable-O0-optnone -std=c++11 -static -fno-discard-value-names -O0 -emit-llvm -c uli.h -o uli.bc
