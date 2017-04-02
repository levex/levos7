#!/bin/bash

if [[ $1 ]]
then
	echo -n "  "
	cat results | grep $1 | awk '{print $1}'
	exit
fi

WORKS=`cat results | grep "SUCCESS" | wc -l`
DOESNTWORK=`cat results | grep "FAILURE" | wc -l`

echo "$WORKS tests pass, $DOESNTWORK test fail"

cat results
