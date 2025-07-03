#!/bin/sh

rm -f tmpsign.sh
lwmake GSPHSCFG_PROFILE=gsphs-tu10x clobber build release

RC=$?
if [ $RC -eq 0 -a -f tmpsign.sh ]
then
  sh tmpsign.sh
  RC=$?
fi

rm -f tmpsign.sh

if [ $RC -ne 0 ]
then
  echo "*                                                   "
  echo "****************************************************"
  echo "*                                                  *"
  echo "*  Error:  GSP HS build has FAILED !!!             *"
  echo "*                                                  *"
  echo "****************************************************"
  echo "*                                                   "
fi

exit $RC

