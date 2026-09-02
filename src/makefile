# makefile for building Agena
# Integrated with DD precision logic - April 2026
#
# For Dr. Memory issue:
# make clean && make config && make drmem -j8
# make -f makefile.pluswin DEBUG_BUILD=1 clean
# make -f makefile.pluswin DEBUG_BUILD=1
#
# Run in ../testall folder, `./logs` must be an existing folder:
# drmemory -no_check_uninitialized -logdir ./logs -suppress ..\src\drmemory.cfg -debug -brief -- ..\src\agena.exe
# See src/drmemory.cfg for more info on the call to Dr. Memory.

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

# Your platform. See PLATS for possible values.
PLAT= none

CC= gcc
# -Wtype-limits is great to check for signed values compared to unsigned ones
# CFLAGS= -Wall -Wtype-limits -Wno-unknown-pragmas $(MYCFLAGS)
CFLAGS= -Wall -Wextra -Wno-unknown-pragmas $(MYCFLAGS)
#AR= ar rcu
AR= ar rc
RANLIB= ranlib
RM= rm -f
LIBS= -lm $(MYLIBS)

# C99 for variable-length arrays (VLAs)
MYCFLAGS= -g
MYLDFLAGS=

MYLIBS= -lz -lexpat -lmpfr -lpcre2-posix -lpcre2-8

WINSYSLIBS= -lole32 -luuid -lgdi32 -lpowrprof -lnetapi32 -lkernel32 -lpsapi \
            -lversion -lsetupapi -liphlpapi -lmpr -lws2_32 -lmswsock -lm

SOLSYSLIBS= -lm -lc -lreadline -lcurses -lkstat -lsocket -lnsl \
            -lpcre2-posix -lpcre2-8 -lz -lexpat -liconv -lrt \
            -lX11 -lXext -lXrandr

LINSYSLIBS= -ldl -lreadline -lhistory -lncurses -lrt -lexpat -lpcre2-posix -lpcre2-8 -lX11 -lXrandr

# Specific flags for Double-Double precision logic to prevent
# compiler "simplification" of high-precision math.
# These flags are ONLY for dd.c
# -ffloat-store: Prevents the CPU from keeping floating-point variables in high-precision registers
# (like the 80-bit registers in old x86 chips) and forces them back to 64-bit memory. This ensures consistency.
# -fno-builtin: Prevents GCC from replacing your math functions with its own "optimized" versions.
# -fno-unsafe-math-optimizations: Stops the compiler from reordering math operations in a way that might lose precision.
DDMATHFLAGS= -fno-builtin -ffloat-store -fno-unsafe-math-optimizations

# == END OF USER SETTINGS. NO NEED TO CHANGE ANYTHING BELOW THIS LINE =========

PLATS= ansi Darwin generic linux macos macosx mingw mingwsse posix stretch raspi solaris

LUA_A= libagena.a

CORE_O= lapi.o lcode.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o \
  lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o \
  lundump.o lvm.o lzio.o lset.o lseq.o lpair.o lreg.o

LIB_O= agnxlib.o lbaselib.o ldblib.o liolib.o lmathlib.o loslib.o ltablib.o \
  agnt64.o lrtable.o lenviron.o lstrlib.o utils.o units.o loadlib.o linit.o \
  cephes.o binio.o xml.o sofa.o sdncal.o iniparse.o lreglib.o registry.o \
  stack.o intvec.o dblvec.o charbuf.o memfile.o lseqlib.o lsetlib.o agnhlps.o \
  pbl.o pblhash.o pblkf.o keyfile.o bfield.o jwent.o fzy.o tuples.o long.o ints.o \
  interp.o calc.o linalg.o stats.o combinat.o numtheory.o dual.o xbase.o utf8.o \
  llist.o bags.o lookup.o numarray.o factory.o bytes.o sema.o semalib.o vecint.o \
  jbmath.o trie.o regex.o regex_f.o regexcom.o hashes.o bloom.o cuckoo.o fifo.o \
  lifo.o rbtree.o clock.o bimaps.o heaps.o dblhash.o numcuckoo.o numfilter.o \
  dd.o ddmath.o network.o uintmap.o intmap.o dblmap.o suffix.o divsufsort.o \
  sssort.o trsort.o

