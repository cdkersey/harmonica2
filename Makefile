CXXFLAGS = -std=c++11 -O3 -DDEBUG
LDLIBS = -lchdl
ARCH = 4w8/8/1

all : h2.vcd

h2: regfile.o h2.o alu.o exec.o sched.o fetch.o mem.o muldiv.o branch.o
	$(CXX) $(LDFLAGS) -o h2 $^ $(LDLIBS)

h2.o: h2.cpp config.h interfaces.h
alu.o: alu.cpp config.h interfaces.h
regfile.o: regfile.cpp config.h interfaces.h
exec.o: exec.cpp config.h interfaces.h
sched.o: sched.cpp config.h interfaces.h
fetch.o: fetch.cpp config.h interfaces.h
mem.o: mem.cpp config.h interfaces.h
muldiv.o: muldiv.cpp config.h interfaces.h
branch.o: branch.cpp config.h interfaces.h

rom.hex : rom.bin
	hexdump -v -e '1/4 "%08x" "\n"' rom.bin > rom.hex
#       hexdump -v -e '2/4 "%08x " "\n"' rom.bin | \
#          sed 's/\([0-9a-f]*\) \([0-9a-f]*\)/\2\1/' > rom.hex

rom.bin : rom.HOF
	harptool -L --arch $(ARCH) -o rom.bin rom.HOF

rom.HOF : rom.s
	harptool -A --arch $(ARCH) -o rom.HOF rom.s

h2.vcd h2.crit : h2 rom.hex
	./h2

clean:
	rm -f h2 h2.vcd h2.crit *.o *~ rom.hex rom.bin rom.HOF
