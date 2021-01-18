#!/bin/sh

TESTSUITE_PATH=$HOME/gcc/gcc-3.2/gcc/testsuite/gcc.c-torture
SWIRL="./swirl -B. -I. -DNO_TRAMPOLINES"
rm -f swirl.sum swirl.fail
nb_ok="0"
nb_failed="0"
nb_exe_failed="0"

for src in $TESTSUITE_PATH/compile/*.c ; do
  echo $SWIRL -o /tmp/tst.o -c $src 
  $SWIRL -o /tmp/tst.o -c $src >> swirl.fail 2>&1
  if [ "$?" = "0" ] ; then
    result="PASS"
    nb_ok=$(( $nb_ok + 1 ))
  else
    result="FAIL"
    nb_failed=$(( $nb_failed + 1 ))
  fi
  echo "$result: $src"  >> swirl.sum
done

for src in $TESTSUITE_PATH/execute/*.c ; do
  echo $SWIRL $src -o /tmp/tst -lm
  $SWIRL $src -o /tmp/tst -lm >> swirl.fail 2>&1
  if [ "$?" = "0" ] ; then
    result="PASS"
    if /tmp/tst >> swirl.fail 2>&1
    then
      result="PASS"
      nb_ok=$(( $nb_ok + 1 ))
    else
      result="FAILEXE"
      nb_exe_failed=$(( $nb_exe_failed + 1 ))
    fi
  else
    result="FAIL"
    nb_failed=$(( $nb_failed + 1 ))
  fi
  echo "$result: $src"  >> swirl.sum
done

echo "$nb_ok test(s) ok." >> swirl.sum
echo "$nb_ok test(s) ok."
echo "$nb_failed test(s) failed." >> swirl.sum
echo "$nb_failed test(s) failed."
echo "$nb_exe_failed test(s) exe failed." >> swirl.sum
echo "$nb_exe_failed test(s) exe failed."
