EXTFLAG=-DRHAPSODY_DEBUG

.PHONY: compile run clean run_cluster

all: compile 

compile:  src/utility.cc src/main.cc src/bankfunRE_2bits.cc
	mkdir -p bin
	g++ -std=c++20 -g -O0 src/main.cc src/utility.cc src/bankfunRE_2bits.cc  $(EXTFLAG) -o bin/main
	
2bit_test: src/2bit_test.cc src/utility.cc src/test_utility.cc
	mkdir -p bin
	g++ -std=c++20 -g -O0 src/2bit_test.cc src/utility.cc src/test_utility.cc $(EXTFLAG) -o bin/2bit_test

run:
	./scripts/run_REmain.sh | tee ./RE.log
		
clean:
	rm -rf bin