LUA_T= agena
LUA_O= agena.o

ALL_O= $(CORE_O) $(LIB_O) $(LUA_O)
ALL_T= $(LUA_A) $(LUA_T)
ALL_A= $(LUA_A)

default: $(PLAT)

all: $(ALL_T)

o: $(ALL_O)

a: $(ALL_A)

# Specific rule for the Double-Double math module
# This overrides the generic compilation to apply safety flags
dd.o ddmath.o: %.o: %.c ddmath.h
	$(CC) $(CFLAGS) $(DDMATHFLAGS) $(MYCFLAGS) -c $< -o $@

# Generic rule for standard C files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# for UNIX
$(LUA_A): $(CORE_O) $(LIB_O)
	$(AR) $@ $?
	$(RANLIB) $@

$(LUA_T): $(LUA_O) $(LUA_A)
	$(CC) -o $@ $(MYLDFLAGS) $(LUA_O) $(LUA_A) $(LIBS)

# for Windows
$(WINLUA_A): $(CORE_O) $(LIB_O)
	$(AR) $@ $^ $(MYLIBS) $(WINSYSLIBS)

$(WINLUA_T): $(LUA_O) $(WINLUA_A) agena.res
	$(CC) -o $@ $(MYLDFLAGS) $(LUA_O) $(WINLUA_A) agena.res $(LIBS) $(WINSYSLIBS)

clean:
	$(RM) $(ALL_T) $(ALL_O) agena.dll agena.exe agncfg.h config config.exe libagena.so agena.res

config: config.c
	$(CC) $(CFLAGS) -o config config.c
	./config agncfg.h

depend:
	@$(CC) $(CFLAGS) -MM l*.c

echo:
	@echo "PLAT = $(PLAT)"
	@echo "CC = $(CC)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "AR = $(AR)"
	@echo "RANLIB = $(RANLIB)"
	@echo "RM = $(RM)"
	@echo "MYCFLAGS = $(MYCFLAGS)"
	@echo "MYLDFLAGS = $(MYLDFLAGS)"
	@echo "MYLIBS = $(MYLIBS)"

# this will mostly report false positives ...
analyse:
	scan-build --use-cc=$(CC) $(MAKE) all

# -- Platform Targets --

none:
	@echo "Please choose a platform:"
	@echo "   $(PLATS)"

ansi:
	$(MAKE) all MYCFLAGS=-DLUA_ANSI

generic:
	$(MAKE) all MYCFLAGS=

opensuse:
	$(MAKE) all \
		MYCFLAGS="-O2 -g -Wno-attributes -fgnu89-inline -DLUA_USE_LINUX -DOPENSUSE -D_FILE_OFFSET_BITS=64" \
		MYLIBS="-Wl,-E $(LINSYSLIBS)"
	strip $(LUA_T)
	$(CC) $(MYCFLAGS) -o libagena.so -shared *.o
	strip libagena.so

linux:
	$(MAKE) all \
		MYCFLAGS="-O2 -g -Wno-attributes -fgnu89-inline -DLUA_USE_LINUX -D_FILE_OFFSET_BITS=64" \
		MYLIBS="-Wl,-E $(LINSYSLIBS)"
	strip $(LUA_T)
	$(CC) $(MYCFLAGS) -o libagena.so -shared *.o
	strip libagena.so

stretch:
	$(MAKE) all \
		MYCFLAGS="-O2 -g -DLUA_USE_LINUX -DLUA_RASPI_STRETCH -D_FILE_OFFSET_BITS=64 -DCPUID -Wall \
			-fgnu89-inline -fPIC" \
		MYLIBS="-Wl,-E $(LINSYSLIBS)"
	strip $(LUA_T)
	$(CC) $(MYCFLAGS) -o libagena.so -shared *.o
	strip libagena.so

