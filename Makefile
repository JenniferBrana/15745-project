OBJ=objects
SRC=src
TESTS=tests

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) -g -O0 -fPIC
CLANG_FLAGS = -static --target=riscv64 -march=rv64gc -std=c++11 -stdlib=libc++ -Xclang -disable-O0-optnone -fno-discard-value-names -O0 -emit-llvm -I/usr/include/c++/11/ -I/usr/include/x86_64-linux-gnu/c++/11/ -I/usr/include/


all: $(OBJ)/identify-streams.so

#$(OBJ)/loop-exposed-vars.o
$(OBJ)/identify-streams.o: $(SRC)/identify-streams.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -c -o $@

# $(OBJ)/dataflow.o
$(OBJ)/loop-exposed-vars.o: $(SRC)/loop-exposed-vars.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -c -o $@

$(OBJ)/dataflow.o: $(SRC)/dataflow.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -c -o $@

$(OBJ)/identify-streams.so: $(OBJ)/identify-streams.o $(OBJ)/dataflow.o $(OBJ)/loop-exposed-vars.o
	$(CXX) -dylib -shared $^ -o $@

clean:
	rm -f $(OBJ)/*.o *~ $(OBJ)/*.so out $(TESTS)/*.ll $(TESTS)/*.bc

.PHONY: clean all test-identify-streams

$(TESTS)/%.bc: $(TESTS)/%.cpp
	clang++ $(CLANG_FLAGS) -c $< -o $@
#-o $@.no-m2r
#	opt -mem2reg -loop-rotate $@.no-m2r -o $@
#	rm $@.no-m2r

test-identify-streams: $(OBJ)/identify-streams.so $(TESTS)/test3.bc
	@echo "========== Identify Streams =========="
	@opt -mem2reg -loop-rotate -enable-new-pm=0 -load $(OBJ)/identify-streams.so -identify-streams $(TESTS)/test3.bc -o $(TESTS)/test3_opt.bc
#	@llc -O0 -march=rv64gc
#	@clang++ -static --target=riscv64 -march=rv64gc -std=c++11 -stdlib=libc++ -Xclang -disable-O0-optnone -fno-discard-value-names -O0 -include ./uli.h -I/usr/include/c++/11/ -I/usr/include/x86_64-linux-gnu/c++/11/ -I/usr/include/ ../test3 $< -o $@.no-m2r

# compile uli.h:
# clang++ -Xclang -disable-O0-optnone -std=c++11 -static -fno-discard-value-names -O0 -emit-llvm -c uli.h -o uli.bc
