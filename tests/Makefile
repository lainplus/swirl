#
# Swirl Compiler Makefile - tests
#

TOP = ..
include $(TOP)/Makefile
VPATH = $(TOPSRC)/tests $(TOPSRC) $(TOP)
CFLAGS := $(filter-out -W% -g% -O%,$(CFLAGS)) -I$(TOPSRC) $(LDFLAGS)

# what tests to run
TESTS = \
 hello-exe \
 hello-run \
 libtest \
 libtest_mt \
 test3 \
 memtest \
 dlltest \
 abitest \
 asm-c-connect-test \
 vla_test-run \
 cross-test \
 tests2-dir \
 pp-dir

# test4_static -- not all relocation types are implemented yet.
# asmtest / asmtest2 -- minor differences with gcc

ifneq ($(CONFIG_bcheck),no)
 TESTS += btest test1b
endif
ifeq ($(CONFIG_dll),no)
 TESTS := $(filter-out dlltest, $(TESTS))
endif
ifeq (,$(filter arm64 i386 x86_64,$(ARCH)))
 TESTS := $(filter-out vla_test-run,$(TESTS))
endif
ifeq ($(CONFIG_arm_eabi),yes)
 TESTS := $(filter-out test3,$(TESTS))
endif
ifeq (,$(filter i386 x86_64,$(ARCH)))
 TESTS := $(filter-out asm-c-connect-test,$(TESTS))
endif
ifeq ($(OS),Windows_NT) # for libswirl_test to find libswirl.dll
 PATH := $(CURDIR)/$(TOP)$(if $(findstring ;,$(PATH)),;,:)$(PATH)
endif
ifdef CONFIG_OSX
LIBS += $(LINK_LIBSWIRL)
endif
ifeq ($(ARCH),arm)
# swirltest refers to the alignment of functions, and with thumb mode
# the low bit of code addresses selects the mode, so the "alignment"
# of functions via bit masking comes out as 1.  Just disable thumb.
test.ref: CFLAGS+=-marm
endif
ifeq ($(ARCH)$(CONFIG_WIN32),i386)
# swirltest.c:get_asm_string uses a construct that is checked too strictly
# by GCC in 32bit mode when PIC is enabled.
test.ref: CFLAGS+=-fno-PIC -fno-PIE -Wl,-z,notext
endif
ifeq ($(CC_NAME),msvc)
test.ref abitest : CC = gcc
endif
ifeq ($(TARGETOS),OpenBSD)
dlltest: CFLAGS+=-fno-stack-protector
endif
ifneq (,$(filter FreeBSD NetBSD,$(TARGETOS)))
  # test3 has dlsym problems
  TESTS := $(filter-out test3,$(TESTS))
  TESTS += test1
endif

RUN_SWIRL = $(NATIVE_DEFINES) -run $(TOPSRC)/swirl.c $(SWIRLFLAGS)
DISAS = objdump -d
ifdef CONFIG_OSX
DUMPSWIRL = (set -x; $(TOP)/swirl -vv; otool -L $(TOP)/swirl; exit 1)
else
DUMPSWIRL = (set -x; $(TOP)/swirl -vv; ldd $(TOP)/swirl; exit 1)
endif

all test :
	@$(MAKE) --no-print-directory -s clean
	@$(MAKE) --no-print-directory -s -r $(TESTS)

hello-exe: ../examples/ex1.c
	@echo ------------ $@ ------------
	$(SWIRL) $< -o hello$(EXESUF) && ./hello$(EXESUF) || $(DUMPSWIRL)

hello-run: ../examples/ex1.c
	@echo ------------ $@ ------------
	$(SWIRL) -run $< || $(DUMPSWIRL)

libtes%: libswirl_tes%$(EXESUF)
	@echo ------------ $@ ------------
	./libswirl_tes$*$(EXESUF) $(TOPSRC)/swirl.c $(SWIRLFLAGS) $(NATIVE_DEFINES)

