# A1Emu
Console based Apple 1 emulator for Windows

To build type

```make```

which outputs an exectable named A1Emu.exe

NOTE: `WOZMON` and `WOZACI` ROMs are needed to proceed. They are not provided in this repository.

Next go to the working directory and copy a ROM for Integer Basic named "BASIC" (no extension) into that directory. The emulator will copy the first 4KB of that file's contents into address 0xE000.

The emulator runs in a normal console without any GUI, just like the original Apple 1 worked. You are greeted with a `\` character. Type the following to start Apple 1 BASIC:

```
\
E000R

E000: 4C
>
```

The cursor should now be blinking after the `>` character. Type commands like

```
>PRINT "HELLO WORLD"
HELLO WORLD

>
```

If you want to load cassette storage into memory, you're going to need to uze Woz'z ACI. To do that, run from Woz's monitor (not BASIC)
```
C100R
```
to start the cassette interface. Now suppose we wanted to store the contents of memory from address `0xE000` to `0xEFFF`. To do that, you would type afterwards
```
E000.EFFFW
```
Hit enter after typing the command. Now, within 10 seconds type the `|` (pipe) character to pause the emulator. The emulator will display the last reads and writes into memory of the previous opcode. Now type
```
tstart write
resume
```
which starts the emulated cassette. After waiting a few seconds, you will be greeted with a `\` prompt again, meaning you have returned to the monitor. Type `|` once again, but this time, type the following 2 commands
```
tstop
tstore SOME_FILE
resume
```
This stores in the same directory as the executable a file named `SOME_FILE` which can be read from using the cassette interface. 

Suppose we wanted to read that file into the same memory addresses we had stored it from. First, we would enter the ACI again using
```
C100R
```
and then type
```
E000.EFFFR
```
but DON'T PRESS ENTER. We need to start the cassette tape playing back again before we finish giving this command to the ACI. Type `|` to pause the emulator. Then run
```
tload SOME_FILE
tstart read
resume
```
After entering the `resume` command, press enter one more time to finish telling the ACI to read to `0xE000`. After a few seconds, you should be greeted by `\`. At this point, press `|` to pause emulation and type
```
tstop
resume
```
The contents of addresses `0xE000` to `0xEFFF` should now be the same as the contents originally stored onto the file using the ACI. 

It functions exactly like the original Apple 1. To learn how to use Apple 1 basic, go here: https://archive.org/details/apple1_basic_manual/page/n11

Here is a good place to learn more about the Apple 1 computer: https://www.sbprojects.net/projects/apple1/