raspi:
	$(MAKE) all \
		MYCFLAGS="-O2 -g -DLUA_USE_LINUX -DLUA_RASPI_STRETCH -DRASPIPI -D_FILE_OFFSET_BITS=64 \
			-Wall -fgnu89-inline -fPIC -fno-fast-math" \
		MYLIBS="-Wl,-E $(LINSYSLIBS)"
	strip $(LUA_T)
	$(CC) $(MYCFLAGS) -o libagena.so -shared *.o
	strip libagena.so

# strip -x does not yield smaller binaries, and switching from static to dynamic causes ival,
# gdi not to work any longer. Do NOT use -D_DARWIN_USE_64_BIT_INODE as it will break results.
Darwin macos macosx:
	$(MAKE) all \
	MYCFLAGS="-O2 -g -DLUA_USE_LINUX -DNOINLINE -D_DIRENT_HAVE_D_TYPE \
		-force_cpusubtype_ALL -mmacosx-version-min=10.4" \
	MYLIBS="-lreadline -lcurses -lexpat -lpcre2-posix -lpcre2-8 -lpthread \
		-framework ApplicationServices \
		-framework IOKit \
		-framework CoreFoundation" \
	MYLDFLAGS="-Wl,-force_cpusubtype_ALL -mmacosx-version-min=10.4"

mingw:
	windres agena.rc -O coff -o agena.res
	windres -I .. run.rc -O coff -o run.res
	$(MAKE) "WINLUA_A=agena.dll" "WINLUA_T=agena.exe" \
	"AR=$(CC) -shared -static-libgcc -o" \
	"RANLIB=strip --strip-unneeded" \
	"MYCFLAGS=-O3 -Wall -Wno-attributes -Wno-deprecated-declarations -fgnu89-inline \
		-DWIN32_LEAN_AND_MEAN -DLUA_BUILD_AS_DLL -D_FILE_OFFSET_BITS=64 -DPCRE2_CODE_UNIT_WIDTH=8" \
	"MYLIBS=$(MYLIBS) agena.res" \
	"LIBS=$(MYLIBS) $(WINSYSLIBS)" \
	"MYLDFLAGS=-s" agena.exe
	$(CC) -mwindows -static -static-libgcc -o run.exe $(MYLDFLAGS) run.c run.res

# Run this for analysis with Dr. Memory only. Change back to make mingw before deploying.
drmem:
	windres agena.rc -O coff -o agena.res
	windres -I .. run.rc -O coff -o run.res
	$(MAKE) "WINLUA_A=agena.dll" "WINLUA_T=agena.exe" \
	"AR=$(CC) -g -shared -static-libgcc -o" \
	"RANLIB=echo" \
	"MYCFLAGS=-O0 -g3 -Wall -Wno-attributes -Wno-deprecated-declarations -fgnu89-inline \
		-DWIN32_LEAN_AND_MEAN -DLUA_BUILD_AS_DLL -D_FILE_OFFSET_BITS=64 -DPCRE2_CODE_UNIT_WIDTH=8" \
	"MYLIBS=$(MYLIBS) agena.res" \
	"LIBS=$(MYLIBS) $(WINSYSLIBS)" \
	"MYLDFLAGS=-g3 -static-libgcc" agena.exe

mingwsse:
	windres agena.rc -O coff -o agena.res
	$(MAKE) "WINLUA_A=agena.dll" "WINLUA_T=agena.exe" \
	"AR=$(CC) -O3 -shared -fgnu89-inline -o" \
	"RANLIB=strip --strip-unneeded" \
	"MYCFLAGS=-O3 -msse2 -mfpmath=sse -march=nehalem -Wall -Wno-attributes \
		-Wno-deprecated-declarations -fgnu89-inline -DWIN32_LEAN_AND_MEAN \
		-DLUA_BUILD_AS_DLL -D_FILE_OFFSET_BITS=64 -DPCRE2_CODE_UNIT_WIDTH=8 -DSSE2" \
	"MYLIBS=$(MYLIBS) agena.res" \
	"LIBS=$(MYLIBS) $(WINSYSLIBS)" \
	"MYLDFLAGS=-s" agena.exe

