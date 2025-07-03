#!/bin/sh

diagExelwtable="/usr/share/lwpu/diagnostic/mods"

myName=$(basename $0)

gpuArgIsSet=false
logPathArgIsSet=false
wasError=false

wasgError=false # prevent multiple g option error messages
waslError=false # prevent multiple l option error messages

usage()
{
    cat <<EOF
usage: $myName [options]
OPTIONS
    -g GPUNUMBER
        Execute the diagnostic on the GPUNUMBER device. If this option is
        omitted, then on the first available device.
    -l PATH
        Write the log file to the specified path and file name. If this option
        is omitted, the log file name will be lwpu-diagnostic.log and it will
        be placed in the current directory.
    -L
        Write the log file only on a failure.
    -n
        Do not write the log file.
    -s
        Suppress output to stdout or stderr.
    -h
        Print this help message.
EXAMPLES
    Execute the diagnostic on the first available GPU.
        lwpu-diagnostic.sh
    Execute the diagnostic on GPU 0.
        lwpu-diagnostic.sh -g 0
    Execute the diagnostic on GPU 1, do not write the log file, do not output to
    stdout or stderr.
        lwpu-diagnostic.sh -sng 1
EOF
}

while getopts ":g:l:Lshn" Option
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
      if [ "$logPathArgIsSet" = true -a "$waslError" = false ]; then
          echo "Option -l can be specified only once." >&2
          wasError=true
          waslError=true
      fi
      diagArgs="$diagArgs logfilename="'"'"$OPTARG"'"'""
      logPathArgIsSet=true
      ;;
    L )
      diagArgs="$diagArgs logonfail"
      ;;
    s )
      diagArgs="$diagArgs silent"
      ;;
    n )
      diagArgs="$diagArgs nolog"
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
        l )
          echo "Option -l requires a path to the log file as an argument." >&2
          ;;
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
eval "$diagExelwtable $diagArgs"

