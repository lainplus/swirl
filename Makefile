# Swirl Makefile

ifndef TOP
 TOP = .
 INCLUDED = no
endif

ifeq ($(findstring $(MAKECMDGOALS),clean distclean),)
 include $(TOP)/config.mak
endif

CONFIG_strip = no

ifeq (-$(GCC_MAJOR)-$(findstring $(GCC_MINOR),56789)-,-4--)
 CFLAGS += -D_FORTIFY_SOURCE=0
endif

LIBSWIRL = libswirl.a
LIBSWIRL1 = libswirl1.a
LINK_LIBSWIRL =
LIBS =
CFLAGS += -I$(TOP)
CFLAGS += $(CPPFLAGS)
VPATH = $(TOPSRC)

ifdef CONFIG_WIN32
 CFG = -win
 ifneq ($(CONFIG_static),yes)
  LIBSWIRL = libswirl$(DLLSUF)
  LIBSWIRLDEF = libswirl.def
 endif
 NATIVE_TARGET = $(ARCH)-win$(if $(findstring arm,$(ARCH)),ce,32)
else
 CFG = -unx
 LIBS=-lm -lpthread
 ifneq ($(CONFIG_ldl),no)
  LIBS+=-ldl
 endif
 # make libswirl as static or dynamic library?
 ifeq ($(CONFIG_static),no)
  LIBSWIRL=libswirl$(DLLSUF)
  export LD_LIBRARY_PATH := $(CURDIR)/$(TOP)
  ifneq ($(CONFIG_rpath),no)
    ifndef CONFIG_OSX
      LINK_LIBSWIRL += -Wl,-rpath,"$(libdir)"
    else
      # macOS doesn't support env-vars libdir out of the box - which we need for
      # `make test' when libswirl.dylib is used (configure --disable-static), so
      # we bake a relative path into the binary. $libdir is used after install.
      LINK_LIBSWIRL += -Wl,-rpath,"@executable_path/$(TOP)" -Wl,-rpath,"$(libdir)"
      DYLIBVER += -current_version $(VERSION)
      DYLIBVER += -compatibility_version $(VERSION)
    endif
  endif
 endif
 NATIVE_TARGET = $(ARCH)
 ifdef CONFIG_OSX
  NATIVE_TARGET = $(ARCH)-osx
  ifneq ($(CC_NAME),swirl)
    LDFLAGS += -flat_namespace -undefined warning
  endif
  export MACOSX_DEPLOYMENT_TARGET := 10.6
 endif
endif

# run local version of swirl with local libraries and includes
SWIRLFLAGS-unx = -B$(TOP) -I$(TOPSRC)/include -I$(TOPSRC) -I$(TOP)
SWIRLFLAGS-win = -B$(TOPSRC)/win32 -I$(TOPSRC)/include -I$(TOPSRC) -I$(TOP) -L$(TOP)
SWIRLFLAGS = $(SWIRLFLAGS$(CFG))
SWIRL = $(TOP)/swirl$(EXESUF) $(SWIRLFLAGS)

CFLAGS_P = $(CFLAGS) -pg -static -DCONFIG_SWIRL_STATIC -DSWIRL_PROFILE
LIBS_P = $(LIBS)
LDFLAGS_P = $(LDFLAGS)

CONFIG_$(ARCH) = yes
NATIVE_DEFINES_$(CONFIG_i386) += -DSWIRL_TARGET_I386
NATIVE_DEFINES_$(CONFIG_x86_64) += -DSWIRL_TARGET_X86_64
NATIVE_DEFINES_$(CONFIG_WIN32) += -DSWIRL_TARGET_PE
NATIVE_DEFINES_$(CONFIG_OSX) += -DSWIRL_TARGET_MACHO
NATIVE_DEFINES_$(CONFIG_uClibc) += -DSWIRL_UCLIBC
NATIVE_DEFINES_$(CONFIG_musl) += -DSWIRL_MUSL
NATIVE_DEFINES_$(CONFIG_libgcc) += -DCONFIG_USE_LIBGCC
NATIVE_DEFINES_$(CONFIG_selinux) += -DHAVE_SELINUX
NATIVE_DEFINES_$(CONFIG_arm) += -DSWIRL_TARGET_ARM
NATIVE_DEFINES_$(CONFIG_arm_eabihf) += -DSWIRL_ARM_EABI -DSWIRL_ARM_HARDFLOAT
NATIVE_DEFINES_$(CONFIG_arm_eabi) += -DSWIRL_ARM_EABI
NATIVE_DEFINES_$(CONFIG_arm_vfp) += -DSWIRL_ARM_VFP
NATIVE_DEFINES_$(CONFIG_arm64) += -DSWIRL_TARGET_ARM64
NATIVE_DEFINES_$(CONFIG_riscv64) += -DSWIRL_TARGET_RISCV64
NATIVE_DEFINES_$(CONFIG_BSD) += -DTARGETOS_$(TARGETOS)
NATIVE_DEFINES_no_$(CONFIG_bcheck) += -DCONFIG_SWIRL_BCHECK=0
NATIVE_DEFINES_no_$(CONFIG_backtrace) += -DCONFIG_SWIRL_BACKTRACE=0
NATIVE_DEFINES += $(NATIVE_DEFINES_yes) $(NATIVE_DEFINES_no_no)

