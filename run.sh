#!/bin/bash

for i in {1..10}
do
    time taskset 0x1 $1
done

