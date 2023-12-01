all: identify-streams.so

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) -g -O0 -fPIC
CLANG_FLAGS = -static --target=riscv64 -march=rv64gc -std=c++11 -stdlib=libc++ -Xclang -disable-O0-optnone -fno-discard-value-names -O0 -emit-llvm -I/usr/include/c++/11/ -I/usr/include/x86_64-linux-gnu/c++/11/ -I/usr/include/

objects/identify-streams.o: src/identify-streams.cpp objects/loop-exposed-vars.o

objects/loop-exposed-vars.o: src/loop-exposed-vars.cpp src/loop-exposed-vars.h objects/dataflow.o

objects/dataflow.o: src/dataflow.cpp src/dataflow.h

objects/%.so: objects/%.o objects/dataflow.o objects/loop-exposed-vars.o
	$(CXX) -dylib -shared $^ -o $@

clean:
	rm -f objects/*.o *~ obects/*.so out ../tests/*.ll ../tests/*.bc

.PHONY: clean all test-identify-streams

tests/%.bc: tests/%.cpp
	clang++ $(CLANG_FLAGS) -c $< -o $@
#-o $@.no-m2r
#	opt -mem2reg -loop-rotate $@.no-m2r -o $@
#	rm $@.no-m2r

test-identify-streams: objects/identify-streams.so tests/test3.bc
	@echo "========== Identify Streams =========="
	@opt -mem2reg -loop-rotate -enable-new-pm=0 -load ./objets/identify-streams.so -identify-streams ./tests/test3.bc -o ./tests/test3_opt.bc
#	@llc -O0 -march=rv64gc
#	@clang++ -static --target=riscv64 -march=rv64gc -std=c++11 -stdlib=libc++ -Xclang -disable-O0-optnone -fno-discard-value-names -O0 -include ./uli.h -I/usr/include/c++/11/ -I/usr/include/x86_64-linux-gnu/c++/11/ -I/usr/include/ ../test3 $< -o $@.no-m2r

# compile uli.h:
# clang++ -Xclang -disable-O0-optnone -std=c++11 -static -fno-discard-value-names -O0 -emit-llvm -c uli.h -o uli.bc
