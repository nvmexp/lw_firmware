#!/bin/bash
p4 edit ...
shopt -s globstar
for i in **/test.log; do # Whitespace-safe and relwrsive
    target=$(dirname $i)/../$(basename $i)
    cp $i ${target/test/reference}
    p4 add ${target/test/reference}
done
p4 revert -a ...