libswirl_test$(EXESUF): libswirl_test.c $(LIBSWIRL)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

libswirl_test_mt$(EXESUF): libswirl_test_mt.c $(LIBSWIRL)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

%-dir:
	@echo ------------ $@ ------------
	$(MAKE) -k -C $*

# test.ref - generate using cc
test.ref: swirltest.c
	$(CC) -o swirltest.gcc $< $(NATIVE_DEFINES) $(CFLAGS) -w -O0 -std=gnu99 -fno-omit-frame-pointer
	./swirltest.gcc > $@

# auto test
test1 test1b: swirltest.c test.ref
	@echo ------------ $@ ------------
	$(SWIRL) $(RUN_SWIRL) -w -run $< > test.out1
	@diff -u test.ref test.out1 && echo "$(AUTO_TEST) OK"

# iterated test2 (compile swirl then compile swirltest.c !)
test2 test2b: swirltest.c test.ref
	@echo ------------ $@ ------------
	$(SWIRL) $(RUN_SWIRL) $(RUN_SWIRL) -w -run $< > test.out2
	@diff -u test.ref test.out2 && echo "$(AUTO_TEST)2 OK"

# iterated test3 (compile swirl then compile swirl then compile swirltest.c !)
test3 test3b: swirltest.c test.ref
	@echo ------------ $@ ------------
	$(SWIRL) $(RUN_SWIRL) $(RUN_SWIRL) $(RUN_SWIRL) -w -run $< > test.out3
	@diff -u test.ref test.out3 && echo "$(AUTO_TEST)3 OK"

AUTO_TEST = Auto Test
test%b : SWIRLFLAGS += -b -bt1
test%b : AUTO_TEST = Auto Bound-Test

# binary output test
test4: swirltest.c test.ref
	@echo ------------ $@ ------------
# object + link output
	$(SWIRL) -c -o swirltest3.o $<
	$(SWIRL) -o swirltest3 swirltest3.o
	./swirltest3 > test3.out
	@if diff -u test.ref test3.out ; then echo "Object $(AUTO_TEST) OK"; fi
# dynamic output
	$(SWIRL) -o swirltest1 $<
	./swirltest1 > test1.out
	@if diff -u test.ref test1.out ; then echo "Dynamic $(AUTO_TEST) OK"; fi
# dynamic output + bound check
	$(SWIRL) -b -o swirltest4 $<
	./swirltest4 > test4.out
	@if diff -u test.ref test4.out ; then echo "BCheck $(AUTO_TEST) OK"; fi

test4_static: swirltest.c test.ref
	@echo ------------ $@ ------------
# static output.
	$(SWIRL) -static -o swirltest2 $<
	./swirltest2 > test2.out
	@if diff -u test.ref test2.out ; then echo "Static $(AUTO_TEST) OK"; fi

# use swirl to create libswirl.so/.dll and the swirl(.exe) frontend and run them
dlltest:
	@echo ------------ $@ ------------
	$(SWIRL) $(NATIVE_DEFINES) -DLIBSWIRL_AS_DLL $(TOPSRC)/libswirl.c $(LIBS) -shared -o libswirl2$(DLLSUF)
	$(SWIRL) $(NATIVE_DEFINES) -DONE_SOURCE=0 $(TOPSRC)/swirl.c libswirl2$(DLLSUF) $(LIBS) -Wl,-rpath=. -o swirl2$(EXESUF)
	./swirl2$(EXESUF) $(SWIRLFLAGS) $(RUN_SWIRL) -run $(TOPSRC)/examples/ex1.c
ifndef CONFIG_WIN32
	@echo ------------ $@ with PIC ------------
	$(CC) $(CFLAGS) -fPIC $(NATIVE_DEFINES) -DLIBSWIRL_AS_DLL -c $(TOPSRC)/libswirl.c
	$(SWIRL) libswirl.o $(LIBS) -shared -o libswirl2$(DLLSUF)
	$(SWIRL) $(NATIVE_DEFINES) -DONE_SOURCE=0 $(TOPSRC)/swirl.c libswirl2$(DLLSUF) $(LIBS) -Wl,-rpath=. -o swirl2$(EXESUF)
	./swirl2$(EXESUF) $(SWIRLFLAGS) $(RUN_SWIRL) -run $(TOPSRC)/examples/ex1.c
