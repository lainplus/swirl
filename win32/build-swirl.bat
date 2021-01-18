@rem -----------------------------------------------------------
@rem batch file to build swirl using mingw, msvc or swirl itself
@rem -----------------------------------------------------------

@echo off
setlocal
if (%1)==(-clean) goto :cleanup
set CC=gcc
set /p VERSION= < ..\VERSION
set INST=
set BIN=
set DOC=no
set EXES_ONLY=no
goto :a0
:a2
shift
:a3
shift
:a0
if not (%1)==(-c) goto :a1
set CC=%~2
if (%2)==(cl) set CC=@call :cl
goto :a2
:a1
if (%1)==(-t) set T=%2&& goto :a2
if (%1)==(-v) set VERSION=%~2&& goto :a2
if (%1)==(-i) set INST=%2&& goto :a2
if (%1)==(-b) set BIN=%2&& goto :a2
if (%1)==(-d) set DOC=yes&& goto :a3
if (%1)==(-x) set EXES_ONLY=yes&& goto :a3
if (%1)==() goto :p1
:usage
echo usage: build-swirl.bat [ options ... ]
echo options:
echo   -c prog              use prog (gcc/swirl/cl) to compile swirl
echo   -c "prog options"    use prog with options to compile swirl
echo   -t 32/64             force 32/64 bit default target
echo   -v "version"         set swirl version
echo   -i swirldir            install swirl into swirldir
echo   -b bindir            optionally install binaries into bindir elsewhere
echo   -d                   create swirl-doc.html too (needs makeinfo)
echo   -x                   just create the executables
echo   -clean               delete all previously produced files and directories
exit /B 1

@rem ------------------------------------------------------
@rem sub-routines

:cleanup
set LOG=echo
%LOG% removing files:
for %%f in (*swirl.exe libswirl.dll lib\*.a) do call :del_file %%f
for %%f in (..\config.h ..\config.texi) do call :del_file %%f
for %%f in (include\*.h) do @if exist ..\%%f call :del_file %%f
for %%f in (include\swirllib.h examples\libswirl_test.c) do call :del_file %%f
for %%f in (lib\*.o *.o *.obj *.def *.pdb *.lib *.exp *.ilk) do call :del_file %%f
%LOG% removing directories:
for %%f in (doc libswirl) do call :del_dir %%f
%LOG% done.
exit /B 0
:del_file
if exist %1 del %1 && %LOG%   %1
exit /B 0
:del_dir
if exist %1 rmdir /Q/S %1 && %LOG%   %1
exit /B 0

:cl
@echo off
set CMD=cl
:c0
set ARG=%1
set ARG=%ARG:.dll=.lib%
if (%1)==(-shared) set ARG=-LD
if (%1)==(-o) shift && set ARG=-Fe%2
set CMD=%CMD% %ARG%
shift
if not (%1)==() goto :c0
echo on
%CMD% -O1 -W2 -Zi -MT -GS- -nologo -link -opt:ref,icf
@exit /B %ERRORLEVEL%

@rem ------------------------------------------------------
@rem main program

:p1
if not %T%_==_ goto :p2
set T=32
if %PROCESSOR_ARCHITECTURE%_==AMD64_ set T=64
if %PROCESSOR_ARCHITEW6432%_==AMD64_ set T=64
:p2
if "%CC:~-3%"=="gcc" set CC=%CC% -Os -s -static
set D32=-DSWIRL_TARGET_PE -DSWIRL_TARGET_I386
set D64=-DSWIRL_TARGET_PE -DSWIRL_TARGET_X86_64
set P32=i386-win32
set P64=x86_64-win32
if %T%==64 goto :t64
set D=%D32%
set DX=%D64%
set PX=%P64%
goto :p3
:t64
set D=%D64%
set DX=%D32%
set PX=%P32%
goto :p3

:p3
@echo on

:config.h
echo>..\config.h #define SWIRL_VERSION "%VERSION%"
echo>> ..\config.h #ifdef SWIRL_TARGET_X86_64
echo>> ..\config.h #define SWIRL_LIBSWIRL1 "libswirl1-64.a"
echo>> ..\config.h #else
echo>> ..\config.h #define SWIRL_LIBSWIRL1 "libswirl1-32.a"
echo>> ..\config.h #endif

