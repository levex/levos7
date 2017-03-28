#!/bin/bash

WORKS=`cat results | grep "SUCCESS" | wc -l`
DOESNTWORK=`cat results | grep "FAILURE" | wc -l`

echo "$WORKS tests pass, $DOESNTWORK test fail"

cat results
