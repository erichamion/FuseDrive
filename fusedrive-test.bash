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






clean_exit() {
    fuselog
    fuse_unmount
    fuselog
    fuselog "Finished with $FAILURE_COUNT errors ($FATAL_FAILURE_COUNT fatal)."
    fuselog "Errors encountered (if any):"
    fuselog "$FAILURE_MSGS"
    exit $1
}

tally_failure() {
    # $1 is 0 for non-fatal error, non-zero for fatal
    # $2 is the name of the failed test
    # $3 is the message returned by the test
    # Results are accumulated in the global variables FAILURE_COUNT, FATAL_FAILURE_COUNT and FAILURE_MSGS
    ((FAILURE_COUNT++))
    if [ "$1" -eq 0 ]; then
        FAILURE_MSGS="${FAILURE_MSGS}Non-"
    else
        FATAL_FAILURE_COUNT=1
    fi
    FAILURE_MSGS="${FAILURE_MSGS}Fatal failure on ${2}: $3\n"
}

run_test() {
    # $1 is the number of tries allowed
    # $2 is the number of seconds to wait between tries
    # $3 determines whether failure is fatal (0 for non-fatal, non-zero for fatal)
    # $4 is the name of the test to run
    # Any additional parameters are passed on to the test
    # The test should fill in the TEST_RESULT variable for logging.
    unset TEST_RESULT
    local attempts="$1"
    local wait_time="$2"
    local is_fatal="$3"
    shift 3
    if "$@"; then
        return 0
    else
        ((attempts--))
        if [ "$attempts" -ge 0 ]; then
            fuselog -n "$TEST_RESULT. Retrying... "
            sleep "$wait_time"
            run_test "$attempts" "$wait_time" "$is_fatal" "$@"
            return $?
        else
            tally_failure "$is_fatal" "$1" "$TEST_RESULT"
            if [ "$is_fatal" -eq 0 ]; then
                return 1
            else
                fuselog "Unrecoverable failure."
                clean_exit 1
            fi
        fi
    fi
}








test_df() {
    # Check statfs (using df)
    local dfresult=$(df --output=size,pcent,target "$MOUNTPATH" | tail -n 1)
    if [ $(echo $dfresult | awk '{printf $1}') -eq 0 ]; then
        TEST_RESULT="Size is 0, expected nonzero."
        return 1
    fi
    if [ $(echo $dfresult | awk '{printf $2}') = "-" ]; then
        TEST_RESULT='Use% unknown (-), expected numeric percent'
        return 1
    fi
    if [ $(echo $dfresult | awk '{printf $3}') != "$MOUNTPATH" ]; then
        TEST_RESULT="Wrong mountpoint. Expected '$MOUNTPATH', saw $(echo $DFRESULT | awk '{printf $3}')"
        return 1
    fi
    TEST_RESULT=""
}

test_ls_basic() {
    if ! ls "$MOUNTPATH" > /dev/null 2>&1; then
        TEST_RESULT="Returned value $?, expected 0."
        return 1
    fi
    TEST_RESULT=""
    return 0
}

test_cd() {
    if ! cd "$MOUNTPATH"; then
        TEST_RESULT="Failed to change directory to '$MOUNTPATH'."
        return 1
    fi
    TEST_RESULT=""
    return 0
}

test_mkdir() {
    # $1 is the name of the directory to create. If empty or unspecified, a name
    # will be generated. Upon success, the created directory's name is returned
    # in the TEST_RESULT variable.
    local newdirname="${1:-}"
    if [ "$newdirname" = "" ]; then
        make_name
        while [ -e "$GENERATED_NAME" ]; do
            make_name
        done
        newdirname="$GENERATED_NAME"
        unset GENERATED_NAME
    fi

    fuselog -n "Creating new directory '$newdirname' with mkdir... "
    if ! mkdir "$newdirname"; then
        TEST_RESULT="mkdir command returned nonzero error status"
        return 1
    fi
    fuselog "mkdir command indicated success."
    fuselog -n "Making sure '$newdirname' exists... "
    if ! [ -d "$newdirname" ]; then
        TEST_RESULT="The test ([) command shows '$newdirname' doesn't exist."
        return 1
    fi
    if ! ls -d "$newdirname" > /dev/null 2>&1; then
        TEST_RESULT="The test ([) command succeeded, ls shows '$newdirname' doesn't exist."
        return 1
    fi
    TEST_RESULT="$newdirname"
    return 0
}

