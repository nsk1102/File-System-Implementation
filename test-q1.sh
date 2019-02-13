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
testnum=0
echo 'Test 0 - simple passing test'
test_equal 0 0 'this better not fail'
test_summary
# ----  test is finished --------------

# bash arrays:
#     array=(val1 val2 val3)
#     array+=(val4)
#     for v in ${array[@]}; do
#         .. lists all values

# data for tests - see description

testnum=1
echo 'Test 1 - ls test'
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
    set -- $(ls -l -d --time-style=+%s $DIR$path)
    test_equal $mode $1 'mode not equal'
    test_equal $len $5 'len not equal'
    test_equal $time $6 'mode not equal'
done
test_summary

testnum=2
echo 'Test 2 - cat test'
test2dat=('/file.A 3509208153 1000' \
	      '/file.7 94780536 6644' \
	      '/dir1/file.0 4294967295 0' \
	      '/dir1/file.2 3106598394 2012' \
	      '/dir1/file.270 1733278825 276177')

for line in "${test2dat[@]}"; do
    set $line
    path=$1; cksum=$2.$3
    echo path=$path cksum=$cksum
    set -- $(cat $DIR$path | cksum)
    test_equal $cksum $1.$2 'mode not equal'
done
test_summary

# because of the way the FUSE framework works, you don't get proper
# error codes out of commands, so just check that 'ls' on these fails
testnum=3
echo 'Test 3 - dd test'
for line in "${test2dat[@]}"; do
    set $line
    path=$1; cksum=$2.$3
    echo path=$path cksum=$cksum
    set -- $(dd if=$DIR$path status=none bs=17 | cksum)
    test_equal $cksum $1.$2 'mode not equal'
done
test_summary

testnum=4
echo 'Test 4 - ls fail test'
test3dat=("/not-a-file" "/dir1/not-a-file" "/not-a-dir/file.0" \
          "/file.A/file.0" "/dir1/file.0/testing")
for path in "${test3dat[@]}"; do
    echo path=$path
    echo $(ls -l -d --time-style=+%s $DIR$path)
done
test_summary

# last test -
#   read a directory (with 'cat')
#   list a file ('ls file/')
testnum=5
echo 'Test 5 - read a directory'
test4dat=("/" "/dir1")
for path in "${test4dat[@]}"; do
    echo path=$path
    echo $(cat $DIR$path)
done
test_summary

testnum=6
echo 'Test 6 - list a file'
test5dat=("/file.A" "/file.7" "/dir1/file.2")
for path in "${test5dat[@]}"; do
    echo path=$path
    set -- $(ls $DIR$path)
    test_equal $DIR$path $1 'result not equal'
done
test_summary