DEF-i386           = -DSWIRL_TARGET_I386
DEF-i386-win32     = -DSWIRL_TARGET_I386 -DSWIRL_TARGET_PE
DEF-i386-OpenBSD   = $(DEF-i386) -DTARGETOS_OpenBSD
DEF-x86_64         = -DSWIRL_TARGET_X86_64
DEF-x86_64-win32   = -DSWIRL_TARGET_X86_64 -DSWIRL_TARGET_PE
DEF-x86_64-osx     = -DSWIRL_TARGET_X86_64 -DSWIRL_TARGET_MACHO
DEF-arm-fpa        = -DSWIRL_TARGET_ARM
DEF-arm-fpa-ld     = -DSWIRL_TARGET_ARM -DLDOUBLE_SIZE=12
DEF-arm-vfp        = -DSWIRL_TARGET_ARM -DSWIRL_ARM_VFP
DEF-arm-eabi       = -DSWIRL_TARGET_ARM -DSWIRL_ARM_VFP -DSWIRL_ARM_EABI
DEF-arm-eabihf     = $(DEF-arm-eabi) -DSWIRL_ARM_HARDFLOAT
DEF-arm            = $(DEF-arm-eabihf)
DEF-arm-NetBSD     = $(DEF-arm-eabihf) -DTARGETOS_FreeBSD
DEF-arm-wince      = $(DEF-arm-eabihf) -DSWIRL_TARGET_PE
DEF-arm64          = -DSWIRL_TARGET_ARM64
DEF-arm64-FreeBSD  = $(DEF-arm64) -DTARGETOS_FreeBSD
DEF-arm64-NetBSD   = $(DEF-arm64) -DTARGETOS_NetBSD
DEF-arm64-OpenBSD  = $(DEF-arm64) -DTARGETOS_OpenBSD
DEF-riscv64        = -DSWIRL_TARGET_RISCV64
DEF-c67            = -DSWIRL_TARGET_C67 -w # disable warnigs
DEF-x86_64-FreeBSD = $(DEF-x86_64) -DTARGETOS_FreeBSD
DEF-x86_64-NetBSD  = $(DEF-x86_64) -DTARGETOS_NetBSD
DEF-x86_64-OpenBSD = $(DEF-x86_64) -DTARGETOS_OpenBSD

DEF-$(NATIVE_TARGET) = $(NATIVE_DEFINES)

ifeq ($(INCLUDED),no)
# --------------------------------------------------------------------------
# running top Makefile

PROGS = swirl$(EXESUF)
SWIRLLIBS = $(LIBSWIRLDEF) $(LIBSWIRL) $(LIBSWIRL1)

all: $(PROGS) $(SWIRLLIBS)

# cross compiler targets to build
SWIRL_X = i386 x86_64 i386-win32 x86_64-win32 x86_64-osx arm arm64 arm-wince c67
SWIRL_X += riscv64
# SWIRL_X += arm-fpa arm-fpa-ld arm-vfp arm-eabi

# cross libswirl1.a targets to build
LIBSWIRL1_X = i386 x86_64 i386-win32 x86_64-win32 x86_64-osx arm arm64 arm-wince
LIBSWIRL1_X += riscv64

PROGS_CROSS = $(foreach X,$(SWIRL_X),$X-swirl$(EXESUF))
LIBSWIRL1_CROSS = $(foreach X,$(LIBSWIRL1_X),$X-libswirl1.a)

