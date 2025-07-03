#!/bin/tcsh

p4 edit config.txt

cd ./configs
# rm -f ../config.txt
ls -1 bug_*/*.txt > ../config.txt
ls alloc_testing/*.txt  >> ../config.txt
ls *.txt  >> ../config.txt
ls expected_fails/*.fail >> ../config.txt
ls bug_285605/*.fail  >>  ../config.txt
ls min_clocks/*.txt  >> ../config.txt
ls min_clocks/*.fail >> ../config.txt
ls training/*.txt  >> ../config.txt
ls gt218/*.txt  >> ../config.txt
ls apple_study/*.txt >>  ../config.txt
ls asr/*  >> ../config.txt
ls gt21a/*.txt  >> ../config.txt
ls gf100/*.txt >> ../config.txt