posix:
	$(MAKE) all MYCFLAGS=-DLUA_USE_POSIX

solaris:
	$(MAKE) all \
		MYCFLAGS="-O3 -Wno-attributes -fgnu89-inline -DLUA_USE_POSIX -DLUA_USE_DLOPEN \
			-D__EXTENSIONS__ -DLUA_USE_READLINE" \
		MYLIBS="-ldl $(SOLSYSLIBS)"
	strip $(LUA_T)
	gcc -o libagena.so -G -h libagena.so -O3 -fpic -lm -lc $(SOLSYSLIBS) *.o
	strip libagena.so

# for OpenSUSE
install:
# (deliberately commented)

# list targets that do not create files (but not all makes understand .PHONY)
#.PHONY: all $(PLATS) default o a clean depend echo none

# DO NOT DELETE
lapi.o: lapi.c agena.h agnconf.h lapi.h lobject.h llimits.h ldebug.h \
	lstate.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h \
	lundump.h lvm.h
agnxlib.o: agnxlib.c agena.h agnconf.h agnxlib.h llimits.h
lbaselib.o: lbaselib.c lbaselib.h agena.h agnconf.h agnxlib.h agenalib.h agnhlps.h
lcode.o: lcode.c agena.h agnconf.h lcode.h llex.h lobject.h llimits.h \
	lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h ldo.h lgc.h \
	ltable.h
ldblib.o: ldblib.c agena.h agnconf.h agnxlib.h agenalib.h agnhlps.h
ldebug.o: ldebug.c agena.h agnconf.h lapi.h lobject.h llimits.h lcode.h \
	llex.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h ldo.h \
	lfunc.h lstring.h lgc.h ltable.h lvm.h
ldo.o: ldo.c agena.h agnconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
	lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lparser.h lstring.h \
	ltable.h lundump.h lvm.h
ldump.o: ldump.c agena.h agnconf.h lobject.h llimits.h lstate.h ltm.h \
  lzio.h lmem.h lundump.h
lfunc.o: lfunc.c agena.h agnconf.h lfunc.h lobject.h llimits.h lgc.h lmem.h \
	lstate.h ltm.h lzio.h
lgc.o: lgc.c agena.h agnconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
	lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h
linit.o: linit.c agena.h agnconf.h agenalib.h agnxlib.h
liolib.o: liolib.c agena.h agnconf.h agnxlib.h agenalib.h
llex.o: llex.c agena.h agnconf.h ldo.h lobject.h llimits.h lstate.h ltm.h \
	lzio.h lmem.h llex.h lparser.h lstring.h lgc.h ltable.h
lmathlib.o: lmathlib.c agena.h agnxlib.h agenalib.h agnconf.h agncmpt.h sunpro.h
lmem.o: lmem.c agena.h agnconf.h ldebug.h lstate.h lobject.h llimits.h \
	ltm.h lzio.h lmem.h ldo.h
loadlib.o: loadlib.c agnxlib.h agena.h agnconf.h lobject.h llimits.h \
	agenalib.h
lobject.o: lobject.c agena.h agnconf.h ldo.h lobject.h llimits.h lstate.h \
	ltm.h lzio.h lmem.h lstring.h lgc.h lvm.h
lopcodes.o: lopcodes.c lopcodes.h llimits.h agena.h agnconf.h
network.o:	network.c agena.h agnxlib.h network.h
loslib.o: loslib.c agena.h agnconf.h agnxlib.h agenalib.h agnhlps.h
lparser.o: lparser.c agena.h agnconf.h lcode.h llex.h lobject.h llimits.h \
	lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h ldo.h \
	lfunc.h lstring.h lgc.h ltable.h agnhlps.h
