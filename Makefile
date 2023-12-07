OBJ=objects
SRC=src
TESTS=tests
ULI=uli

ALL_TESTS=$(wildcard $(TESTS)/*.cpp)
ALL_TESTS_BC=$(patsubst %.cpp, %.bc, $(ALL_TESTS))

LIB_FLAGS = -I/usr/include/c++/11/ -I/usr/include/ -I/usr/include/x86_64-linux-gnu/c++/11/
CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) -g -O0 -fPIC
CLANG_FLAGS = -static --target=riscv64 -march=rv64gc -std=c++11 -stdlib=libc++ -Xclang -disable-O0-optnone -fno-discard-value-names -O0 -emit-llvm $(LIB_FLAGS)


all: $(OBJ)/identify-streams.so $(OBJ)/handler.o

#$(OBJ)/loop-exposed-vars.o
$(OBJ)/identify-streams.o: $(SRC)/identify-streams.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -c -o $@

# $(OBJ)/dataflow.o
$(OBJ)/loop-exposed-vars.o: $(SRC)/loop-exposed-vars.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -c -o $@

$(OBJ)/dataflow.o: $(SRC)/dataflow.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -c -o $@

$(OBJ)/offload.o: $(SRC)/offload.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -c -o $@

$(OBJ)/handler.o: $(ULI)/handler.cpp
	clang++ -pthread --target=riscv64 -march=rv64gc -std=c++11 -Xclang -disable-O0-optnone -fno-discard-value-names -O0 $(LIB_FLAGS) $^ -c -o $@


$(OBJ)/identify-streams.so: $(OBJ)/identify-streams.o $(OBJ)/dataflow.o $(OBJ)/loop-exposed-vars.o $(OBJ)/offload.o
	$(CXX) -dylib -shared $^ -o $@

tests: $(ALL_TESTS_BC)


clean:
	rm -f $(OBJ)/*.o *~ $(OBJ)/*.so out $(TESTS)/*.ll $(TESTS)/*.bc

.PHONY: clean all test-identify-streams tests

$(TESTS)/%.bc: $(TESTS)/%.cpp
	clang++ -pthread $(CLANG_FLAGS) -c $< -o $@
#-o $@.no-m2r
#	opt -mem2reg -loop-rotate $@.no-m2r -o $@
#	rm $@.no-m2r

test-identify-streams: $(OBJ)/identify-streams.so $(TESTS)/test3.bc
	@echo "========== Identify Streams =========="
	opt -mem2reg -loop-rotate -enable-new-pm=0 -load $(OBJ)/identify-streams.so -identify-streams $(TESTS)/test3.bc -o $(TESTS)/test3_opt.bc
#	@llc -O0 -march=rv64gc
#	@clang++ -static --target=riscv64 -march=rv64gc -std=c++11 -stdlib=libc++ -Xclang -disable-O0-optnone -fno-discard-value-names -O0 -include ./uli.h -I/usr/include/c++/11/ -I/usr/include/x86_64-linux-gnu/c++/11/ -I/usr/include/ ../test3 $< -o $@.no-m2r

# compile uli.h:
# clang++ -Xclang -disable-O0-optnone -std=c++11 -static -fno-discard-value-names -O0 -emit-llvm -c uli.h -o uli.bc
