#!/bin/bash
adb push android_scripts/vg.sh /data/local/tmp
adb push android_scripts/bs.sh /data/local/tmp
adb shell "root"
adb shell "chmod -R 777 /data/local/tmp/vg.sh"
adb shell "chmod -R 777 /data/local/tmp/bs.sh"

adb shell "/data/local/tmp/bs.sh"