lrtable.o:	lrtable.c agena.h agnxlib.h agenalib.h
lenviron.o:	lenviron.c agena.h agnxlib.h agenalib.h
lstate.o: lstate.c agena.h agnconf.h ldebug.h lstate.h lobject.h llimits.h \
	ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h llex.h lstring.h ltable.h
lstring.o: lstring.c agena.h agnconf.h lmem.h llimits.h lobject.h lstate.h \
	ltm.h lzio.h lstring.h lgc.h
lstrlib.o: lstrlib.c agena.h agnconf.h agnxlib.h agenalib.h lstrlib.h \
	agnhlps.h jwent.h divsufsort.h
ltable.o: ltable.c agena.h agnconf.h ldebug.h lstate.h lobject.h llimits.h \
	ltm.h lzio.h lmem.h ldo.h lgc.h ltable.h
ltablib.o: ltablib.c agena.h agnconf.h agnxlib.h agenalib.h
lreglib.o: lreglib.c agena.h agnxlib.h agenalib.h
lseqlib.o: lseqlib.c agena.h agnxlib.h agenalib.h
lsetlib.o: lsetlib.c agena.h agnxlib.h agenalib.h
lreg.o: lreg.c agena.h ldebug.h lstate.h lobject.h llimits.h \
	ltm.h lzio.h lmem.h ldo.h lgc.h lreg.h
ltm.o: ltm.c agena.h agnconf.h lobject.h llimits.h lstate.h ltm.h lzio.h \
	lmem.h lstring.h lgc.h ltable.h
agena.o: agena.c agena.h agnconf.h agnxlib.h agenalib.h
lundump.o: lundump.c agena.h agnconf.h ldebug.h lstate.h lobject.h \
  llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lstring.h lgc.h lundump.h
lvm.o: lvm.c lvm.h agena.h agnconf.h agnhlps.h ldebug.h lstate.h lobject.h \
	llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lcode.h llex.h lopcodes.h \
	lstring.h ltable.h lset.h lseq.h lreg.h lpair.h lcomplex.h lstrlib.h sunpro.h \
	charbuf.h intvec.h
lzio.o: lzio.c agena.h agnconf.h llimits.h lmem.h lstate.h lobject.h ltm.h \
	lzio.h
cephes.o: cephes.c cephes.h
jbmath.o:	jbmath.c jbmath.h
dd.o:	dd.c agena.h agnxlib.h agenalib.h ddmath.h
agnhlps.o: agnhlps.c agnt64.h agnhlps.h agnconf.h llimits.h agena.h agncmpt.h \
	cephes.h
utils.o: utils.c agena.h agnxlib.h agenalib.h
units.o: units.c agena.h agnxlib.h agenalib.h
agnt64.o: agnt64.c agnt64_l.h agnt64_c.h
sofa.o: sofa.c sofa.h
sdncal.o: sdncal.c sdncal.h
intvec.o:	intvec.c intvec.h agnhlps.h
dblvec.o:	dblvec.c dblvec.h agnhlps.h
charbuf.o:	charbuf.c charbuf.h
iniparse.o:	iniparse.c iniparse.h
lset.o: lset.c agena.h ldebug.h lstate.h lobject.h \
	lmem.h ldo.h lgc.h lset.h
lseq.o: lseq.c agena.h ldebug.h lstate.h lobject.h lmem.h ldo.h lgc.h \
	lseq.h
lpair.o: lpair.c agena.h ldebug.h lstate.h lobject.h lmem.h ldo.h lgc.h \
	lpair.h
