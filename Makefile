ifeq ($(DEBUG),y)
EXTRAFLAGS += -DDEBUG
endif

ifeq ($(PORTABLE_X86),y)
# I'm not sure how "portable" the build flags are,
# but the resulting libraries should work on any newer system
# than the one they were built on (within reason).
# So for extra portability, build on the oldest system you can.
EXTRAFLAGS += -nodefaultlibs -static-libgcc -static-libstdc++
EXTRALIBS += -lc -Wl,-Bstatic -lstdc++ -lgcc -lgcc_eh
else
EXTRALIBS += -ldl
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
	g++ -std=c++11 -Wall $(EXTRAFLAGS) $(ARCHFLAGS) -fPIC -shared $< $(EXTRALIBS) -o $@

clean:
	rm -rf lib lib64 lib32
