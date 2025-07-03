#!/bin/sh

diagExelwtable="./griddiag"

myName=$(basename $0)

gpuArgIsSet=false
longArgIsSet=false
wasError=false

wasgError=false # prevent multiple g option error messages

usage()
{
    cat <<EOF
usage: $myName [options]
OPTIONS
    -g GPUNUMBER
        Execute the diagnostic on the GPUNUMBER device. If this option is
        omitted, then on the first available device.
    -l
        Execute the long version of the diagnostic.
    -h
        Print this help message.
EXAMPLES
    Execute the short diagnostic on all GPUs.
        grid-diagnostic.sh
    Execute the long diagnostic on GPU 0.
        grid-diagnostic.sh -l -g 0
EOF
}

while getopts ":g:lLshn" Option
do
  case $Option in
    g )
      if [ "$gpuArgIsSet" = true -a "$wasgError" = false ]; then
          echo "Option -g can be specified only once." >&2
          wasError=true
          wasgError=true
      fi
      diagArgs="$diagArgs device=$OPTARG"
      gpuArgIsSet=true
      ;;
    l )
      longArgIsSet=true
      ;;
    h )
      usage
      exit 0
      ;;
    \? )
      echo "Invalid option: -$OPTARG" >&2
      wasError=true
      ;;
    : )
      case $OPTARG in
        g )
          echo "Option -g requires a GPU number as an argument." >&2
          ;;
        * )
          echo "Option -$OPTARG requires an argument." >&2
          ;;
      esac
      wasError=true
      ;;
  esac
done

if [ "$wasError" = true ]; then
    usage
    exit 1;
fi


# Pass callwlated arguments through the command line processing steps again before
# the exelwtion. Otherwise -l "file" will result in a creation of a file with double
# quotes in its name.
if [ "$longArgIsSet" = true ]; then
    eval "$diagExelwtable longdisplay logfilename=griddiag.display.log $diagArgs"  || exit $?
    eval "$diagExelwtable long logfilename=griddiag.long.log $diagArgs" || exit $?
else
    eval "$diagExelwtable short logfilename=griddiag.short.log $diagArgs" || exit $?
fi