test_create_file() {
    # $1 is a directory in which to create the file. "/", empty string, or missing
    #    will be treated as the current directory.
    # $2 is the filename to create. Empty or missing will result in a generated
    #    name with ".txt" extension.
    # Upon success, the filename (if in the current directory) or the
    #    directory/filename is returned in the TEST_RESULT variable.
    local mydir="${1:-/}"
    if [ "$mydir" = "" ] || [ "$mydir" = "/" ]; then
        mydir=""
    else
        mydir="${mydir}/"
    fi
    local myfile="${2:-}"
    if [ "$myfile" = "" ]; then
        make_name txt
        while [ -e "${mydir}$GENERATED_NAME" ]; do
            make_name txt
        done
        myfile="$GENERATED_NAME"
        unset GENERATED_NAME
    fi
    
    local fullname="${mydir}${myfile}"

    fuselog -n "Creating new file '$fullname' with touch... "
    if ! touch "$fullname"; then
        TEST_RESULT="touch \"$fullname\" failed'"
        return 1
    fi
    fuselog "touch command indicated success."
    fuselog -n "Making sure '$fullname' exists... "
    if ! [ -f "$fullname" ]; then
        TEST_RESULT="The test ([) command shows '$fullname' doesn't exist."
        return 1
    fi
    if ! ls "$fullname" > /dev/null 2>&1; then
        TEST_RESULT="The ls command shows '$fullname' doesn't exist."
        return 1
    fi
    TEST_RESULT="$fullname"
    return 0
}

test_noexist() {
    local myfile="$1"
    if [ -e "$myfile" ]; then
        TEST_RESULT="Failed with test ([), file '$myfile' exists."
        return 1
    fi
    if ls "$myfile"  > /dev/null 2>&1; then
        TEST_RESULT="Failed, ls says '$myfile' exists."
        return 1
    fi
    TEST_RESULT=""
    return 0
}

test_set_atime() {
    # $1 is the path of the file
    # $2 is the timestamp to set
    local pathname="$1"
    local stamp="$2"
    if ! set_atime "$pathname" "$stamp"; then
        TEST_RESULT="Could not set access time for '${NEWDIRNAME}/${SUBFILENAME}', touch command indicated error."
        return 1
    fi
    fuselog "touch command indicated success."
    fuselog -n "Verifying access time... "
    get_atime "$pathname"; local newatime="$ATIME"
    unset ATIME
    if [ -z "$newatime" ]; then
        TEST_RESULT="Could not retrieve access time to verify."
        return 1
    fi
    if [ "$newatime" != "$stamp" ]; then
        fuselog "Verifying access time failed. Expected '$stamp', saw '$newatime'."
        return 1
    fi
    TEST_RESULT=""
    return 0
}

test_compare_atime() {
    # $1 is the filepath
    # $2 is the timestamp to compare to
    local myfile="$1"
    local cmp_atime="$2"
    get_atime "$myfile"; local real_atime="$ATIME"
    unset ATIME
    if [ -z "$real_atime" ]; then
        TEST_RESULT="Could not retrieve modification time."
        return 1
    fi
    if [ "$real_atime" != "$cmp_atime" ]; then
        TEST_RESULT="Failed. Expected '$cmp_atime', saw '$real_atime'."
        return 1
    fi
}

