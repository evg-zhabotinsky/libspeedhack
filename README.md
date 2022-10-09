libSPEEDHACK
============

### A simple dynamic library to slowdown or speedup games on Linux

Source code:  
<https://github.com/evg-zhabotinsky/libspeedhack>

**Download binaries ready for use** (should work in most cases) **here:**  
<https://github.com/evg-zhabotinsky/libspeedhack/releases/>

AUR package:  
<https://aur.archlinux.org/packages/libspeedhack-git/>

The main purpose of this rather simple library is to change the speed at which
games run, for example, to make fights easier by slowing everything down and
giving yourself more time to react, or to get from point A to point B faster
in real-time terms by speeding the game up.

Think of it as CheatEngine's speedhack feature, but one that works on Linux.
I have written this to make Undertale less impossible for me to win. :smile:

It might have other uses, but I can't think of any just yet.
Apart from faking benchmark scores, of course. :smile:


### Caveats

Due to how this hack library works, it needs to intercept
*every single time-getting function* that the game in question uses
to properly adjust the time seen by the game.  
*Unfortunately*, there are *tons* of such functions,
so this library intercepts only those I found
and will need updates to work with some games.  
*Moreover*, it is possible to get system time
without calling any library function by directly using a syscall,
and in such cases this library's approach cannot work at all.

All that means that the library will work well for some games (e.g. Undertale),
but for others, it will cause glitches (like occasional crashes or hangs)
due to not intercepting some of the functions that the game uses.
(For example, in Half-Life: Source slowing time
 for more than a few minutes usually makes it hang and require force-close.)
And in some games, it will not work at all,
plus for some of them, this is impossible to fix.

Having said that, there is a "compatibility list" at
<https://github.com/evg-zhabotinsky/libspeedhack/wiki>.
If you try a game not listed there, feel free to add it if you have time.

One more note: games handle time differently.  
If you make a game think that time passes 100 times faster,
it will also think your computer is 100 times slower,
i.e. a 30MHz piece-of-crap CPU with 0.6 FPS screen update rate,
so it might "lag as hell", resulting in net speedup of only 5x or so.  
And if you make it think that time goes 100 times slower,
it might still assume something like "VSync is around 60Hz"
and base its timings off that, resulting in little or no slowdown.  
Usually, speed multipliers between 0.01 and 10 work fine. YMMV

This library can only be used with *one process at a time*,
due to how it is controlled, otherwise each command is received
only by a single randomly chosen process.  
*Beware!* Many games are launched through some sort of "launcher".
*If, for whatever reason, it gets time while the game is running,
 that violates the above restriction and it will swallow commands at random!*
Shell scripts are usually ok, but anything with GUI is suspect.
For example, you can't run whole Steam with libspeedhack,
you have to change launch options for the specific game.


### How to build

**1. Don't!**  
Not before checking if there is a prebuilt release that works on your system!  
<https://github.com/evg-zhabotinsky/libspeedhack/releases/>  
Failing that, read on to build it yourself.

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

If you simply ran make and didn't do anything fancy, this should work:

    libspeedhack_directory/speedhack path/to/executable [args]

Example with glxgears and this repo in the home directory:

    ~/libspeedhack/speedhack glxgears

To control speed, write floating point speedup multiplier into
`/tmp/speedhack_pipe`. Example:

    echo 0.5 >/tmp/speedhack_pipe  # 2x slowdown

To make it more practical, bind those `echo`s to keyboard shortcuts.
(Those are likely somewhere like _control panel > keyboard > shortcuts_)
Like `Win + 890-=` for multipliers .25, .5, 1, 2 and 4.

All this should work with Steam games too, if you wonder.
Just open launch options from game properties and put this there:

    $HOME/libspeedhack/speedhack %command%

Don't forget to substitute the real path to the `speedhack` script.


### How it works

Libspeedhack consists of 2 parts:
the library itself and a wrapper script to make using it easier.

Library itself, `libspeedhack.so`, implements wrappers around various standard
functions that applications use to get timing information. It uses `dlsym()`
to get pointers to the real functions from system libraries, calls these
functions, and corrects timing values reported by them.

To make use of these wrappers, `libspeedhack.so` must be loaded into the process
using `LD_PRELOAD`, like this:

    LD_PRELOAD="libspeedhack.so:$LD_PRELOAD" glxgears

Controlling how slow or fast things are is done by writing floating point
multiplier into control FIFO, which is currently hardcoded to
`/tmp/speedhack_pipe`. It's kinda ugly but works.

Wrapper script `speedhack` just recreates the control pipe
and fills in LD_LIBRARY_PATH and LD_PRELOAD for the library to work.
Library directories `lib` and/or `lib32` and `lib64` must reside in the same
directory as wrapper script for it to be used without modifications.

