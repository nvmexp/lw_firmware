#!/bin/sh

if [ $# -lt 1 ] ; then
  UNIX_BUILD="unix-build"
else
  UNIX_BUILD="$1"
fi

# Run lwmake inside unix-build.
LWMAKE_CMD="${UNIX_BUILD} --no-devrel --no-read-only-bind-mounts"
# Pass through DVS environment variables.
LWMAKE_CMD="${LWMAKE_CMD} --elwvar LW_DVS_BLD --elwvar CHANGELIST"
# Bind-mount extra directories.
# For some reason falcon GCC fails with SEGV if /proc isn't present.
LWMAKE_CMD="${LWMAKE_CMD} --extra ${LW_TOOLS}/falcon-gcc --extra /proc"
# Actually ilwoke lwmake.
LWMAKE_CMD="${LWMAKE_CMD} lwmake"

${LWMAKE_CMD} clean
${LWMAKE_CMD} clobber
${LWMAKE_CMD} -k
RC=$?
rm -rf selwrescrub_build.succeded
if [ $RC -eq 0 ]
then
    touch selwrescrub_build.succeded
fi   
