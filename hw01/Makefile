CXX=g++
LD=g++
AR=ar
CXXFLAGS=-std=c++20 -Wall -pedantic -O2
CXXFLAGSDEBUG=-std=c++20 -Wall -pedantic -g
SHELL:=/bin/bash
MACHINE=$(shell uname -m)-$(shell echo $$OSTYPE)

all: test.out

deps:
	g++ -MM *.cpp > Makefile.d

test.out: solution.o sample_tester.o
	$(LD) $(CXXFLAGSDEBUG) -o $@ $^ -L./$(MACHINE) -lprogtest_solver -lpthread

%.o: %.cpp
	$(CXX) $(CXXFLAGSDEBUG) -c -o $@ $<

lib: progtest_solver.o bigint.o
	mkdir -p $(MACHINE)
	$(AR) cfr $(MACHINE)/libprogtest_solver.a $^

clean:
	rm -f *.o test.out *~ core sample.tgz Makefile.d

pack: clean
	rm -f sample.tgz
	tar zcf sample.tgz --exclude progtest_solver.cpp --exclude Makefile.mingw --exclude bigint.cpp *


-include Makefile.d
