#!/bin/bash

#########################################################

# Build valgrind for linux x86-64

#########################################################

#########################################################
#Set these variables accordingly
#########################################################

VALGRIND_HOME=$HOME/cstracer

#########################################################

#########################################################

cd $VALGRIND_HOME 

make clean

./autogen.sh

./configure --prefix="$VALGRIND_HOME/inst_x86-64" \

make clean

make -j16
make -j16 install
