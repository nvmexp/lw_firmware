#!/bin/sh


if [ -f ./mods.arg ];  then 
 ./mods 
else
 ./mods -s
fi 
