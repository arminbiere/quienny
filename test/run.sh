#!/bin/sh
die () {
  echo "test/run.sh: error: $*" 1>&2
  exit 1
}

run () {
  [ -f ./$quienny ] || die "could not find '$quienny'"
  pol=test/$1.pol
  out=test/$1.out
  log=test/$1.log
  err=test/$1.err
  gld=test/$1.gld
  echo "./$quienny $pol $out"
  ./$quienny $pol $out 1>$log 2>$err
  status=$?
  if [ $status = 0 ]
  then
    if [ -f $gld ]
    then
      cmp $out $gld -s 1>/dev/null 2>/dev/null
      status=$?
      [ $status = 0 ] || die "mismatch of '$out' and '$gld'"
    else
      die "could not find golden output file '$gld'"
    fi
  else
    die "unexpected exit status '$status'"
  fi
}

for quienny in quienny quienny8 quienny16 quienny32 quienny64
do
  run empty
  run one
  run two
  run xor
  run xnor
  run ite
  run three
  run nothree
  run example
  run all3
  run noall3
  run all4
  run noall4
  case $quienny in
    quienny8|quienny16) continue;
  esac
  run two32
done

