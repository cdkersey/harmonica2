CXXFLAGS = -std=c++11
LDFLAGS = -lchdl
ARCH = 4w32/32/8

h2.vcd : h2 rom.hex
	./h2

h2: h2.cpp

rom.hex : rom.bin
	hexdump -v -e '1/4 "%08x" "\n"' rom.bin > rom.hex

rom.bin : rom.HOF
	harptool -L --arch $(ARCH) -o rom.bin rom.HOF

rom.HOF : rom.s
	harptool -A --arch $(ARCH) -o rom.HOF rom.s

clean:
	rm -f rom.hex rom.bin rom.HOF *~
