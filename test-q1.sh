#!/bin/bash
#
# usage: test-q1.sh <dir>
#        where 'test.img' is mounted on <dir>
#        remember to do 'fusermount -u <dir>' when you're done.
#

DIR=$1

failed_test=

# usage: test_equal expr1 expr2 "error message"
# assumes: $testnum
#
test_equal(){
    if [ "$1" != "$2" ] ; then
	echo "FAIL $testnum: $3 ($1 != $2)"
	failed_test=$testnum
    fi
}

test_not_equal(){
    if [ "$1" = "$2" ] ; then
	echo "FAIL $testnum: $3 ($1 = $2)"
	failed_test=$testnum
    fi
}

test_summary(){
    if [ "$testnum" = "$failed_test" ] ; then
        echo '  FAILED!'
    else
        echo '  PASSED!'
    fi
    echo
}

# ----  this is a test ----------------
testnum=1
echo 'Test 1 - simple passing test'
test_equal 0 0 'this better not fail'
test_summary
# ----  test is finished --------------

# bash arrays:
#     array=(val1 val2 val3)
#     array+=(val4)
#     for v in ${array[@]}; do
#         .. lists all values

# data for tests - see description

test1dat=('/file.A -rwxrwxrwx 1000 1342177480' \
	      '/file.7 -rwxrwxrwx 6644 1342177580' \
	      '/dir1 drwxr-xr-x 1024 1342177680' \
              '/dir1/file.2 -rwxrwxrwx 2012 1342177480' \
              '/dir1/file.0 -rwxrwxrwx 0 1342177480' \
              '/dir1/file.270 -rwxrwxrwx 276177 1342177580')

for line in "${test1dat[@]}"; do
    set $line
    path=$1; mode=$2; len=$3; time=$4
    echo path=$path mode=$mode len=$len time=$time
done

test2dat=('/file.A 3509208153 1000' \
	      '/file.7 94780536 6644' \
	      '/dir1/file.0 4294967295 0' \
	      '/dir1/file.2 3106598394 2012' \
	      '/dir1/file.270 1733278825 276177')

for line in "${test2dat[@]}"; do
    set $line
    path=$1; cksum=$2.$3
    echo path=$path cksum=$cksum
done

# because of the way the FUSE framework works, you don't get proper
# error codes out of commands, so just check that 'ls' on these fails

test3dat=("/not-a-file" "/dir1/not-a-file" "/not-a-dir/file.0" \
	      "/file.A/file.0" "/dir1/file.0/testing")
for path in "${test3dat[@]}"; do
    echo path=$path
done

# last test -
#   read a directory (with 'cat')
#   list a file ('ls file/')





