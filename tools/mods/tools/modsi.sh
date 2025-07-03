#!/bin/sh

`dirname $0`/mods -s -i mods.js -e 'gputest_main();' gputest.js -engr -notest "$@"
