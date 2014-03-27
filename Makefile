CXXFLAGS = -std=c++11 -O3 -DDEBUG
LDLIBS = -lchdl
ARCH = 4w8/8/1

h2.vcd : h2 rom.hex
	./h2

h2: h2.cpp config.h interfaces.h
	$(CXX) $(CXXFLAGS) -o h2 h2.cpp $(LDLIBS)

rom.hex : rom.bin
	hexdump -v -e '1/4 "%08x" "\n"' rom.bin > rom.hex
#       hexdump -v -e '2/4 "%08x " "\n"' rom.bin | \
#          sed 's/\([0-9a-f]*\) \([0-9a-f]*\)/\2\1/' > rom.hex

rom.bin : rom.HOF
	harptool -L --arch $(ARCH) -o rom.bin rom.HOF

rom.HOF : rom.s
	harptool -A --arch $(ARCH) -o rom.HOF rom.s

clean:
	rm -f h2 h2.vcd *.o *~ rom.hex rom.bin rom.HOF
