mkfloppy                                                  bumbread / 2021-12-19
-------------------------------------------------------------------------------

mkfloppy is a command line utility that lets you create FAT12-formatted floppy
disk images without mounting devices, using loop devices etc. This utility is
made to work under Windows and Linux, and maybe other POSIX-compliant operating
systems.

Note, this program takes some simplifying assumptions into account:
    
    Sector size:   512 bytes
    Cluster size:  1   sector
    FAT start sec: 1
    Root Dir ents: 224

Usage:
------

  mkfloppy [-b bootfile] [-d directory] destination

In general this command creates file "destination" of size of exactly 1440 KB.
if bootfile is specified, it is patched to the first 512 bytes of the
destination. Make sure the bootfile contains the correct BIOS Parameter Block
filled out. This program does not check the BPB, and assumes the values
provided above. If directory is specified, the contents of that directory are
recursively copied into the new FAT-12 filesystem.

Make sure that every file stays within the following limitations:

    Filename length:  8 ASCII characters
    Extension length: 3 ASCII characters
    Directory length: <224 files
    Total disk size:  1440 KB

These limitations are not specific to this program, but rather inheret
limitations of the FAT-12 filesystem. If either of the limitations is exceeded
this program terminates with an error code 1.

Build:
------

On Linux make sure you got gcc installed. Clone this repository and run make

    $ ./make

On Windows, launch Visual Studio Developer's command prompt. Use Start menu to
find it. Navigate to the directory where this repository is cloned and run
make.bat

    > make

Bugs:
------

It should be possible in theory make a directory with short name length of 11,
since directories don't have extensions. But this program treats directories as
normal files, meaning they also obey the 8.3 rule. If you want to make
directory with short name longer than 8 characters no matter what put a dot
after the 8-th character like this:

    mydirnam.e

Corresponds to directory name mydirname.
