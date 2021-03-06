ifeq ($(DEBUG),y)
EXTRAFLAGS += -DDEBUG
endif

.PHONY: clean native multilib 32bit 64bit

.DEFAULT_GOAL = native

multilib: 64bit 32bit

native: ARCHFLAGS =
native: lib/libspeedhack.so
64bit: ARCHFLAGS = -m64
64bit: lib64/libspeedhack.so
32bit: ARCHFLAGS = -m32
32bit: lib32/libspeedhack.so

%/libspeedhack.so: libspeedhack.cpp
	mkdir -p $(@D)
	g++ -std=c++11 -Wall $(EXTRAFLAGS) $(ARCHFLAGS) -fPIC -shared $< -ldl -o $@

clean:
	rm -rf lib lib64 lib32
