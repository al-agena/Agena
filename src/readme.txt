Compiling Agena
---------------

The sources have been compiled successfully on various GCC compilers (see below). If you use Windows,
you should use MinGW.

It is advised to use GCC 4.4.x or higher in order to get correct results with complex number arithmetic.

With all operating systems, first configure your installation by typing

           'make clean'
           'make config'

(without the quotes), which sets the endianness of your system and some other few parameters.

Depending on your operating system, now enter one of the following statements (without the quotes):

Solaris:   'make solaris' (with command line history)
Mac OS X:  'make macosx' (with command line history)
Windows:   'make mingw' (with command line history)
Linux:     'make linux' (with command line history)
Linux:     'make linuxplain' (without command line history)
Nexenta:   'make nexenta' (without command line history)
DOS:       see file readme.dos
OS/2:      see file readme.os2

To compile Agena with command line history on Solaris, Mac OS X, and Linux, you must have the ncurses
and readline packages installed.

For other operating systems see the Makefile.

The Plus Packages can be separately compiled with one of the `makeplus*` shell scripts you find in the
src folder.


Installation
------------

Although it is recommended to use the respective installer for your operating system, Agena can
be installed in two steps:

1) Type 'make install' for UNIX based systems. This installs the executable into the /usr/local/bin
   folder.

2) Since Agena uses a lot of functions written in the Agena language itself, also install all the
   files contained in the lib folder of the compressed source file agena-X.Y.Z-src.tar.gz
   - maintaining the subdirectory structure of this lib folder - as follows:

   In Solaris and other UNIX-based systems, create a folder /usr/agena and decompress all the
   subdirectories and files in the compressed lib directory into this new folder.

   In Windows, OS/2, and DOS, create a folder anywhere on your drive and decompress all the
   files in the compressed lib directory into this new folder. Also set the environment variable
   AGENAPATH as described in


Have fun !


Compiling Agena in DOS and OS/2
-------------------------------

Since I do not know how to create `DLLs` with current versions of DJGPP or GCC, the plus packages
have to be part of the agena.exe file and thus must be compiled into it with the following procedure:

1.  Make a backup of the Makefile in the src folder.

2a. If you use DOS, rename the 'makefile.dos' file to just 'makefile', overwriting the existing
    Makefile.

2b. If you use OS/2, rename the 'makefile.os2' file to just 'makefile', overwriting the existing
    Makefile.

3.  Run 'make config', followed by 'make dos' in DOS or 'make os2' in OS/2.

At least with Novell DOS 7, you must install CWSDPMI.EXE delivered with the DJPGG edition of GCC as
a TSR program before starting Agena. Novell DOS's command line history in Agena works correctly on
the Agena prompt.


Compatibility
-------------

Agena currently compiles and runs successfully on the following operating systems:

ArcaOS 5.x & eComStation with GCC 4.4.6 compiled by Paul Smedley
Windows 2000 SP 4 through Windows 11 with MinGW and GCC 9.2.0
Solaris 10 update 8 or higher for x86 PCs with GCC 6.3.0
Mac OS X i386 10.6.3 with GCC 4.2.1
OpenSUSE 10.3 with GCC 4.2.1 and GCC 4.4.3
Xubuntu 22.04 64-bit with GCC 11.4.0
FreeDOS 1.3 and Windows 2000 SP 4 with DJGPP/GCC 12.2.0

Agena in the past also compiled successfully on:

Solaris 10 updates 4, 5, 6 for Sparc with GCC 3.4.6
Solaris 10 update 6, 7 and 8 for Sparc with GCC for Sun Systems 4.2.0
Linpus Linux Lite v1.0.13E (Fedora 8 based) with GCC 4.1.2
Mac OS X i386 10.5 with GCC 4.0.1
Mac OS X PPC 10.5 with GCC 4.0.1
Nexenta 1.0.1 with GCC 4.0.3 (Nexenta is an OpenSolaris/Debian derivative)
Novell DOS 7.14 with GCC 4.3.2 (DJGPP v2) and GCC 4.4.1
OpenSUSE 11.2 with GCC 4.4.1
OS/2 Warp 4 (~ 4.5) with GCC 3.3.5, and 3.4.6
Ubuntu 8.10 with GCC 4.3.2
Windows NT 4.0 Workstation SP6a with MinGW and GCC 3.4.5 and GCC 4.4.0, and 4.5.2
Windows NT 4.0 Server SP6a with MinGW and GCC 3.4.5
Xubuntu 9.10 with GCC 4.4.1
Xubuntu 10.04 with GCC 4.4.3

Agena binaries compiled with DJGPP v2 for DOS ran successfully on:
FreeDOS 1.0 through 1.3


Remarks
-------

(none)