all: identify-streams.so

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) -g -O0 -fPIC

identify-streams.o: identify-streams.cpp

%.so: %.o
	$(CXX) -dylib -shared $^ -o $@

clean:
	rm -f *.o *~ *.so out ../tests/*.ll ../tests/*.bc

.PHONY: clean all test-identify-streams

#test-liveness: liveness.so
#	opt -enable-new-pm=0 -load ./liveness.so -liveness ../tests/liveness-test-m2r.bc -o out

#test-available: available.so
#	opt -enable-new-pm=0 -load ./available.so -available ../tests/available-test-m2r.bc -o out

../tests/%.bc: ../tests/%.c
	clang -Xclang -disable-O0-optnone -fno-discard-value-names -O0 -emit-llvm -c $< -o $@.no-m2r
	opt -mem2reg -loop-rotate $@.no-m2r -o $@
	rm $@.no-m2r

test-identify-streams: identify-streams.so ../tests/test2.bc
	@echo "========== Identify Streams =========="
	@opt -enable-new-pm=0 -load ./identify-streams.so -identify-streams ../tests/test2.bc -o /dev/null
