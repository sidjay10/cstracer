# Valgrind with cstracer

This repository contains a new Valgrind tool called "cstracer."
cstracer generates traces for use with ChampSim

Note: This tool is an EXPERIMENTAL PROTOTYPE used for research purposes.
It's neither fast nor safe to use. You have been warned!

## Compiling
See the original Valgrind [README](README) for instructions on building valgrind.

In the valgrind directory, create a directory named `cstracer` containing the files in this repository  
To the file Makefile.am in the valgrind directory, add `cstracer` to the variable named `TOOLS`  
To the file configure.in in the valgrind directory, add `cstracer/Makefile` and `cstracer/tests/Makefile` to `AC_CONFIG_FILES`  

In the valgrind directory, run the following commands  

~~~
./autogen.sh
# adjust the prefix to your preferred location
./configure --prefix=$PWD/inst
make install
~~~

## Running
To generate a data and program trace from `YOUR_APPLICATION`, run

~~~
<path to valgrind>/valgrind --tool=cstracer --trace-file=tracefile --trace=1000 --skip=10 YOUR_APPLICATION
~~~

The traces are written to a file named tracefile

## Options

`--trace-file=<filename>` The name of the output file  
`--skip=<num>`	Number of initital instructions to skip  
`--trace=<num>`	Number of instructions to trace  

## Contact
Author: Siddharth Jayashankar <sidjay@iitk.ac.in>