for %%f in (*swirl.exe *swirl.dll) do @del %%f

:compiler
%CC% -o libswirl.dll -shared ..\libswirl.c %D% -DLIBSWIRL_AS_DLL
@if errorlevel 1 goto :the_end
%CC% -o swirl.exe ..\swirl.c libswirl.dll %D% -DONE_SOURCE"=0"
%CC% -o %PX%-swirl.exe ..\swirl.c %DX%

@if (%EXES_ONLY%)==(yes) goto :files_done

if not exist libswirl mkdir libswirl
if not exist doc mkdir doc
copy>nul ..\include\*.h include
copy>nul ..\swirllib.h include
copy>nul ..\libswirl.h libswirl
copy>nul ..\tests\libswirl_test.c examples
copy>nul swirl-win32.txt doc

.\swirl -impdef libswirl.dll -o libswirl\libswirl.def
@if errorlevel 1 goto :the_end

:libswirl1.a
@set O1=libswirl1.o crt1.o crt1w.o wincrt1.o wincrt1w.o dllcrt1.o dllmain.o chkstk.o
.\swirl -m32 -c ../lib/libswirl1.c
.\swirl -m32 -c lib/crt1.c
.\swirl -m32 -c lib/crt1w.c
.\swirl -m32 -c lib/wincrt1.c
.\swirl -m32 -c lib/wincrt1w.c
.\swirl -m32 -c lib/dllcrt1.c
.\swirl -m32 -c lib/dllmain.c
.\swirl -m32 -c lib/chkstk.S
.\swirl -m32 -c ../lib/alloca86.S
.\swirl -m32 -c ../lib/alloca86-bt.S
.\swirl -m32 -ar lib/libswirl1-32.a %O1% alloca86.o alloca86-bt.o
@if errorlevel 1 goto :the_end
.\swirl -m64 -c ../lib/libswirl1.c
.\swirl -m64 -c lib/crt1.c
.\swirl -m64 -c lib/crt1w.c
.\swirl -m64 -c lib/wincrt1.c
.\swirl -m64 -c lib/wincrt1w.c
.\swirl -m64 -c lib/dllcrt1.c
.\swirl -m64 -c lib/dllmain.c
.\swirl -m64 -c lib/chkstk.S
.\swirl -m64 -c ../lib/alloca86_64.S
.\swirl -m64 -c ../lib/alloca86_64-bt.S
.\swirl -m64 -ar lib/libswirl1-64.a %O1% alloca86_64.o alloca86_64-bt.o
@if errorlevel 1 goto :the_end
.\swirl -m%T% -c ../lib/bcheck.c -o lib/bcheck.o -g
.\swirl -m%T% -c ../lib/bt-exe.c -o lib/bt-exe.o
.\swirl -m%T% -c ../lib/bt-log.c -o lib/bt-log.o
.\swirl -m%T% -c ../lib/bt-dll.c -o lib/bt-dll.o

:swirl-doc.html
@if not (%DOC%)==(yes) goto :doc-done
echo>..\config.texi @set VERSION %VERSION%
cmd /c makeinfo --html --no-split ../swirl-doc.texi -o doc/swirl-doc.html
:doc-done

:files_done
for %%f in (*.o *.def) do @del %%f

:copy-install
@if (%INST%)==() goto :the_end
if not exist %INST% mkdir %INST%
@if (%BIN%)==() set BIN=%INST%
if not exist %BIN% mkdir %BIN%
for %%f in (*swirl.exe *swirl.dll) do @copy>nul %%f %BIN%\%%f
@if not exist %INST%\lib mkdir %INST%\lib
for %%f in (lib\*.a lib\*.o lib\*.def) do @copy>nul %%f %INST%\%%f
for %%f in (include examples libswirl doc) do @xcopy>nul /s/i/q/y %%f %INST%\%%f

:the_end
exit /B %ERRORLEVEL%