test_set_mtime() {
    # $1 is the path of the file
    # $2 is the timestamp to set
    local pathname="$1"
    local stamp="$2"
    if ! set_mtime "$pathname" "$stamp"; then
        TEST_RESULT="Could not set modification time for '${NEWDIRNAME}/${SUBFILENAME}', touch command indicated error."
        return 1
    fi
    fuselog "touch command indicated success."
    fuselog -n "Verifying modification time... "
    get_mtime "$pathname"; local newmtime="$MTIME"
    unset MTIME
    if [ -z "$newmtime" ]; then
        TEST_RESULT="Could not retrieve modification time to verify."
        return 1
    fi
    if [ "$newmtime" != "$stamp" ]; then
        fuselog "Verifying modification time failed. Expected '$stamp', saw '$newmtime'."
        return 1
    fi
    TEST_RESULT=""
    return 0
}

test_compare_mtime() {
    # $1 is the filepath
    # $2 is the timestamp to compare to
    local myfile="$1"
    local cmp_mtime="$2"
    get_mtime "$myfile"; local real_mtime="$MTIME"
    unset MTIME
    if [ -z "$real_mtime" ]; then
        TEST_RESULT="Could not retrieve modification time."
        return 1
    fi
    if [ "$real_mtime" != "$cmp_mtime" ]; then
        TEST_RESULT="Failed. Expected '$cmp_mtime', saw '$real_mtime'."
        return 1
    fi
}

test_truncate() {
    # $1 is the filename
    # $2 is the desired size
    fuselog -n "Truncating '$1' to $2 bytes... "
    if ! truncate -s $2 "$1" > /dev/null 2>&1; then
        TEST_RESULT="truncate command indicated failure"
        return 1
    fi
    get_filesize "$1"
    if [ "$FILESIZE" -ne "$2" ]; then
        TEST_RESULT="Truncate failed. Expected file length '$2', saw file length '$FILESIZE'."
        return 1
    fi
    TEST_RESULT=""
    return 0
}

test_clobber() {
    # $1 is the filename
    # $2 is the number of bytes to write
    # $3 is the character to fill with
    fuselog -n "Writing $2 '$3's to '$1'... "
    if ! head -c $2 /dev/zero | tr '\000' $3 > "$1"; then
        TEST_RESULT="Command indicated an error when overwriting"
        return 1
    fi
    fuselog Ok
    fuselog -n "Testing file length... "
    get_filesize "$1"
    if [ "$FILESIZE" -ne $2 ]; then
        TEST_RESULT="Incorrect size after overwriting. Expected $2, saw '$FILESIZE'."
        return 1
    fi
    fuselog ok
    fuselog -n "Testing file contents... "
    if ! grep -qE "$3"'{'"$2"'}' "$1"; then
        TEST_RESULT="Overwriting failed. Didn't find expected number of '$3' characters."
        return 1
    fi
    if grep -q '[^'"$3"']' "$1"; then
        TEST_RESULT="Overwriting failed. Unexpected character(s) found in file."
        return 1
    fi
    TEST_RESULT=""
    return 0
}

test_append() {
    # $1 is the filename
    # $2 is the number of bytes to write
    # $3 is the character to fill with
    local orig_size

    fuselog -n "Getting current length of '$1'... "
    get_filesize "$1"; orig_size="$FILESIZE"
    fuselog Done
    fuselog -n "Appending $2 '$3's to '$1'... "
    if ! head -c $2 /dev/zero | tr '\000' $3 >> "$1"; then
        TEST_RESULT="Command indicated an error when appending"
        return 1
    fi
    fuselog Ok
    fuselog -n "Testing file length... "
    get_filesize "$1"
    if [ $(($FILESIZE - $orig_size)) -ne $2 ]; then
        TEST_RESULT="Incorrect file size after appending. Expected $(($2 + $ORIG_SIZE)), saw '$FILESIZE'."
        return 1
    fi
    fuselog ok
    fuselog -n "Testing file contents... "
    if ! tail -c "$2" "$1" | grep -qE "$3"'{'"$2"'}'; then
        TEST_RESULT="Appending failed. Didn't find expected number of '$3' characters at end of file."
        return 1
    fi
    if tail -c "$2" "$1" | grep -q '[^'"$3"']'; then
        TEST_RESULT='Appending failed. Unexpected character(s) found within end of file.'
        return 1
    fi
    TEST_RESULT=""
    return 0
}

