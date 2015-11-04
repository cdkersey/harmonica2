CXXFLAGS += -std=c++11 -g #-O3
LDLIBS += -lchdl -lchdl-module

h2: regfile.o h2.o alu.o exec.o fpu.o sched.o fetch.o mem.o muldiv.o branch.o \
    barrier.o
	$(CXX) $(LDFLAGS) -o h2 $^ $(LDLIBS)

h2.o: h2.cpp config.h interfaces.h util.h
alu.o: alu.cpp config.h interfaces.h
regfile.o: regfile.cpp config.h interfaces.h
exec.o: exec.cpp config.h interfaces.h
fpu.o: fpu.cpp config.h interfaces.h
sched.o: sched.cpp config.h interfaces.h
fetch.o: fetch.cpp config.h interfaces.h
mem.o: mem.cpp config.h interfaces.h util.h
muldiv.o: muldiv.cpp config.h interfaces.h
branch.o: branch.cpp config.h interfaces.h
barrier.o: barrier.cpp config.h interfaces.h

clean:
	rm -f h2 *.vcd h2.crit h2.nand h2.v *.o *~ *.hex *.bin *.HOF *.out