binio.o: binio.c agena.h agnxlib.h agenalib.h agnhlps.h
registry.o: registry.c agena.h agnxlib.h agenalib.h agncmpt.h
stack.o: stack.c agena.h agnxlib.h agenalib.h
memfile.o: memfile.c agena.h agnxlib.h agenalib.h
bfield.o: bfield.c agena.h agnxlib.h agenalib.h
jwent.o: jwent.c jwent.h
divsufsort.o:	divsufsort.c divsufsort.h divsufsort_private.h
sssort.o: sssort.c divsufsort.h divsufsort_private.h
trsort.o: trsort.c divsufsort.h divsufsort_private.h
suffix.o: suffix.c agena.h agnxlib.h agenalib.h
fzy.o:	fzy.c fzy.h agena.h agnxlib.h agenalib.h lucase.def
tuples.o: tuples.c agena.h agnxlib.h agenalib.h
long.o:	long.c agena.h agnxlib.h agenalib.h agnhlps.h
ints.o:	ints.c agena.h agnxlib.h agenalib.h agnhlps.h
interp.o: interp.c interp.h
calc.o: calc.c agena.h agnxlib.h agenalib.h agnhlps.h cephes.h \
	interp.h jbmath.h ddmath.h
linalg.o: linalg.c agena.h agnxlib.h agenalib.h
stats.o: stats.c agena.h agnxlib.h agenalib.h cephes.h jbmath.h
combinat.o: combinat.c agena.h agnxlib.h agenalib.h
numtheory.o: numtheory.c agena.h agnxlib.h agenalib.h
dual.o: dual.c agena.h agnxlib.h agenalib.h
xbase.o: xbase.c agena.h agnxlib.h agenalib.h
utf8.o: utf8.c lprefix.h agena.h agnconf.h agnxlib.h agenalib.h
llist.o:	llist.c agena.h agnxlib.h agenalib.h
bags.o: bags.c agena.h agnxlib.h agenalib.h
lookup.o: lookup.c lbaselib.h agena.h agnxlib.h agenalib.h
numarray.o:	numarray.c agena.h agnxlib.h agenalib.h agnhlps.h
factory.o:	factory.c agena.h agnxlib.h agenalib.h agnhlps.h
bytes.o:	bytes.c agena.h agnxlib.h agenalib.h agnhlps.h
semalib.o:	semalib.c semalib.h
sema.o:	sema.c semalib.h semalib.c agena.h agnxlib.h agenalib.h agnhlps.h
vecint.o:	vecint.c vecint.h intvec.h agena.h agnxlib.h agenalib.h agnhlps.h
trie.o:	trie.c agena.h agnxlib.h agenalib.h
xml.o:	xml.c agena.h agnxlib.h agenalib.h xml.h
regex_f.o:	regex_f.c regex.h
regexcom.o:	regexcom.c regexcom.h
regex.o:	regex.c regex.h agena.h agnxlib.h agenalib.h regex_f.c \
	regexcom.c regexcom.h regexalg.h
hashes.o: hashes.c agena.h agnxlib.h agenalib.h
bloom.o: bloom.c agena.h agnxlib.c agenalib.h
cuckoo.o:	cuckoo.c agena.h agnxlib.h agenalib.h
fifo.o:	fifo.c agena.h agnxlib.h agenalib.h
lifo.o:	lifo.c agena.h agnxlib.h agenalib.h
rbtree.o:	rbtree.c rbtree.h agena.h agnxlib.h agenalib.h
clock.o:	clock.c agena.h agnxlib.h agenalib.h
bimaps.o:	bimaps.c agena.h agnxlib.h agenalib.h
heaps.o:	heaps.c agena.h agnxlib.h agenalib.h
dblhash.o:	dblhash.c agena.h agnxlib.h agenalib.h dblhash.h
uintmap.o:	uintmap.c agena.h agnxlib.h agenalib.h agnhlps.h khash.h
intmap.o:	intmap.c agena.h agnxlib.h agenalib.h agnhlps.h khash.h
dblmap.o:	dblmap.c agena.h agnxlib.h agenalib.h agnhlps.h khash.h
numcuckoo.o:	numcuckoo.c agena.h agnxlib.h agenalib.h
numfilter.o:	numfilter.c agena.h agnxlib.h agenalib.h
pbl.o:	pbl.c pbl.h
pblhash.o:	pblhash.c pbl.h
pblkf.o:	pblkf.c pbl.h
keyfile.o:	keyfile.c agena.h agnxlib.h agenalib.h pbl.h

# (end of Makefile)