# build cross compilers & libs
cross: $(LIBSWIRL1_CROSS) $(PROGS_CROSS)

# build specific cross compiler & lib
cross-%: %-swirl$(EXESUF) %-libswirl1.a ;

install: ; @$(MAKE) --no-print-directory  install$(CFG)
install-strip: ; @$(MAKE) --no-print-directory  install$(CFG) CONFIG_strip=yes
uninstall: ; @$(MAKE) --no-print-directory uninstall$(CFG)

ifdef CONFIG_cross
all : cross
endif

# --------------------------------------------

T = $(or $(CROSS_TARGET),$(NATIVE_TARGET),unknown)
X = $(if $(CROSS_TARGET),$(CROSS_TARGET)-)

DEFINES += $(DEF-$T) $(DEF-all)
DEFINES += $(if $(ROOT-$T),-DCONFIG_SYSROOT="\"$(ROOT-$T)\"")
DEFINES += $(if $(CRT-$T),-DCONFIG_SWIRL_CRTPREFIX="\"$(CRT-$T)\"")
DEFINES += $(if $(LIB-$T),-DCONFIG_SWIRL_LIBPATHS="\"$(LIB-$T)\"")
DEFINES += $(if $(INC-$T),-DCONFIG_SWIRL_SYSINCLUDEPATHS="\"$(INC-$T)\"")
DEFINES += $(DEF-$(or $(findstring win,$T),unx))

ifneq ($(X),)
ifeq ($(CONFIG_WIN32),yes)
DEF-win += -DSWIRL_LIBSWIRL1="\"$(X)libswirl1.a\""
DEF-unx += -DSWIRL_LIBSWIRL1="\"lib/$(X)libswirl1.a\""
else
DEF-all += -DSWIRL_LIBSWIRL1="\"$(X)libswirl1.a\""
DEF-win += -DCONFIG_SWIRLDIR="\"$(swirldir)/win32\""
endif
endif

# include custom configuration (see make help)
-include config-extra.mak

CORE_FILES = swirl.c swirltools.c libswirl.c swirlpp.c swirlgen.c swirlelf.c swirlasm.c swirlrun.c
CORE_FILES += swirl.h config.h libswirl.h swirltok.h
i386_FILES = $(CORE_FILES) i386-gen.c i386-link.c i386-asm.c i386-asm.h i386-tok.h
i386-win32_FILES = $(i386_FILES) swirlpe.c
x86_64_FILES = $(CORE_FILES) x86_64-gen.c x86_64-link.c i386-asm.c x86_64-asm.h
x86_64-win32_FILES = $(x86_64_FILES) swirlpe.c
x86_64-osx_FILES = $(x86_64_FILES) swirlmacho.c
arm_FILES = $(CORE_FILES) arm-gen.c arm-link.c arm-asm.c arm-tok.h
arm-wince_FILES = $(arm_FILES) swirlpe.c
arm-eabihf_FILES = $(arm_FILES)
arm-fpa_FILES     = $(arm_FILES)
arm-fpa-ld_FILES  = $(arm_FILES)
arm-vfp_FILES     = $(arm_FILES)
arm-eabi_FILES    = $(arm_FILES)
arm-eabihf_FILES  = $(arm_FILES)
arm64_FILES = $(CORE_FILES) arm64-gen.c arm64-link.c arm64-asm.c
c67_FILES = $(CORE_FILES) c67-gen.c c67-link.c swirlcoff.c
riscv64_FILES = $(CORE_FILES) riscv64-gen.c riscv64-link.c riscv64-asm.c

SWIRLDEFS_H$(subst yes,,$(CONFIG_predefs)) = swirldefs_.h

# libswirl sources
LIBSWIRL_SRC = $(filter-out swirl.c swirltools.c,$(filter %.c,$($T_FILES)))

