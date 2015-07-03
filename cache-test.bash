#!/bin/bash
# 
# File:   cache-test.bash
# Author: me
#
# Created on Jul 3, 2015, 12:39:38 PM
#

print_usage () {
echo Usage: 
    echo "     $0 <executable> <commandfile> <outputfile>"
    echo "     $0 <PID> <commandfile> [outputfile]"
    echo
    echo If the first argument is numeric, it is treated as the process id of
    echo an already running executable. In this case, outputfile is ignored if
    echo specified. If the first argument is non-numeric, the specified program
    echo is started.
}

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
    print_usage
    exit
fi

if [ "$1" -eq "$1" ] 2> /dev/null; then
    # Argument is numeric
    PID="$1"
    if ! kill -0 "$PID" 2> /dev/null; then
        echo Cannot send signals to process ID "$PID"
        exit
    fi
else
    "$1" "$2" "$3" &
    PID=$!
fi

CMDFILE="$2"
touch "$CMDFILE" 2> /dev/null
if ! [ -w "$CMDFILE" ]; then
    echo Could not open "$CMDFILE" for writing
    exit
fi

echo
echo Enter commands one at a time, or \"exit\" to quit
KEEPGOING=1
while [ "$KEEPGOING" -eq 1 ]; do
    echo -n "> "
    read
    if [ "$REPLY" != "exit" ]; then
        echo "$REPLY" > "$CMDFILE"
        kill -USR1 "$PID"
    else
        echo Quitting
        KEEPGOING=0
    fi
done
kill "$PID"