#Logwrapper Script to Launch App with Valgrind

#!/system/bin/sh

#PACKAGE=com.android.calculator2
#PACKAGE=com.antutu.ABenchMark
PACKAGE=com.primatelabs.geekbench5

#TRACING:
SKIP=82 	# In Billion
TRAC=250	# In Million

SKIP=${SKIP}000000000
TRAC=${TRAC}000000

#VGPARAMS="--aspace-minaddr=0x100000000 --tool=none"

VGPARAMS="--trace-children=yes --log-file=/data/local/tmp/vglogs/logs.%p --aspace-minaddr=0x100000000 --tool=cstracer --trace-file=/data/local/tmp/traces/trace --exit-after=no --skip=${SKIP} --trace=${TRAC}"

#VGPARAMS="--trace-children=yes --log-file=/data/local/tmp/vglogs/logs.%p --aspace-minaddr=0x100000000 --tool=ctlite --trace-file=/data/local/tmp/traces/trace --code-size=16 --mem-size=16"
export TMPDIR=/data/data/$PACKAGE

exec /data/local/tmp/Inst/bin/valgrind $VGPARAMS $@
