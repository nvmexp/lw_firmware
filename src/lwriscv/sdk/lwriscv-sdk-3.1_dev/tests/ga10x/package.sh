#!/bin/sh
./build.sh install

BRANCH=`git rev-parse --symbolic-full-name --abbrev-ref HEAD`
COMMIT=`git rev-parse --short HEAD`
DIRTY=`git diff --quiet || echo '-dirty'`
DATE=`date "+%Y-%m-%d"`

NAME="ga10x-hacking-tools_${BRANCH}_${DATE}_${COMMIT}${DIRTY}"
echo "Creating archive $NAME..."
#echo $NAME
FNAME="${NAME}.tar.gz"
TNAME="${NAME}.tar"
rm -f $TNAME $FNAME

git archive --format=tar.gz HEAD > $FNAME
gzip -d ${FNAME}
tar --owner=0 --group=0 -r -f ${TNAME} `ls bin/*`
gzip --best ${TNAME}
