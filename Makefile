all: 32bit 64bit

32bit: lib32/libspeedhack.so

lib32/libspeedhack.so: libspeedhack.cpp
	mkdir -p lib32
	g++ -std=gnu++14 -Wall -m32 -fPIC -shared -Wl,-init=init_libspeedhack $< -ldl -o $@

64bit: lib64/libspeedhack.so

lib64/libspeedhack.so: libspeedhack.cpp
	mkdir -p lib64
	g++ -std=gnu++14 -Wall -m64 -fPIC -shared -Wl,-init=init_libspeedhack $< -ldl -o $@

clean:
	rm -rf lib32 lib64

