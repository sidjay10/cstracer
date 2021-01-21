# Valgrind with cstracer

This repository contains a new Valgrind tool called "cstracer."
cstracer generates traces for use with ChampSim

Note: This tool is an EXPERIMENTAL PROTOTYPE used for research purposes.
It's neither fast nor safe to use. You have been warned!

## Compiling
See the original Valgrind [README](README.valgrind) for instructions on building valgrind.

### andorid aarch64
To build the cstracer tool for android aarch 64, 

1. Install android ndk-10rc if not already done.

2. Point the NDK_HOME variable in the build_ndk10_gcc.sh to the android ndk on your system.

Then 
~~~
bash build_ndk10_gcc.sh
cd Inst/data/local/tmp
adb push Inst/ /data/local/tmp/
adb shell export VALGRIND_LIB=/data/local/tmp/lib/valgrind/
~~~

### Linux x86-64
~~~
bash build_x86-64.sh
~~~
Valgrind will be installed in the directory Inst_x86-64/bin/

## Running
To generate a data and program trace from `YOUR_APPLICATION`, run

~~~
<path to valgrind>/valgrind --tool=cstracer --trace-file=tracefile --trace=1000 --skip=10 YOUR_APPLICATION
~~~

The traces are written to a file named tracefile_pid where pid is the pid of the process being traced. 

## Options

`--trace-file=<filename>` The name of the output file  
`--skip=<num>`	Number of initital instructions to skip  
`--trace=<num>`	Number of instructions to trace  
`--exit-after=<yes|no>` Halt execution after the tracing is completed

## Note

Valgrind is not compatible with android version 10 (as of now) due to the introduction of execute only memory.

## Contact
Author: Siddharth Jayashankar <sidjay@iitk.ac.in>