endif
	@rm swirl2$(EXESUF) libswirl2$(DLLSUF)

memtest:
	@echo ------------ $@ ------------
	$(CC) $(CFLAGS) $(NATIVE_DEFINES) -DMEM_DEBUG=2 $(TOPSRC)/swirl.c $(LIBS) -o memtest-swirl$(EXESUF)
	./memtest-swirl$(EXESUF) $(SWIRLFLAGS) $(NATIVE_DEFINES) $(TOPSRC)/swirl.c $(LIBS)
	./memtest-swirl$(EXESUF) $(SWIRLFLAGS) $(NATIVE_DEFINES) -run $(TOPSRC)/swirl.c $(SWIRLFLAGS) -w $(TOPSRC)/tests/swirltest.c
	@echo OK

# memory and bound check auto test
BOUNDS_OK  = 1 4 8 10 14 16
BOUNDS_FAIL= 2 5 6 7 9 11 12 13 15 17 18

btest: boundtest.c
	@echo ------------ $@ ------------
	@for i in $(BOUNDS_OK); do \
	   if $(SWIRL) -b -run $< $$i >/dev/null 2>&1 ; then \
	       echo "Test $$i succeeded as expected" ; \
	   else\
	       echo "Failed positive test $$i" ; exit 1 ; \
	   fi ;\
	done ;\
	for i in $(BOUNDS_FAIL); do \
	   if $(SWIRL) -b -bt1 -run $< $$i >/dev/null 2>&1 ; then \
	       echo "Failed negative test $$i" ; exit 1 ;\
	   else\
	       echo "Test $$i failed as expected" ; \
	   fi ;\
	done ;\
	echo Bound test OK

# speed test
speedtest: ex2 ex3
	@echo ------------ $@ ------------
	time ./ex2 1238 2 3 4 10 13 4
	time $(SWIRL) -run $(TOPSRC)/examples/ex2.c 1238 2 3 4 10 13 4
	time ./ex3 35
	time $(SWIRL) -run $(TOPSRC)/examples/ex3.c 35

weaktest: swirltest.c test.ref
	@echo ------------ $@ ------------
	$(SWIRL) -c $< -o weaktest.swirl.o
	$(CC) -c $< -o weaktest.gcc.o $(NATIVE_DEFINES) $(CFLAGS) -w -O0 -std=gnu99 -fno-omit-frame-pointer
	objdump -t weaktest.swirl.o | grep ' w ' | sed -e 's/.* \([a-zA-Z0-9_]*\)$$/\1/' | LC_ALL=C sort > weaktest.swirl.o.txt
	objdump -t weaktest.gcc.o | grep ' w ' | sed -e 's/.* \([a-zA-Z0-9_]*\)$$/\1/' | LC_ALL=C sort > weaktest.gcc.o.txt
	diff weaktest.gcc.o.txt weaktest.swirl.o.txt && echo "Weak Auto Test OK"

ex%: $(TOPSRC)/examples/ex%.c
	$(CC) -o $@ $< $(CFLAGS)

# tiny assembler testing
asmtest.ref: asmtest.S
	$(CC) -Wa,-W -o asmtest.ref.o -c asmtest.S
	objdump -D asmtest.ref.o > asmtest.ref

ifeq ($(ARCH),arm)
asmtest asmtest2:
	SWIRL="${SWIRL}" ./arm-asm-testsuite.sh
else
asmtest asmtest2: asmtest.ref
	@echo ------------ $@ ------------
	$(SWIRL) $(MAYBE_RUN_SWIRL) -c asmtest.S
	objdump -D asmtest.o > asmtest.out
	@if diff -u --ignore-matching-lines="file format" asmtest.ref asmtest.out ; then echo "ASM Auto Test OK"; fi