ifeq ($(ONE_SOURCE),yes)
LIBSWIRL_OBJ = $(X)libswirl.o
LIBSWIRL_INC = $($T_FILES)
SWIRL_FILES = $(X)swirl.o
swirl.o : DEFINES += -DONE_SOURCE=0
$(X)swirl.o $(X)libswirl.o  : $(SWIRLDEFS_H)
else
LIBSWIRL_OBJ = $(patsubst %.c,$(X)%.o,$(LIBSWIRL_SRC))
LIBSWIRL_INC = $(filter %.h %-gen.c %-link.c,$($T_FILES))
SWIRL_FILES = $(X)swirl.o $(LIBSWIRL_OBJ)
$(SWIRL_FILES) : DEFINES += -DONE_SOURCE=0
$(X)swirlpp.o : $(SWIRLDEFS_H)
endif

ifeq ($(CONFIG_strip),no)
CFLAGS += -g
LDFLAGS += -g
else
ifndef CONFIG_OSX
LDFLAGS += -s
endif
endif

# convert "include/swirldefs.h" to "swirldefs_.h"
%_.h : include/%.h conftest.c
	$S$(CC) -DC2STR $(filter %.c,$^) -o c2str.exe && ./c2str.exe $< $@

# target specific object rule
$(X)%.o : %.c $(LIBSWIRL_INC)
	$S$(CC) -o $@ -c $< $(DEFINES) $(CFLAGS)

# additional dependencies
$(X)swirl.o : swirltools.c

# Host Tiny C Compiler
swirl$(EXESUF): swirl.o $(LIBSWIRL)
	$S$(CC) -o $@ $^ $(LIBS) $(LDFLAGS) $(LINK_LIBSWIRL)

# Cross Tiny C Compilers
# (the SWIRLDEFS_H dependency is only necessary for parallel makes,
# ala 'make -j x86_64-swirl i386-swirl swirl', which would create multiple
# c2str.exe and swirldefs_.h files in parallel, leading to access errors.
# This forces it to be made only once.  Make normally tracks multiple paths
# to the same goals and only remakes it once, but that doesn't work over
# sub-makes like in this target)
%-swirl$(EXESUF): $(SWIRLDEFS_H) FORCE
	@$(MAKE) --no-print-directory $@ CROSS_TARGET=$* ONE_SOURCE=$(or $(ONE_SOURCE),yes)

$(CROSS_TARGET)-swirl$(EXESUF): $(SWIRL_FILES)
	$S$(CC) -o $@ $^ $(LIBS) $(LDFLAGS)

# profiling version
swirl_p$(EXESUF): $($T_FILES)
	$S$(CC) -o $@ $< $(DEFINES) $(CFLAGS_P) $(LIBS_P) $(LDFLAGS_P)

# static libswirl library
libswirl.a: $(LIBSWIRL_OBJ)
	$S$(AR) rcs $@ $^

# dynamic libswirl library
libswirl.so: $(LIBSWIRL_OBJ)
	$S$(CC) -shared -Wl,-soname,$@ -o $@ $^ $(LDFLAGS)

libswirl.so: CFLAGS+=-fPIC
libswirl.so: LDFLAGS+=-fPIC

# OSX dynamic libswirl library
libswirl.dylib: $(LIBSWIRL_OBJ)
	$S$(CC) -dynamiclib $(DYLIBVER) -install_name @rpath/$@ -o $@ $^ $(LDFLAGS) 

# OSX libswirl.dylib (without rpath/ prefix)
libswirl.osx: $(LIBSWIRL_OBJ)
	$S$(CC) -shared -install_name libswirl.dylib -o libswirl.dylib $^ $(LDFLAGS) 

# windows dynamic libswirl library
libswirl.dll : $(LIBSWIRL_OBJ)
	$S$(CC) -shared -o $@ $^ $(LDFLAGS)
libswirl.dll : DEFINES += -DLIBSWIRL_AS_DLL

# import file for windows libswirl.dll
libswirl.def : libswirl.dll swirl$(EXESUF)
	$S$(XSWIRL) -impdef $< -o $@
XSWIRL ?= ./swirl$(EXESUF)

# TinyCC runtime libraries
libswirl1.a : swirl$(EXESUF) FORCE
	@$(MAKE) -C lib

# Cross libswirl1.a
%-libswirl1.a : %-swirl$(EXESUF) FORCE
	@$(MAKE) -C lib CROSS_TARGET=$*

.PRECIOUS: %-libswirl1.a
FORCE:

run-if = $(if $(shell which $1),$S $1 $2)
S = $(if $(findstring yes,$(SILENT)),@$(info * $@))

# install

INSTALL = install -m644
INSTALLBIN = install -m755 $(STRIP_$(CONFIG_strip))
STRIP_yes = -s

