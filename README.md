libSPEEDHACK
============

### A simple dynamic library to slowdown or speedup games on Linux

https://github.com/evg-zhabotinsky/libspeedhack

https://github.com/evg-zhabotinsky/libspeedhack/releases/

The main purpose of this rather simple library is to change speed at which
games run, for example, to make fights easier by slowing everything down and
giving yourself more time to react, or to get from point A to point B faster
in real time terms by speeding game up.

Think of it as CheatEngine's speedhack feature, but one that works on Linux.
I have written this to make Undertale less impossible for me to win. :smile:

It might have other uses, but I can't think of any just yet.
Apart from faking benchmark scores, of course. :smile:

### How to build

To build for your system's native architecture, run `make` or `make native`.  
On Debian and derivatives, you will need `build-essential` installed
(`sudo apt-get install build-essential`) for that to work.

On a multilib system, like most modern x86-64 installations,
some games may be using secondary architecture, like i386.  
To build both 32-bit and 64-bit libraries, run `make multilib`,
or separately `make 64bit` and/or `make 32bit`.  
On Debian and derivatives, you will also need `g++-multilib` installed
to build for the secondary architecture.

Run `make clean` to remove built libraries.


### How to use

If you simply ran `make` and didn't do anything fancy, this should work:

    libspeedhack_directory/speedhack path/to/executable [args]

Example with glxgears and this repo in home directory:

    ~/libspeedhack/speedhack glxgears

To control speed, write floating point speedup multiplier into
`/tmp/speedhack_pipe`. Example:

    echo 0.5 >/tmp/speedhack_pipe  # 2x slowdown

_Try to keep numbers short, or things might break._

To make it more practical, bind those `echo`s to keyboard shortcuts.
(Those are likely somewhere like _control panel > keyboard > shortcuts_)
Like `Win + 890-=` for multipliers .25, .5, 1, 2 and 4.

All this should work with Steam games too, if you wonder. Just open
launch options from game properties and put this there:

    $HOME/libspeedhack/speedhack %command%

Don't forget to substiture real path to `speedhack`.


### How it works

Libspeedhack consists of 2 parts:
the library itself and a wrapper script to make using it easier.

Library itself, `libspeedhack.so`, implements wrappers around various standard
functions that applications use to get timing information. It uses `dlsym()`
to get pointers to the real functions from system libraries, calls these
functions, and corrects timing values reported by them.

To make use of these wrappers, `libspeedhack.so` must be loaded into process
using `LD_PRELOAD`, like this:

    LD_PRELOAD="libspeedhack.so:$LD_PRELOAD" glxgears

Controlling how slow or fast things are is done by writing floating point
multiplier into control fifo, which is currently hardcoded to
`/tmp/speedhack_pipe`. It's kinda ugly as of now, use numbers as short as
possible or else things might break. Or just fix how the lib reads them.

Wrapper script `speedhack` just creates control pipe if it does not exist yet
and fills in LD_LIBRARY_PATH and LD_PRELOAD for library to work. Library
directories `lib32` and `lib64` must reside in the same directory as wrapper
script for it to be used as is.
