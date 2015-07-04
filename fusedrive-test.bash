#!/usr/bin/env bash
# 
# File:   fuse-drive-test.bash
# Author: me
#
# Created on May 30, 2015, 9:14:39 PM
#

set -u

print_usage () {
    fuselog "Usage:"
    fuselog "    $0 logfile executable mountpoint"
    fuselog "    $0 --no-mount logfile executable mountpoint"
    fuselog "    $0 --valgrind <outfile> [valgrind-options] -- logfile executable mountpoint"

    fuselog "    Example: $0 fuselog ./fuse-drive ~/mytemp"
    fuselog
    fuselog "Options include:"
    fuselog "    --no-mount             Skip any mounting and unmounting steps."
    fuselog "    --valgrind <outfile>   Run valgrind, send output to the"
    fuselog "                           specified file. (Does not work with"
    fuselog "                           --no-mount)"
}

fuse_unmount() {
    if [ "$NOMOUNT" -eq 0 ]; then
        fuselog -n "Unmounting '$MOUNTPATH'..."
        cd $ORIGINAL_WORKING_DIR
        fusermount -u $MOUNTPATH 2> /dev/null
        while [ $? -ne 0 ]; do
            sleep 1
            fuselog -n .
            fusermount -u $MOUNTPATH 2> /dev/null
        done
        fuselog " Ok"
    else
        fuselog Skipping unmount step
    fi
}

fuse_mount() {
    if [ "$NOMOUNT" -eq 0 ]; then
        fuselog Starting fusedrive
        $VALGRIND $VALGRIND_OPTS $EXE "$MOUNTPATH" $MOUNTOPTIONS > /dev/stderr 2>> "$VALGRIND_REDIR" &
        FDPID=$!

        fuselog -n "Waiting for mount..."
        until mount | grep -qF "on $MOUNTPATH"; do
            sleep 1
            fuselog -n .
            if ! kill -0 $FDPID > /dev/null 2>&1; then
                fuselog " Failed."
                exit 1
            fi
        done
        fuselog " Ok"
    else
        fuselog Skipping mount step
    fi

    fuselog checking statfs
    fuselog -n "Looking for '$MOUNTPATH' in output of 'df'... "
    if df | grep -qF "$MOUNTPATH"; then
        fuselog Ok
    else
        fuselog Failed.
        clean_exit 1
    fi
}

clean_exit() {
    fuselog
    fuse_unmount
    exit $1
}



fuselog() {
    # Don't just use tee because the trailing newline isn't optional. Whether or
    # not we can suppress the newline in the logfile, we sometimes want to 
    # suppress it on stdout.
    echo "$@"
    echo "$@" >> $LOGFILE
}

make_name() {
    # Generates a random-looking (not actually random, or even pseudo-random) 
    # name from the current time
    declare EXT
    if [ "$#" -ge 1 ] && [ -n "$1" ]; then
        EXT=".$1"
    else
        EXT=""
    fi
    GENERATED_NAME="$(date +%s | sha256sum | base64 | head -c 16 | tail -c 8)$EXT"
}

make_timestamp() {
    # $1 should be (positive or negative) number of seconds to add to the 
    # current time.
    GENERATED_TIMESTAMP=$(date -d "@$(($(date +%s) + $1))" "$TIMESTAMP_FMT")
}

get_atime() {
    # $1 should be the filename.
    ATIME=$(ls -lu --time-style=$TIMESTAMP_FMT "$1" | awk '{printf $6}')
}

get_mtime() {
    # $1 should be the filename.
    # This could also be done with date -r, but there doesn't seem to be an
    # option for access time. This way, get_mtime() and get_atime() are more
    # symmetric.
    MTIME=$(ls -l --time-style=$TIMESTAMP_FMT "$1" | awk '{printf $6}')
}

set_atime() {
    # $1 is the filename
    # $2 is the timestamp to set
    touch -cat "$2" "$1" 2> /dev/null
    return $?
}