test_remount_persistence() {
    # Each positional parameter is the path to a file to compare
    local i
    local -a filenames
    local -a filecontents
    
    # Get the original contents of all the files
    fuselog "Capturing current contents of file(s)."
    for i in $(seq 1 $#); do
        filenames[$i]=${!i}
        fuselog -n "Capturing contents of '${filenames[$i]}'... "
        if ! filecontents[$i]=$(< "${filenames[$i]}"); then
            TEST_RESULT="Failed. Could not capture file's original contents."
            return 1
        fi
        fuselog "Done."
    done
    fuselog "Done for all files."
    
    # Unmount, remount, and move back into the mounted directory
    fuse_unmount
    fuse_mount
    fuselog -n "Changing working directory back to '$MOUNTPATH'... "
    if ! test_cd; then
        # test_cd has already set TEST_RESULT
        return 1
    fi
    unset TEST_RESULT
    fuselog Ok
    
    # Check new contents of all files
    fuselog "Confirming files still exist and still have the same contents."
    for i in $(seq 1 $#); do
        fuselog -n "Confirming '${filenames[$i]}' still exists... "
        if ! [ -e "${filenames[$i]}" ]; then
            TEST_RESULT="Failed. '${filenames[$i]}' does not exist after unmounting and remounting."
            return 1
        fi
        fuselog Ok
        fuselog -n "Comparing contents of '${filenames[$i]}' to old contents... "
        if [ "${filecontents[$i]}" != "$(< ${filenames[$i]})" ]; then
            TEST_RESULT="Failed. Contents of '${filenames[$i]}' changed after unmounting and remounting."
            return 1
        fi
        fuselog Ok
    done
    fuselog "Done for all files."
    TEST_RESULT=""
    return 0
}

test_rmdir_fail() {
    # $1 should be a non-empty directory. This test succeeds if rmdir fails.
    if rmdir "$NEWDIRNAME" 2> /dev/null; then
        TEST_RESULT="Removed non-empty directory, incorrect behavior."
        return 1
    fi
    TEST_RESULT=""
    return 0
}

test_rm() {
    # $1 is the filename to delete
    local myfile="$1"
    if ! rm "$myfile" 2> /dev/null; then
        TEST_RESULT="rm command indicated error."
        return 1
    fi
    if [ -e "$myfile" ]; then
        TEST_RESULT="Failed. rm command indicated success, but file still exists."
        return 1
    fi
}

test_rmdir() {
    # $1 is the name of the directory to remove
    local mydir="$1"
    if ! rmdir "$mydir" 2> /dev/null; then
        TEST_RESULT="rmdir command indicated error."
        return 1
    fi
    if [ -d "$mydir" ]; then
        TEST_RESULT="Failed. rmdir command indicated success, but file still exists."
        return 1
    fi
    TEST_RESULT=""
    return 0
}

test_hard_link() {
    # Create two directories to work with
    if ! run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_mkdir; then
        TEST_RESULT="Could not create directory. $TEST_RESULT"
        return 1
    fi
    fuselog Ok
    local dirone="$TEST_RESULT"
    if ! run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_mkdir; then
        TEST_RESULT="Could not create directory. $TEST_RESULT"
        return 1
    fi
    fuselog Ok
    local dirtwo="$TEST_RESULT"
    if ! run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_create_file "$dirone"; then
        TEST_RESULT="Could not create file. $TEST_RESULT"
        return 1
    fi
    fuselog Ok
    local mybasename="${TEST_RESULT##*/}"
    unset TEST_RESULT
    
    fuselog -n "Setting hard link from '$dirtwo/$mybasename' to '$dirone/$mybasename'... "
    if ! ln "$dirone/$mybasename" "$dirtwo/$mybasename" 2> /dev/null; then
        TEST_RESULT="ln command returned nonzero error status"
        return 1
    fi
    fuselog Ok
    fuselog -n "Verifying '$dirone/$mybasename' still exists... "
    if ! [ -e "$dirone/$mybasename" ]; then
        TEST_RESULT="Failed. Original file does not exist."
        return 1
    fi
    fuselog Ok
    fuselog -n "Verifying '$dirtwo/$mybasename' also exists... "
    if ! [ -e "$dirtwo/$mybasename" ]; then
        TEST_RESULT="Failed. New hard link does not exist."
        return 1
    fi
    fuselog Ok
    fuselog -n "Setting hard link from '$mybasename' to '$dirone/$mybasename'... "
    if ! ln "$dirone/$mybasename" "$mybasename" 2> /dev/null; then
        TEST_RESULT="ln command returned nonzero error status"
        return 1
    fi
    fuselog Ok
    fuselog -n "Verifying '$mybasename' exists... "
    if ! [ -e "$mybasename" ]; then
        fuselog "Failed. New hard link in the root mount directory does not exist."
        return 1
    fi
    fuselog Ok
    
    # Cleanup. It doesn't matter whether this fails.
    fuselog "Cleaning up files and directories from the hard link test."
    fuselog "These will say 'Ok' regardless of success or failure."
    fuselog -n "Deleting '$mybasename'... "
    run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_rm "$mybasename"
    fuselog Ok
    fuselog -n "Deleting '$dirone/$mybasename'... "
    run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_rm "$dirone/$mybasename"
    fuselog Ok
    fuselog -n "Deleting '$dirtwo/$mybasename'... "
    run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_rm "$dirtwo/$mybasename"
    fuselog Ok
    fuselog -n "Deleting directory '$dirone'... "
    run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_rmdir "$dirone"
    fuselog Ok
    fuselog -n "Deleting directory '$dirtwo'... "
    run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_rmdir "$dirtwo"
    fuselog Ok
    
    TEST_RESULT=""
    return 0
}









MOUNTOPTIONS='-f -s'
TIMESTAMP_FMT='+%Y%m%d%H%M.%S'
LOGFILE=/dev/null
ORIGINAL_WORKING_DIR=$(pwd)
VALGRIND=""
VALGRIND_REDIR="/dev/stderr"
VALGRIND_OPTS=""
NOMOUNT=0
FAILURE_COUNT=0
FATAL_FAILURE_COUNT=0
FAILURE_MSGS=""
DEFAULT_ATTEMPTS=3
DEFAULT_WAIT=2

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




fuselog -n "Checking output of 'df \"$MOUNTPATH\"'... "
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_df; then
    fuselog Ok
else
    fuselog "Failed, continuing on."
fi

# Get ready to change directory to the mountpoint
fuselog
fuselog -n "Basic ls check (just checking returned value)... "
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_ls_basic; then
    fuselog Ok
else
    fuselog "Failed, continuing on."
fi

# Change the directory. Do not continue on failure.
fuselog -n "Changing working directory... "
run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 1 test_cd
fuselog Ok


# Test file and directory creation (create() and mkdir())

fuselog
fuselog "Checking file and directory creation"

fuselog "Directory creation:"
# Error is fatal because the created directory is used later.
run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 1 test_mkdir
fuselog Ok
NEWDIRNAME="$TEST_RESULT"

fuselog "File creation in root directory:"
# Error is fatal because the file is used later
run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 1 test_create_file
ROOTFILENAME="$TEST_RESULT"
fuselog Ok
fuselog -n "Making sure '$NEWDIRNAME/$ROOTFILENAME' doesn't exist... "
if run_test 1 0 0 test_noexist "$NEWDIRNAME/$ROOTFILENAME"; then
    fuselog Ok
else
    fuselog "Failed, continuing on."
fi

fuselog "File creation in subdirectory:"
# Error is fatal because the file is used later
run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 1 test_create_file "$NEWDIRNAME"
SUBFILENAME="${TEST_RESULT##*/}"
fuselog Ok
fuselog -n "Making sure '$SUBFILENAME' doesn't exist... "
if run_test 1 0 0 test_noexist "$SUBFILENAME"; then
    fuselog Ok
else
    fuselog "Failed, continuing on."
fi


# Check utimens() (using touch)

fuselog
fuselog Setting access and modification times

fuselog -n "Reading original access time for '${NEWDIRNAME}/${SUBFILENAME}'... "
get_atime "${NEWDIRNAME}/${SUBFILENAME}"; OLDATIME="$ATIME"
unset ATIME
if [ -z "$OLDATIME" ]; then
    tally_failure 0 get_atime "Failed."
else
    fuselog Done
fi
unset OLDATIME
fuselog -n "Reading original modification time for '${NEWDIRNAME}/${SUBFILENAME}'... "
get_mtime "${NEWDIRNAME}/${SUBFILENAME}"; OLDMTIME="$MTIME"
unset MTIME
if [ -z "$OLDMTIME" ]; then
    tally_failure 0 get_mtime "Failed."
else
    fuselog Done.
fi

fuselog "Access times:"
# $RANDOM is 16-bit unsigned, up to 32767. Multiply by 10000 to get up to about
# 10 years' worth of seconds, and multiply by -1 to subtract from the current
# time.
make_timestamp -${RANDOM}0000; PAST_TIMESTAMP="$GENERATED_TIMESTAMP"
unset GENERATED_TIMESTAMP
fuselog -n "Setting access time for '${NEWDIRNAME}/${SUBFILENAME}' to '$PAST_TIMESTAMP'... "
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_set_atime "${NEWDIRNAME}/${SUBFILENAME}" "$PAST_TIMESTAMP"; then
    fuselog Ok
else
    fuselog "Failed, continuing on."
fi
OLDATIME="$PAST_TIMESTAMP"
unset PAST_TIMESTAMP
fuselog -n "Verifying modification time is unchanged... "
if run_test 1 0 0 test_compare_mtime "${NEWDIRNAME}/${SUBFILENAME}" "$OLDMTIME"; then
    fuselog Ok
else
    fuselog "Failed, continuing on."
fi
unset OLDMTIME

# Google Drive doesn't seem to support future access times.
## Use a positive number this time, to make a future timestamp
#make_timestamp ${RANDOM}0000; FUTURE_TIMESTAMP="$GENERATED_TIMESTAMP"
#fuselog -n "Setting access time for '${NEWDIRNAME}/${SUBFILENAME}' to '$FUTURE_TIMESTAMP'... "
#if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_set_atime "${NEWDIRNAME}/${SUBFILENAME}" "$FUTURE_TIMESTAMP"; then
#    fuselog Ok
#else
#    fuselog "Failed, continuing on."
#fi
#unset FUTURE_TIMESTAMP
#fuselog -n "Verifying modification time is unchanged... "
#if run_test 1 0 0 test_compare_mtime "${NEWDIRNAME}/${SUBFILENAME}" "$OLDMTIME"; then
#    fuselog Ok
#else
#    fuselog "Failed, continuing on."
#fi
#unset OLDMTIME

fuselog "Modification times:"
# Negative number for a time in the past
make_timestamp -${RANDOM}0000; PAST_TIMESTAMP="$GENERATED_TIMESTAMP"
unset GENERATED_TIMESTAMP
fuselog -n "Setting modification time for '${NEWDIRNAME}/${SUBFILENAME}' to '$PAST_TIMESTAMP'... "
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_set_mtime "${NEWDIRNAME}/${SUBFILENAME}" "$PAST_TIMESTAMP"; then
    fuselog Ok
else
    fuselog "Failed, continuing on."
fi
unset PAST_TIMESTAMP
fuselog -n "Verifying access time is unchanged... "
if run_test 1 0 0 test_compare_atime "${NEWDIRNAME}/${SUBFILENAME}" "$OLDATIME"; then
    fuselog Ok
else
    fuselog "Failed, continuing on."
fi

# Use a positive number this time, to make a future timestamp
make_timestamp ${RANDOM}0000; FUTURE_TIMESTAMP="$GENERATED_TIMESTAMP"
unset GENERATED_TIMESTAMP
fuselog -n "Setting modification time for '${NEWDIRNAME}/${SUBFILENAME}' to '$FUTURE_TIMESTAMP'... "
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_set_mtime "${NEWDIRNAME}/${SUBFILENAME}" "$FUTURE_TIMESTAMP"; then
    fuselog Ok
else
    fuselog "Failed, continuing on."
fi
unset FUTURE_TIMESTAMP
fuselog -n "Verifying access time is unchanged... "
if run_test 1 0 0 test_compare_atime "${NEWDIRNAME}/${SUBFILENAME}" "$OLDATIME"; then
    fuselog Ok
else
    fuselog "Failed, continuing on."
fi
unset OLDATIME

# Check write() (which also requires open(), release(), fsync(), truncate()/ftruncate())

fuselog
fuselog Reading, writing and truncation
fuselog Truncation:
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 0; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 100; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 1000; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 50; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 50; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
# if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 24681357; then
#    fuselog Ok
#else
#    fuselog Failed, continuing on.
#fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_truncate "${NEWDIRNAME}/${SUBFILENAME}" 0; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi

fuselog "Writing (Overwriting) and Reading:"
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_clobber "${NEWDIRNAME}/${SUBFILENAME}" 1024 F; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_clobber "${NEWDIRNAME}/${SUBFILENAME}" 75 u; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_clobber "${NEWDIRNAME}/${SUBFILENAME}" 100 s; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_clobber "${NEWDIRNAME}/${SUBFILENAME}" 50 e; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi

# Make sure the total size doesn't get too large here, since we'll read the
# entire file contents into variables later.
fuselog "Appending and Reading:"
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_append "${NEWDIRNAME}/${SUBFILENAME}" 3 D; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_append "${NEWDIRNAME}/${SUBFILENAME}" 50 r; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_append "${NEWDIRNAME}/${SUBFILENAME}" 100 i; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_append "${NEWDIRNAME}/${SUBFILENAME}" 10 v; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_append "${NEWDIRNAME}/${SUBFILENAME}" 200 e; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi

# Test persistence through unmount/remount
fuselog Testing persistence
run_test 1 0 1 test_remount_persistence "$ROOTFILENAME" "${NEWDIRNAME}/${SUBFILENAME}"
fuselog Ok



# Check unlink() and rmdir() (using rm and rmdir)

fuselog
fuselog Deleting files and directories
fuselog Checking for proper failure of rmdir on non-empty directories:
fuselog -n "Trying to rmdir '$NEWDIRNAME', should NOT succeed... "
if run_test 1 0 0 test_rmdir_fail "$NEWDIRNAME"; then
    fuselog "Ok, behaved as expected."
else
    fuselog Failed, continuing on.
fi
fuselog "Ok, behaved as expected."

fuselog Deleting files:
fuselog -n "rm \"$ROOTFILENAME\"... "
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_rm "$ROOTFILENAME"; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
unset ROOTFILENAME
fuselog -n "rm \"${NEWDIRNAME}/${SUBFILENAME}\"... "
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_rm "${NEWDIRNAME}/${SUBFILENAME}"; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
unset SUBFILENAME

fuselog Removing directory:
fuselog -n "rmdir \"$NEWDIRNAME\" (should succeed this time)... "
if run_test 5 3 0 test_rmdir "$NEWDIRNAME"; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi
unset NEWDIRNAME



fuselog
fuselog Hard links and symbolic links
fuselog Creating hard link:
if run_test "$DEFAULT_ATTEMPTS" "$DEFAULT_WAIT" 0 test_hard_link; then
    fuselog Ok
else
    fuselog Failed, continuing on.
fi




fuselog
fuselog 'DONE!'
fuselog No fatal errors
clean_exit 0