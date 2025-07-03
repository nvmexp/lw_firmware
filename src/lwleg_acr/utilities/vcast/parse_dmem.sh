#!/bin/bash

dump=$1
size=$2

function rev_endian () {
	dw=$1
	new_dw=""
	while read -n2 byte; do
		byte=$(echo $byte | rev)
		new_dw="$new_dw ${byte^^}"
	done < <(echo $dw | rev)
	new_dw=$(echo $new_dw | tr -d ' ')
	echo $new_dw
}

dwords=""
while read line; do
	line_tr=$(echo $line | tr -s ' ')
	for token in $line_tr; do
		dw=$(echo $token | /bin/grep -Pi "0x[0-9A-F]{8}" | cut -f 2 -d 'x')
		dwords="$dwords $dw"
	done
done < $dump

dwords=$(echo $dwords | tr -d ' ')
new_dwords=""
while read -n8 dw; do
	new_dw=$(rev_endian $dw)
	new_dwords="$new_dwords $new_dw"
done < <(echo $dwords)

new_dwords=$(echo $new_dwords | tr -d ' ')
codec_size=$((size * 2))
codec=$(echo $new_dwords | cut -c 1-$codec_size)
echo $codec
