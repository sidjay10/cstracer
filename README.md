# Valgrind with cstracer

This repository contains a new Valgrind tool called "cstracer."
cstracer generates traces for use with ChampSim

Note: This tool is an EXPERIMENTAL PROTOTYPE used for research purposes.
It's neither fast nor safe to use. You have been warned!

## Compiling

### andorid aarch64
To build the cstracer tool for android aarch 64, 

1. Install android ndk-10rc if not already done.

2. Point the NDK_HOME variable in the build_scripts/aarch64-ndk10.sh to the android ndk on your system.

3. Point the VALGRIND_HOME variable in the build_scripts/aarch64-ndk10.sh to the root of the cstracer directory.

Then 
~~~
bash build_scripts/aarch64-ndk10.sh
cd Inst-aarch64/data/local/tmp
adb push Inst/ /data/local/tmp/
adb shell export VALGRIND_LIB=/data/local/tmp/lib/valgrind/
~~~

### andorid x86-64
To build the cstracer tool for android x86-64, 

1. Install android ndk-10rc if not already done.

2. Point the NDK_HOME variable in the build_scripts/amd64-ndk10.sh to the android ndk on your system.

3. Point the VALGRIND_HOME variable in the build_scripts/amd64-ndk10.sh to the root of the cstracer directory.

Then 
~~~
bash build_scripts/amd64-ndk10.sh
cd Inst-amd64/data/local/tmp
adb push Inst/ /data/local/tmp/
adb shell export VALGRIND_LIB=/data/local/tmp/lib/valgrind/
~~~

### Linux x86-64
1. Point the VALGRIND_HOME variable in the build_scripts/amd64-linux.sh to the root of the cstracer directory.
~~~
bash build_scripts/amd64-linux.sh
~~~
Valgrind will be installed in the directory inst_x86-64/bin/

### For More Info
See the original Valgrind [README](README.valgrind) for detailed instructions on building valgrind.

## Running

### Linux Executable
To generate a trace for `EXECUTABLE`, run
~~~
<path to valgrind>/valgrind --tool=cstracer --trace-file=tracefile --trace=1000 --skip=10 EXECUTABLE EXECUTABLE_ARGS
~~~

### Android Application
To generate a trace for `APK`, run
~~~
bash android_scripts/init.sh
~~~
set the PACKAGE and ACTIVITY variables in android_scripts/vg.sh and android_scripts/bs.sh
~~~
bash android_scripts/run_apk.sh
~~~

The traces are written to a file named /data/local/tmp/traces/trace_pid where pid is the pid of the process being traced. 
The execution log is present in /data/local/tmp/vglogs/logs.pid where pid is the pid of the process being traced. 

### Android Executable
To generate a trace for `EXECUTABLE`, run
~~~
bash android_scripts/init.sh
adb shell /data/local/tmp/valgrind --tool=cstracer --trace-file=tracefile --trace=1000 --skip=10 EXECUTABLE EXECUTABLE_ARGS
~~~

## Tool Options

`--trace-file=<filename>` The name of the output file  
`--skip=<num>`	Number of initital instructions to skip  
`--trace=<num>`	Number of instructions to trace  
`--exit-after=<yes|no>` Halt execution after the tracing is completed

## Notes

1. Valgrind is not compatible with android version 10 (as of now) due to the introduction of execute only memory.

2. Running cstracer requires root privileges.

## Contact
Author: Siddharth Jayashankar <sidjay@iitk.ac.in>
