#!/bin/bash
adb shell "root"
adb shell "chmod -R 777 /data/local/tmp/Inst"
adb shell "export VALGRIND_LIB=/data/local/tmp/Inst/lib/valgrind"
adb shell "mkdir -p /data/local/tmp/traces"
adb shell "chmod -R 777 /data/local/tmp/traces"
adb shell "mkdir -p /data/local/tmp/vglogs"
adb shell "chmod -R 777 /data/local/tmp/vglogs"

## Disable SELINUX
adb shell "setenforce 0"
adb shell "stop"
adb shell "start"