endif

# test assembler with swirl compiled by itself
asmtest2: MAYBE_RUN_SWIRL = $(RUN_SWIRL)

# check that code generated by libswirl is binary compatible with
# that generated by CC
abitest-cc.exe: abitest.c $(LIBSWIRL)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) -w

abitest-swirl.exe: abitest.c libswirl.c
	$(SWIRL) -o $@ $^ $(NATIVE_DEFINES) $(LIBS)

abitest-% : abitest-%.exe
	@echo ------------ $@ ------------
	./$< $(SWIRLFLAGS)

abitest: abitest-cc
ifneq ($(CONFIG_arm_eabi),yes) # not ARM soft-float
abitest: abitest-swirl
endif

vla_test$(EXESUF): vla_test.c
	$(SWIRL) -o $@ $^

vla_test-run: vla_test$(EXESUF)
	@echo ------------ $@ ------------
	./vla_test$(EXESUF)

asm-c-connect$(EXESUF): asm-c-connect-1.c asm-c-connect-2.c
	$(SWIRL) -o $@ $^

asm-c-connect-%.o: asm-c-connect-%.c
	$(SWIRL) -c -o $@ $<

asm-c-connect-sep$(EXESUF): asm-c-connect-1.o asm-c-connect-2.o
	$(SWIRL) -o $@ $^

asm-c-connect-test: asm-c-connect$(EXESUF) asm-c-connect-sep$(EXESUF)
	@echo ------------ $@ ------------
	./asm-c-connect$(EXESUF) > asm-c-connect.out1 && cat asm-c-connect.out1
	./asm-c-connect-sep$(EXESUF) > asm-c-connect.out2 && cat asm-c-connect.out2
	@diff -u asm-c-connect.out1 asm-c-connect.out2 || (echo "error"; exit 1)

# quick sanity check for cross-compilers
cross-test : swirltest.c examples/ex3.c
	@echo ------------ $@ ------------
	$(foreach T,$(CROSS-TGTS),$(call CROSS-COMPILE,$T))

CROSS-TGTS = \
 i386 \
 i386-win32 \
 i386-OpenBSD \
 x86_64 \
 x86_64-win32 \
 x86_64-osx \
 x86_64-FreeBSD \
 x86_64-NetBSD \
 x86_64-OpenBSD \
 arm-fpa \
 arm-eabihf \
 arm-NetBSD \
 arm-wince \
 arm64 \
 arm64-FreeBSD \
 arm64-NetBSD \
 arm64-OpenBSD \
 riscv64 \
 c67

define CROSS-COMPILE
 @echo " . $(1)"
 $(SWIRL) $(DEF-$1) -DSWIRL_CROSS_TEST -run $(TOPSRC)/swirl.c \
    -c $(if $(findstring c67,$1),$(filter %/ex3.c,$^),$<) -w $(SWIRLFLAGS)

endef

# targets for development
%.bin: %.c swirl
	$(SWIRL) -g -o $@ $<
	$(DISAS) $@

instr: instr.o
	objdump -d instr.o

instr.o: instr.S
	$(CC) -o $@ -c $< -O2 -Wall -g

cache: swirl_g
	cachegrind ./swirl_g -o /tmp/linpack -lm bench/linpack.c
	vg_annotate swirl.c > /tmp/linpack.cache.log

# clean
clean:
	rm -f *~ *.o *.a *.bin *.i *.ref *.out *.out? *.out?b *.cc *.gcc
	rm -f *-cc *-gcc *-swirl *.exe hello libswirl_test vla_test swirltest[1234]
	rm -f asm-c-connect$(EXESUF) asm-c-connect-sep$(EXESUF)
	rm -f ex? swirl_g weaktest.*.txt *.def *.pdb *.obj libswirl_test_mt
	@$(MAKE) -C tests2 $@
	@$(MAKE) -C pp $@

