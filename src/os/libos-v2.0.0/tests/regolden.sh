#!/bin/bash
p4 edit ...
shopt -s globstar
for i in **/passed.*; do # Whitespace-safe and relwrsive
    target=$(dirname $i)/../../$(basename $i)
    cp $i ${target/passed/golden}
done
p4 revert -a ...