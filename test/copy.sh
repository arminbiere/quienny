#!/bin/sh
die () {
  echo "copy.sh: error: $*" 1>&2
  exit 1
}
[ $# = 1 ] || die "expected exactly one argument"
name=`basename $1 .out`
src=$name.out
dst=$name.gld
[ -f $src ] || die "could not find '$src'"
echo cp $src $dst
cp $src $dst
