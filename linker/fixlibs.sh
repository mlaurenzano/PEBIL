#!/bin/bash
mkdir -p ${PWD}/fixlibs
for i in `ldd $1 | awk '$2~/=>/&&$3!="not"{print $3;} $2!~/=>/{print $1;}'`
do
   cp -f $i fixlibs
done