set_mtime() {
    # $1 is the filename
    # $2 is the timestamp to set
    touch -cmt "$2" "$1" 2> /dev/null
    return $?
}

get_filesize() {
    # $1 is the filename
    FILESIZE=$(stat -c %s "$1")
}

test_truncate() {
    # $1 is the filename
    # $2 is the desired size
    fuselog -n "Truncating '$1' to $2 bytes... "
    if ! truncate -s $2 "$1" > /dev/null 2>&1; then
        fuselog "truncate command indicated failure"
        clean_exit 1
    fi
    get_filesize "$1"
    if [ "$FILESIZE" -ne "$2" ]; then
        fuselog "Failed. Expected '$2', saw '$FILESIZE'."
        clean_exit 1
    fi
    fuselog Ok
    return 0
}

test_clobber() {
    # $1 is the filename
    # $2 is the number of bytes to write
    # $3 is the character to fill with
    fuselog -n "Writing $2 '$3's to '$1'... "
    if ! head -c $2 /dev/zero | tr '\000' $3 > "$1"; then
        fuselog "Command indicated an error"
        clean_exit 1
    fi
    fuselog Ok
    fuselog -n "Testing file length... "
    get_filesize "$1"
    if [ "$FILESIZE" -ne $2 ]; then
        fuselog "Incorrect size. Expected $2, saw '$FILESIZE'."
        clean_exit 1
    fi
    fuselog ok
    fuselog -n "Testing file contents... "
    if ! grep -qE "$3"'{'"$2"'}' "$1"; then
        fuselog "Failed. Didn't find expected number of '$3' characters."
        clean_exit 1
    fi
    if grep -q '[^'"$3"']' "$1"; then
        fuselog "Failed. Unexpected character(s) found in file."
        clean_exit 1
    fi
    fuselog ok
}

test_append() {
    # $1 is the filename
    # $2 is the number of bytes to write
    # $3 is the character to fill with
    declare ORIG_SIZE

    fuselog -n "Getting current length of '$1'... "
    get_filesize "$1"
    ORIG_SIZE="$FILESIZE"
    fuselog Done
    fuselog -n "Appending $2 '$3's to '$1'... "
    if ! head -c $2 /dev/zero | tr '\000' $3 >> "$1"; then
        fuselog "Command indicated an error"
        clean_exit 1
    fi
    fuselog Ok
    fuselog -n "Testing file length... "
    get_filesize "$1"
    if [ $(($FILESIZE - $ORIG_SIZE)) -ne $2 ]; then
        fuselog "Incorrect size. Expected $(($2 + $ORIG_SIZE)), saw '$FILESIZE'."
        clean_exit 1
    fi
    fuselog ok
    fuselog -n "Testing file contents... "
    if ! tail -c "$2" "$1" | grep -qE "$3"'{'"$2"'}'; then
        fuselog "Failed. Didn't find expected number of '$3' characters."
        clean_exit 1
    fi
    if tail -c "$2" "$1" | grep -q '[^'"$3"']'; then
        fuselog "Failed. Unexpected character(s) found in file."
        clean_exit 1
    fi
    fuselog ok
}




MOUNTOPTIONS='-f -s'
TIMESTAMP_FMT='+%Y%m%d%H%M.%S'
LOGFILE=/dev/null
ORIGINAL_WORKING_DIR=$(pwd)
VALGRIND=""
VALGRIND_REDIR="/dev/stderr"
VALGRIND_OPTS=""
NOMOUNT=0

# Seed RNG
RANDOM=$(($(date +%N) % $$ + $BASHPID))

