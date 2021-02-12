#Shell Script to Launch Application with Valgrind Wrapper

#!/system/bin/sh

#PACKAGE=com.android.calculator2
#ACTIVITY=.Calculator

#PACKAGE=com.antutu.ABenchMark
#ACTIVITY=.ABenchMarkStart

PACKAGE=com.primatelabs.geekbench
ACTIVITY=.HomeActivity

#ACTIVITY=com.antutu.benchmark.ui.teststress.activity.TestStressActivity

setprop wrap.$PACKAGE "logwrapper /data/local/tmp/vg.sh"

am force-stop $PACKAGE
logcat -c
am start -n $PACKAGE/$ACTIVITY & logcat > a


#am start service 
#com.antutu.ABenchMark/com.antutu.benchmark.service.BenchmarkMainService

