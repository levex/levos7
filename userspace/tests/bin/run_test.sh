#!/bin/bash

TEST=$1

if [[  -e testoutput ]]
then
	rm testoutput
fi

if [[ ! -e results ]]
then
	touch results
fi

mkfifo testoutput
qemu-system-x86_64 \
	-kernel ../../kernel.img \
	-m 256 \
	-serial stdio \
	-monitor null \
	-nographic \
	-no-reboot \
	-hda test.img \
	-D /dev/null \
	-pidfile qemu.pid \
		2> /dev/null \
		| tee $TEST.output testoutput > /dev/null &

sleep 1
pid=`cat qemu.pid`
result=`grep --max-count 1 -e "panic\|SUCCESS\|FAILURE" < testoutput`
status=$?
kill $pid >/dev/null
echo $result | grep -i SUCCESS > /dev/null
what=$?
#echo "$what" >> results
if [[ $what == 0 ]]
then
	echo "SUCCESS $TEST" >> results
else
	echo "FAILURE $TEST" >> results
fi