# Test number of parameters
if [ $# -lt 3 ]; then
    fuselog Invalid arguments
    fuselog
    print_usage
    exit 1
fi

while [ "$1" = --valgrind ] || [ "$1" = --no-mount ]; do
    if [ "$1" = --valgrind ]; then
        VALGRIND=valgrind
        VALGRIND_REDIR="$2"
        truncate -s 0 "$VALGRIND_REDIR"
        shift 2
        while [ "$1" != "--" ]; do
            VALGRIND_OPTS="$VALGRIND_OPTS $1"
            shift
        done
        shift
    elif [ "$1" = --no-mount ]; then
        NOMOUNT=1
        shift
    fi
    if [ $# -lt 3 ]; then
        fuselog Invalid arguments
        fuselog
        print_usage
        exit 1
    fi
done
    
LOGFILEOPTION="$1"
EXE="$2"
MOUNTPATHGIVEN="$3"


# $LOGFILE should not exist or should be a writable file
if [ -a "$LOGFILEOPTION" ] && ! [ -w "$LOGFILEOPTION" ]; then
    echo "'$LOGFILEOPTION' exists and cannot be written to"
    echo
    print_usage
    echo
    echo Please make sure logfile either does not exist or can be written
    exit 1
fi
LOGFILE=$(realpath "$LOGFILEOPTION")
unset LOGFILEOPTION

# $EXE should be an executable file
if ! [ -x "$EXE" ]; then
    fuselog "'$EXE' is not an executable file"
    fuselog
    print_usage
    exit 1
fi

# $MOUNTPOINT should be an empty directory, unless the --no-mount option was
# specified
if ! [ -d "$MOUNTPATHGIVEN" ] || ([ "$NOMOUNT" -eq 0 ] && [ $(ls -A1 "$MOUNTPATHGIVEN" | wc -l) -ne 0 ]); then
    fuselog "'$MOUNTPATHGIVEN'" is not a valid empty directory
    fuselog
    print_usage
    exit 1
fi
MOUNTPATH=$(realpath "$MOUNTPATHGIVEN")
unset MOUNTPATHGIVEN

# Separate the new log from any existing information in the logfile
if ! echo -e '\n========================================\n' >> "$LOGFILE"; then
    echo Could not append to logfile "'$LOGFILE'"
    LOGFILE=/dev/null
    exit 1
fi

fuselog $0 run $(date -u "+%F %T") UTC
fuselog

if [ "$NOMOUNT" -eq 0 ]; then
    fuselog -n "Checking mountpoint... "
    if mount | grep -qF "on $MOUNTPATH"; then
        fuselog "Mountpoint in use."
        exit 1
    else
        fuselog Ok
    fi
fi

# Startup

fuselog
fuse_mount


# Check statfs (using df)

fuselog -n "Checking output of 'df \"$MOUNTPATH\"'... "
DFRESULT=$(df --output=size,pcent,target "$MOUNTPATH" | tail -n 1)
if [ $(echo $DFRESULT | awk '{printf $1}') -eq 0 ]; then
    fuselog Size is 0, expected nonzero.
    clean_exit 1
fi
if [ $(echo $DFRESULT | awk '{printf $2}') = "-" ]; then
    fuselog 'Use% unknown (-), expected numeric percent'
    clean_exit 1
fi
if [ $(echo $DFRESULT | awk '{printf $3}') != "$MOUNTPATH" ]; then
    fuselog "Wrong mountpoint. Expected '$MOUNTPATH', saw $(echo $DFRESULT | awk '{printf $3}')"
    clean_exit 1
fi
fuselog Ok
unset DFRESULT

# Get ready to change directory to the mountpoint

fuselog
fuselog -n "Basic ls check (just checking returned value)... "
if ! ls "$MOUNTPATH" > /dev/null 2>&1; then
    fuselog Returned value $?, expected 0.
    clean_exit 1
fi
fuselog Ok

fuselog -n "Changing working directory... "
if ! cd "$MOUNTPATH"; then
    fuselog Failed.
    clean_exit 1
fi
fuselog Ok

# Test file and directory creation (create() and mkdir())

fuselog
fuselog "Checking file and directory creation"

fuselog "Directory creation:"
make_name
while [ -e "$GENERATED_NAME" ]; do
    make_name
done
NEWDIRNAME="$GENERATED_NAME"
unset GENERATED_NAME

fuselog -n "Creating new directory '$NEWDIRNAME' with mkdir... "
if ! mkdir "$NEWDIRNAME"; then
    fuselog "mkdir command returned nonzero error status'"
    clean_exit 1
fi
fuselog "mkdir command indicated success."
fuselog -n "Making sure '$NEWDIRNAME' exists... "
if ! [ -d "$NEWDIRNAME" ]; then
    fuselog "Failed with test ([)."
    clean_exit 1
fi
if ! ls -d "$NEWDIRNAME" > /dev/null 2>&1; then
    fuselog "Passed with test ([), but failed with ls."
    fuselog "There may be a problem with readdir() or getattr()."
    clean_exit 1
fi
fuselog Ok

fuselog "File creation in root directory:"
make_name txt
while [ -e "$GENERATED_NAME" ]; do
    make_name txt
done
ROOTFILENAME="$GENERATED_NAME"
unset GENERATED_NAME

fuselog -n "Creating new file '$ROOTFILENAME' with touch... "
if ! touch "$ROOTFILENAME"; then
    fuselog "touch command returned nonzero error status'"
    clean_exit 1
fi
fuselog "touch command indicated success."
fuselog -n "Making sure '$ROOTFILENAME' exists... "
if ! [ -f "$ROOTFILENAME" ]; then
    fuselog "Failed with test ([)."
    clean_exit 1
fi
if ! ls "$ROOTFILENAME" > /dev/null 2>&1; then
    fuselog "Passed with test ([), but failed with ls."
    fuselog "There may be a problem with readdir() or getattr()."
    clean_exit 1
fi
fuselog Ok
fuselog -n "Making sure '$NEWDIRNAME/$ROOTFILENAME' doesn't exist... "
if [ -e "${NEWDIRNAME}/${ROOTFILENAME}" ]; then
    fuselog "Failed with test ([), file exists."
    clean_exit 1
fi
if ls "${NEWDIRNAME}/${ROOTFILENAME}"  > /dev/null 2>&1; then
    fuselog "Passed with test ([), but ls says it exists."
    clean_exit 1
fi
fuselog Ok

fuselog "File creation in subdirectory:"
make_name txt
while [ -e "${NEWDIRNAME}/${GENERATED_NAME}" ]; do
    make_name txt
done
SUBFILENAME="$GENERATED_NAME"
unset GENERATED_NAME

fuselog -n "Creating new file '${NEWDIRNAME}/${SUBFILENAME}' with touch... "
if ! touch "${NEWDIRNAME}/${SUBFILENAME}"; then
    fuselog "touch command returned nonzero error status'"
    clean_exit 1
fi
fuselog "touch command indicated success."
fuselog -n "Making sure '${NEWDIRNAME}/${SUBFILENAME}' exists... "
if ! [ -f "${NEWDIRNAME}/${SUBFILENAME}" ]; then
    fuselog "Failed with test ([)."
    clean_exit 1
fi
if ! ls "${NEWDIRNAME}/${SUBFILENAME}" > /dev/null 2>&1; then
    fuselog "Passed with test ([), but failed with ls."
    fuselog "There may be a problem with readdir() or getattr()."
    clean_exit 1
fi
fuselog Ok
fuselog -n "Making sure '$SUBFILENAME' doesn't exist... "
if [ -e "$SUBFILENAME" ]; then
    fuselog "Failed with test ([), file exists."
    clean_exit 1
fi
if ls "$SUBFILENAME"  > /dev/null 2>&1; then
    fuselog "Passed with test ([), but ls says it exists."
    clean_exit 1
fi
fuselog Ok

# Check utimens() (using touch)

fuselog
fuselog Setting access and modification times

fuselog -n "Reading original access time for '${NEWDIRNAME}/${SUBFILENAME}'... "
get_atime "${NEWDIRNAME}/${SUBFILENAME}"; OLDATIME="$ATIME"
unset ATIME
if [ -z "$OLDATIME" ]; then
    fuselog "Failed."
    clean_exit 1
fi
unset OLDATIME
fuselog Done.
fuselog -n "Reading original modification time for '${NEWDIRNAME}/${SUBFILENAME}'... "
get_mtime "${NEWDIRNAME}/${SUBFILENAME}"; OLDMTIME="$MTIME"
unset MTIME
if [ -z "$OLDMTIME" ]; then
    fuselog "Failed."
    clean_exit 1
fi
fuselog Done.

# $RANDOM is 16-bit unsigned, up to 32767. Multiply by 10000 to get up to about
# 10 years' worth of seconds, and multiply by -1 to subtract from the current
# time.
make_timestamp -${RANDOM}0000; PAST_TIMESTAMP="$GENERATED_TIMESTAMP"
unset GENERATED_TIMESTAMP
fuselog "Access times:"
fuselog -n "Setting access time for '${NEWDIRNAME}/${SUBFILENAME}' to '$PAST_TIMESTAMP'... "
if ! set_atime "${NEWDIRNAME}/${SUBFILENAME}" "$PAST_TIMESTAMP"; then
    fuselog "touch command indicated error."
    clean_exit 1
fi
fuselog "touch command indicated success."
fuselog -n "Verifying access time... "
get_atime "${NEWDIRNAME}/${SUBFILENAME}"; NEWATIME="$ATIME"
unset ATIME
if [ -z "$NEWATIME" ]; then
    fuselog "Could not retrieve access time."
    clean_exit 1
fi
if [ "$NEWATIME" != "$PAST_TIMESTAMP" ]; then
    fuselog "Failed. Expected '$PAST_TIMESTAMP', saw '$NEWATIME'."
    clean_exit 1
fi
unset PAST_TIMESTAMP
fuselog Ok
fuselog -n "Verifying modification time is unchanged... "
get_mtime "${NEWDIRNAME}/${SUBFILENAME}"; NEWMTIME="$MTIME"
unset MTIME
if [ -z "$NEWMTIME" ]; then
    fuselog "Could not retrieve modification time."
    clean_exit 1
fi
if [ "$NEWMTIME" != "$OLDMTIME" ]; then
    fuselog "Failed. Expected '$OLDMTIME', saw '$NEWMTIME'."
    clean_exit 1
fi
unset NEWMTIME
unset OLDMTIME
fuselog Ok

# Google Drive doesn't seem to support future access times.
## Use a positive number this time, to make a future timestamp
#make_timestamp ${RANDOM}0000; FUTURE_TIMESTAMP="$GENERATED_TIMESTAMP"
#fuselog -n "Setting access time for '${NEWDIRNAME}/${SUBFILENAME}' to '$FUTURE_TIMESTAMP'... "
#if ! set_atime "${NEWDIRNAME}/${SUBFILENAME}" "$FUTURE_TIMESTAMP"; then
#    fuselog "touch command indicated error.
#    clean_exit 1
#fi
#fuselog "touch command indicated success."
#fuselog -n "Verifying access time... "
#get_atime "${NEWDIRNAME}/${SUBFILENAME}"; NEWATIME="$ATIME"
#if [ -z "$NEWATIME" ]; then
#    fuselog "Could not retrieve access time."
#    clean_exit 1
#fi
#if [ "$NEWATIME" != "$FUTURE_TIMESTAMP" ]; then
#    fuselog "Failed. Expected '$FUTURE_TIMESTAMP', saw '$NEWATIME'."
#    clean_exit 1
#fi
#fuselog Ok
#fuselog -n "Verifying modification time is unchanged... "
#get_mtime "${NEWDIRNAME}/${SUBFILENAME}"; NEWMTIME="$MTIME"
#if [ -z "$NEWMTIME" ]; then
#    fuselog "Could not retrieve access time."
#    clean_exit 1
#fi
#if [ "$NEWMTIME" != "$OLDMTIME" ]; then
#    fuselog "Failed. Expected '$OLDMTIME', saw '$NEWMTIME'."
#    clean_exit 1
#fi

# Negative number for a time in the past
make_timestamp -${RANDOM}0000; PAST_TIMESTAMP="$GENERATED_TIMESTAMP"
unset GENERATED_TIMESTAMP
fuselog "Modification times:"
fuselog -n "Setting modification time for '${NEWDIRNAME}/${SUBFILENAME}' to '$PAST_TIMESTAMP'... "
if ! set_mtime "${NEWDIRNAME}/${SUBFILENAME}" "$PAST_TIMESTAMP"; then
    fuselog "touch command indicated error."
    clean_exit 1
fi
fuselog "touch command indicated success."
fuselog -n "Verifying modification time... "
get_mtime "${NEWDIRNAME}/${SUBFILENAME}"; NEWMTIME="$MTIME"
unset MTIME
if [ -z "$NEWMTIME" ]; then
    fuselog "Could not retrieve modification time."
    clean_exit 1
fi
if [ "$NEWMTIME" != "$PAST_TIMESTAMP" ]; then
    fuselog "Failed. Expected '$PAST_TIMESTAMP', saw '$NEWMTIME'."
    clean_exit 1
fi
unset NEWMTIME
unset PAST_TIMESTAMP
fuselog Ok
fuselog -n "Verifying access time is unchanged... "
OLDATIME="$NEWATIME"
get_atime "${NEWDIRNAME}/${SUBFILENAME}"; NEWATIME="$ATIME"
unset ATIME
if [ -z "$NEWATIME" ]; then
    fuselog "Could not retrieve access time."
    clean_exit 1
fi
if [ "$NEWATIME" != "$OLDATIME" ]; then
    fuselog "Failed. Expected '$OLDATIME', saw '$NEWATIME'."
    clean_exit 1
fi
unset NEWATIME
fuselog Ok

# Use a positive number this time, to make a future timestamp
make_timestamp ${RANDOM}0000; FUTURE_TIMESTAMP="$GENERATED_TIMESTAMP"
unset GENERATED_TIMESTAMP
fuselog -n "Setting modification time for '${NEWDIRNAME}/${SUBFILENAME}' to '$FUTURE_TIMESTAMP'... "
if ! set_mtime "${NEWDIRNAME}/${SUBFILENAME}" "$FUTURE_TIMESTAMP"; then
    fuselog "touch command indicated error."
    clean_exit 1
fi
fuselog "touch command indicated success."
fuselog -n "Verifying modification time... "
get_mtime "${NEWDIRNAME}/${SUBFILENAME}"; NEWMTIME="$MTIME"
unset MTIME
if [ -z "$NEWMTIME" ]; then
    fuselog "Could not retrieve modification time."
    clean_exit 1
fi
if [ "$NEWMTIME" != "$FUTURE_TIMESTAMP" ]; then
    fuselog "Failed. Expected '$FUTURE_TIMESTAMP', saw '$NEWMTIME'."
    clean_exit 1
fi
unset NEWMTIME
fuselog Ok
fuselog -n "Verifying access time is unchanged... "
get_atime "${NEWDIRNAME}/${SUBFILENAME}"; NEWATIME="$ATIME"
unset ATIME
if [ -z "$NEWATIME" ]; then
    fuselog "Could not retrieve access time."
    clean_exit 1
fi
if [ "$NEWATIME" != "$OLDATIME" ]; then
    fuselog "Failed. Expected '$OLDATIME', saw '$NEWATIME'."
    clean_exit 1
fi
unset NEWATIME
unset OLDATIME
fuselog Ok

# Check write() (which also requires open(), release(), fsync(), truncate()/ftruncate())

fuselog
fuselog Reading, writing and truncation
fuselog Truncation:
test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 0
test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 100
test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 1000
test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 50
test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 50
# test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 24681357
test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 0

fuselog "Writing (Overwriting) and Reading:"
test_clobber "${NEWDIRNAME}/${SUBFILENAME}" 1024 F
test_clobber "${NEWDIRNAME}/${SUBFILENAME}" 75 u
test_clobber "${NEWDIRNAME}/${SUBFILENAME}" 100 s
test_clobber "${NEWDIRNAME}/${SUBFILENAME}" 50 e

# Make sure the total size doesn't get too large here, since we'll read the
# entire file contents into variables later.
fuselog "Appending and Reading:"
test_append "${NEWDIRNAME}/${SUBFILENAME}" 3 D
test_append "${NEWDIRNAME}/${SUBFILENAME}" 50 r
test_append "${NEWDIRNAME}/${SUBFILENAME}" 100 i
test_append "${NEWDIRNAME}/${SUBFILENAME}" 10 v
test_append "${NEWDIRNAME}/${SUBFILENAME}" 200 e

# Test persistence through unmount/remount
fuselog Testing persistence
fuselog -n "Capturing current contents of '${NEWDIRNAME}/${SUBFILENAME}'... "
if ! FILE_CONTENTS=$(< "${NEWDIRNAME}/${SUBFILENAME}"); then
    fuselog "Failed."
    clean_exit 1
fi
fuselog "Done."
fuse_unmount
fuse_mount
fuselog -n "Changing working directory back to '$MOUNTPATH'... "
if ! cd "$MOUNTPATH"; then
    fuselog Failed.
    clean_exit 1
fi
fuselog Ok
fuselog -n "Confirming '$ROOTFILENAME' still exists... "
if ! [ -e "$ROOTFILENAME" ]; then
    fuselog Failed.
    clean_exit 1
fi
fuselog Ok
fuselog -n "Confirming '$NEWDIRNAME' still exists and is a directory... "
if ! [ -d "$NEWDIRNAME" ]; then
    fuselog Failed.
    clean_exit 1
fi
fuselog Ok
fuselog -n "Confirming '${NEWDIRNAME}/${SUBFILENAME}' still exists... "
if ! [ -e "${NEWDIRNAME}/${SUBFILENAME}" ]; then
    fuselog Failed.
    clean_exit 1
fi
fuselog Ok
fuselog -n "Comparing contents of '${NEWDIRNAME}/${SUBFILENAME}' to old contents... "
if [ "$FILE_CONTENTS" != "$(< ${NEWDIRNAME}/${SUBFILENAME})" ]; then
    fuselog Failed.
    clean_exit 1
fi
unset FILE_CONTENTS
fuselog Ok

# Check unlink() and rmdir() (using rm and rmdir)

fuselog
fuselog Deleting files and directories
fuselog Checking for proper failure of rmdir on non-empty directories:
fuselog -n "Trying to rmdir '$NEWDIRNAME', should NOT succeed... "
if rmdir "$NEWDIRNAME" 2> /dev/null; then
    fuselog "Removed non-empty directory, incorrect behavior."
    clean_exit 1
fi
fuselog "Ok, behaved as expected."

fuselog Deleting files:
fuselog -n "rm \"$ROOTFILENAME\"... "
if ! rm "$ROOTFILENAME" 2> /dev/null; then
    fuselog "rm command indicated error."
    clean_exit 1
fi
if [ -e "$ROOTFILENAME" ]; then
    fuselog "Failed. rm command indicated success, but file still exists."
    clean_exit 1
fi
fuselog ok
fuselog -n "rm \"${NEWDIRNAME}/${SUBFILENAME}\"... "
if ! rm "${NEWDIRNAME}/${SUBFILENAME}" 2> /dev/null; then
    fuselog "rm command indicated error."
    clean_exit 1
fi
if [ -e "${NEWDIRNAME}/${SUBFILENAME}" ]; then
    fuselog "Failed. rm command indicated success, but file still exists."
    clean_exit 1
fi
fuselog ok

fuselog Removing directory:
fuselog -n "rmdir \"$NEWDIRNAME\" (should succeed this time)... "
rmdir "$NEWDIRNAME" 2> /dev/null
RMSUCCESS=$?
RMTRIES=1
while [ "$RMSUCCESS" -ne 0 ]; do
    fuselog "rmdir command indicated error."
    if [ "$RMTRIES" -ge 5 ]; then
        fuselog "Too many attempts. Giving up"
        clean_exit 1
    else
        fuselog -n "Waiting and retrying... "
        sleep 3
        rmdir "$NEWDIRNAME" 2> /dev/null
        RMSUCCESS=$?
        ((RMTRIES++))
    fi
done
if [ -d "$NEWDIRNAME" ]; then
    fuselog "Failed. rmdir command indicated success, but file still exists."
    clean_exit 1
fi
fuselog ok

fuselog
fuselog Hard links and symbolic links
fuselog Creating hard link:
make_name
while [ -e "$GENERATED_NAME" ]; do
    make_name
done
DIRONE="$GENERATED_NAME"
make_name; DIRONE="$GENERATED_NAME"
make_name
while [ -e "$GENERATED_NAME" ]; do
    make_name
done
DIRTWO="$GENERATED_NAME"
make_name
while [ -e "$GENERATED_NAME" ]; do
    make_name
done
FILENAME="$GENERATED_NAME"
unset GENERATED_NAME
fuselog -n "Creating directory '$DIRONE'... "
if ! mkdir "$DIRONE"; then
    fuselog "mkdir command returned nonzero error status'"
    clean_exit 1
fi
fuselog Ok
fuselog -n "Creating directory '$DIRTWO'... "
if ! mkdir "$DIRTWO"; then
    fuselog "mkdir command returned nonzero error status'"
    clean_exit 1
fi
fuselog Ok
fuselog -n "Creating file '$DIRONE/$FILENAME'... "
if ! touch "$DIRONE/$FILENAME"; then
    fuselog "touch command returned nonzero error status'"
    clean_exit 1
fi
fuselog Ok
fuselog -n "Setting hard link from '$DIRTWO/$FILENAME' to '$DIRONE/$FILENAME'... "
if ! ln "$DIRONE/$FILENAME" "$DIRTWO/$FILENAME" 2> /dev/null; then
    fuselog "ln command returned nonzero error status"
    clean_exit 1
fi
fuselog -n "Verifying '$DIRONE/$FILENAME' still exists... "
if ! [ -e "$DIRONE/$FILENAME" ]; then
    fuselog "Failed. Does not exist."
    clean_exit 1
fi
fuselog Ok
fuselog -n "Verifying '$DIRTWO/$FILENAME' also exists... "
if ! [ -e "$DIRTWO/$FILENAME" ]; then
    fuselog "Failed. Does not exist."
    clean_exit 1
fi
fuselog Ok
fuselog -n "Setting hard link from '$FILENAME' to '$DIRONE/$FILENAME'... "
if ! ln "$DIRONE/$FILENAME" "$FILENAME" 2> /dev/null; then
    fuselog "ln command returned nonzero error status"
    clean_exit 1
fi
fuselog Ok
fuselog -n "Verifying '$FILENAME' exists... "
if ! [ -e "$FILENAME" ]; then
    fuselog "Failed. Does not exist."
    clean_exit 1
fi
fuselog Ok


fuselog
fuselog 'DONE!'
fuselog All tests successful
clean_exit 0