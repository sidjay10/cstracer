# csTracer

This repository contains a new Valgrind tool called "cstracer."
cstracer that can generate traces of android applications running on 
android-aarch64 and android-x86-64 architectures and without requiring access
to their source code. The collected traces can be used for running simulations on 
[ChampSim](https://github.com/ChampSim/ChampSim).

Note: This tool is an experimental prototype used for research purposes.

For more information about the kind of work that can be done with csTracer traces and champSim, you can check out this [presentation](https://docs.google.com/presentation/d/1ItN-Z4M6NXTQTMUEJOMCRhUswRGA3jiho9J-yJkvMzM/edit?usp=sharing).


# Compiling

### android aarch64
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

### android x86-64
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

To trace android apps, you need to run an engineering build of android with these [patches](android_runtime_patches) applied.

Disable jit on android by setting dalvik.vm.usejit=false and dalvik.vm.usejitprofiles=false in /default.prop on the android device.

You also require an adb connection to the android device.

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

You can pull the traces using adb pull

### Android Executable
To generate a trace for `EXECUTABLE`, run
~~~
bash android_scripts/init.sh
adb shell /data/local/tmp/valgrind --tool=cstracer --trace-file=tracefile --trace=1000 --skip=10 EXECUTABLE EXECUTABLE_ARGS
~~~

## Tool Options - csTracer

`--trace-file=<filename>` The name of the output file  
`--skip=<num>`	Number of initital instructions to skip  
`--trace=<num>`	Number of instructions to trace  
`--heartbeat=<num>` Instruction interval at which output is written to the log file  
`--exit-after=<yes|no>` Halt execution after the tracing is completed

## Tool Options - ctLite

An auxiliary tool - ctlite is provided to collect more coarse grained information
about programs.

To use ctlite, set --tool=ctlite

ctlite comes with the following options

`--trace-file=<filename>` The name of the output file  
`--heartbeat=<num>` Instruction interval at which output is written to the log file   
`--mem-size=<num>` Log base 2 size of the memory window size. (For example, for a memory window size of 32KB, set `--mem-size=15`)  
`--code-size=<num>` Log base 2 size of the code window size

## Notes

1. To trace on android 10, execute only memory needs to be disabled. See [Execute Only Memory - source.android.com](https://source.android.com/devices/tech/debug/execute-only-memory)

2. Running valgrind on android to trace applications requires root privileges.

## Contact
Author: Siddharth Jayashankar <sidjay@iitk.ac.in>