LIBSWIRL1_W = $(filter %-win32-libswirl1.a %-wince-libswirl1.a,$(LIBSWIRL1_CROSS))
LIBSWIRL1_U = $(filter-out $(LIBSWIRL1_W),$(LIBSWIRL1_CROSS))
IB = $(if $1,$(IM) mkdir -p $2 && $(INSTALLBIN) $1 $2)
IBw = $(call IB,$(wildcard $1),$2)
IF = $(if $1,$(IM) mkdir -p $2 && $(INSTALL) $1 $2)
IFw = $(call IF,$(wildcard $1),$2)
IR = $(IM) mkdir -p $2 && cp -r $1/. $2
IM = $(info -> $2 : $1)@

B_O = bcheck.o bt-exe.o bt-log.o bt-dll.o

# install progs & libs
install-unx:
	$(call IBw,$(PROGS) $(PROGS_CROSS),"$(bindir)")
	$(call IFw,$(LIBSWIRL1) $(B_O) $(LIBSWIRL1_U),"$(swirldir)")
	$(call IF,$(TOPSRC)/include/*.h $(TOPSRC)/swirllib.h,"$(swirldir)/include")
	$(call $(if $(findstring .so,$(LIBSWIRL)),IBw,IFw),$(LIBSWIRL),"$(libdir)")
	$(call IF,$(TOPSRC)/libswirl.h,"$(includedir)")
	$(call IFw,swirl.1,"$(mandir)/man1")
	$(call IFw,swirl-doc.info,"$(infodir)")
	$(call IFw,swirl-doc.html,"$(docdir)")
ifneq "$(wildcard $(LIBSWIRL1_W))" ""
	$(call IFw,$(TOPSRC)/win32/lib/*.def $(LIBSWIRL1_W),"$(swirldir)/win32/lib")
	$(call IR,$(TOPSRC)/win32/include,"$(swirldir)/win32/include")
	$(call IF,$(TOPSRC)/include/*.h $(TOPSRC)/swirllib.h,"$(swirldir)/win32/include")
endif

# uninstall
uninstall-unx:
	@rm -fv $(foreach P,$(PROGS) $(PROGS_CROSS),"$(bindir)/$P")
	@rm -fv "$(libdir)/libswirl.a" "$(libdir)/libswirl.so" "$(libdir)/libswirl.dylib" "$(includedir)/libswirl.h"
	@rm -fv "$(mandir)/man1/swirl.1" "$(infodir)/swirl-doc.info"
	@rm -fv "$(docdir)/swirl-doc.html"
	@rm -frv "$(swirldir)"

# install progs & libs on windows
install-win:
	$(call IBw,$(PROGS) $(PROGS_CROSS) $(subst libswirl.a,,$(LIBSWIRL)),"$(bindir)")
	$(call IF,$(TOPSRC)/win32/lib/*.def,"$(swirldir)/lib")
	$(call IFw,libswirl1.a $(B_O) $(LIBSWIRL1_W),"$(swirldir)/lib")
	$(call IF,$(TOPSRC)/include/*.h $(TOPSRC)/swirllib.h,"$(swirldir)/include")
	$(call IR,$(TOPSRC)/win32/include,"$(swirldir)/include")
	$(call IR,$(TOPSRC)/win32/examples,"$(swirldir)/examples")
	$(call IF,$(TOPSRC)/tests/libswirl_test.c,"$(swirldir)/examples")
	$(call IFw,$(TOPSRC)/libswirl.h $(subst .dll,.def,$(LIBSWIRL)),"$(libdir)")
	$(call IFw,$(TOPSRC)/win32/swirl-win32.txt swirl-doc.html,"$(docdir)")
ifneq "$(wildcard $(LIBSWIRL1_U))" ""
	$(call IFw,$(LIBSWIRL1_U),"$(swirldir)/lib")
	$(call IF,$(TOPSRC)/include/*.h $(TOPSRC)/swirllib.h,"$(swirldir)/lib/include")
endif

# the msys-git shell works to configure && make except it does not have install
ifeq ($(CONFIG_WIN32)-$(shell which install || echo no),yes-no)
install-win : INSTALL = cp
install-win : INSTALLBIN = cp
endif

# uninstall on windows
uninstall-win:
	@rm -fv $(foreach P,libswirl.dll $(PROGS) *-swirl.exe,"$(bindir)"/$P)
	@rm -fr $(foreach P,doc examples include lib libswirl,"$(swirldir)/$P"/*)
	@rm -frv $(foreach P,doc examples include lib libswirl,"$(swirldir)/$P")

# --------------------------------------------------------------------------
# other stuff

TAGFILES = *.[ch] include/*.h lib/*.[chS]
tags : ; ctags $(TAGFILES)
# cannot have both tags and TAGS on windows
ETAGS : ; etags $(TAGFILES)

# create release tarball from *current* git branch (including swirl-doc.html
# and converting two files to CRLF)
SWIRL-VERSION = swirl-$(VERSION)
SWIRL-VERSION = tinycc-mob-$(shell git rev-parse --short=7 HEAD)
tar:    swirl-doc.html
	mkdir -p $(SWIRL-VERSION)
	( cd $(SWIRL-VERSION) && git --git-dir ../.git checkout -f )
	cp swirl-doc.html $(SWIRL-VERSION)
	for f in swirl-win32.txt build-swirl.bat ; do \
	    cat win32/$$f | sed 's,\(.*\),\1\r,g' > $(SWIRL-VERSION)/win32/$$f ; \
	done
	tar cjf $(SWIRL-VERSION).tar.bz2 $(SWIRL-VERSION)
	rm -rf $(SWIRL-VERSION)
	git reset

config.mak:
	$(if $(wildcard $@),,@echo "Please run ./configure." && exit 1)

# run all tests
test:
	@$(MAKE) -C tests
# run test(s) from tests2 subdir (see make help)
tests2.%:
	@$(MAKE) -C tests/tests2 $@

testspp.%:
	@$(MAKE) -C tests/pp $@

clean:
	@rm -f swirl$(EXESUF) swirl_p$(EXESUF) *-swirl$(EXESUF) swirl.pod tags ETAGS
	@rm -f *.o *.a *.so* *.out *.log lib*.def *.exe *.dll a.out *.dylib *_.h
	@$(MAKE) -s -C lib $@
	@$(MAKE) -s -C tests $@

distclean: clean
	@rm -fv config.h config.mak config.texi swirl.1 swirl-doc.info swirl-doc.html

.PHONY: all clean test tar tags ETAGS distclean install uninstall FORCE

help:
	@echo "make"
	@echo "   build native compiler (from separate objects)"
	@echo ""
	@echo "make cross"
	@echo "   build cross compilers (from one source)"
	@echo ""
	@echo "make ONE_SOURCE=no/yes SILENT=no/yes"
	@echo "   force building from separate/one object(s), less/more silently"
	@echo ""
	@echo "make cross-TARGET"
	@echo "   build one specific cross compiler for 'TARGET'. Currently supported:"
	@echo "      $(wordlist 1,6,$(SWIRL_X))"
	@echo "      $(wordlist 7,99,$(SWIRL_X))"
	@echo ""
	@echo "make test"
	@echo "   run all tests"
	@echo ""
	@echo "make tests2.all / make tests2.37 / make tests2.37+"
	@echo "   run all/single test(s) from tests2, optionally update .expect"
	@echo ""
	@echo "make testspp.all / make testspp.17"
	@echo "   run all/single test(s) from tests/pp"
	@echo ""
	@echo "Other supported make targets:"
	@echo "   install install-strip doc clean tags ETAGS tar distclean help"
	@echo ""
	@echo "Custom configuration:"
	@echo "   The makefile includes a file 'config-extra.mak' if it is present."
	@echo "   This file may contain some custom configuration.  For example:"
	@echo "      NATIVE_DEFINES += -D..."
	@echo "   Or for example to configure the search paths for a cross-compiler"
	@echo "   that expects the linux files in <swirldir>/i386-linux:"
	@echo "      ROOT-i386 = {B}/i386-linux"
	@echo "      CRT-i386  = {B}/i386-linux/usr/lib"
	@echo "      LIB-i386  = {B}/i386-linux/lib:{B}/i386-linux/usr/lib"
	@echo "      INC-i386  = {B}/lib/include:{B}/i386-linux/usr/include"
	@echo "      DEF-i386  += -D__linux__"

# --------------------------------------------------------------------------
endif # ($(INCLUDED),no)
