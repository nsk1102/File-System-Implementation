More testing
============

This posting covers the following topics:

* another file added to mktest
* using valgrind to test your code in Python
* running and debugging in FUSE mode
* testing using shell scripts

Mktest - generate harder files
--------------------------

If you run `mktest` with the flag `-extra` it will generate an additional file, 'file.10', in the top-level directory:

    hw3$ ./mktest -extra test2.img
	hw3$ ./homework -image test2.img dir
	hw3$ ls -l dir/
	total 10
	drwxr-xr-x 1 student student  1024 Jul 13  2012 dir1
	-rwxrwxrwx 1 student student 10234 Jul 13  2012 file.10
	-rwxrwxrwx 1 student student  6644 Jul 13  2012 file.7
	-rwxrwxrwx 1 student student  1000 Jul 13  2012 file.A
    hw3$ fusermount -u dir

This new file consists of the string '1234567' repeated 1462 times, for a total size of 10234 bytes, and the blocks are out of order in both the direct table and the indirect block. You are encouraged to test with this disk image.

Oh, and it changes the number of free blocks to 720:

	hw3$ df dir
	Filesystem     1K-blocks  Used Available Use% Mounted on
	homework            1024   304       720  30% /home/pjd/2018-fall/cs5600-f18/hw3/dir


Using Valgrind
--------------

Valgrind is a "dynamic binary instrumentation framework", which basically means (a) it emulates the CPU, using binary translation, and (b) it watches things while it's running your code. Most importantly, it complains if you read from any uninitialized variables, use allocated memory after it's been freed, or do any number of other bad things that the C compiler can't find. It's a really useful debugging tool.

To use it you just type `valgrind <cmd> <args>`, and it will run `<cmd>` with arguments `<args`. It's a bit complicated, however, if the command is your Python test script, since Python does a number of things that make valgrind complain. To get around this you need to use a config file to tell Python to ignore errors from Python itself, and only complain about errors in your code; I've pushed that file (`valgrind-python.supp`) to your repositories. Use it like this:

    $valgrind --suppressions=valgrind-python.supp python test-q1.py
	==30956== Memcheck, a memory error detector
	==30956== Copyright (C) 2002-2015, and GNU GPL'd, by Julian Seward et al.
    [... test output ...]
	==30956== 
	   [... more valgrind output ...]
	==30956== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 21411 from 51)

(the number in `==30956==` is a process ID, and will change each time you run it)

If you did something wrong, you'll see messages like this:

	==30939== Use of uninitialised value of size 4
	==30939==    at 0x40AA24B: _itoa_word (_itoa.c:179)
	==30939==    by 0x40AE221: vfprintf (vfprintf.c:1631)
	==30939==    by 0x40B4695: printf (printf.c:33)
	==30939==    by 0x4042398: hw3_init (libhw3.c:33)
	[...]

Running and debugging in FUSE mode
--------------------

To run in FUSE mode, run the `homework` executable with the `-image` flag and directory to mount on, and then unmount it with the `fusermount` command:

	hw3$ ./homework -image test2.img dir
    hw3$
     ... 
    hw3$ fusermount -u dir

Note that `./homework` appears to exit - what's happening is that the program is forking and the subprocess stays running while the parent exits. This is great for a production file system, but not quite what you want for debugging. To debug (whether to use `gdb` or just to see your printfs) use some extra arguments:

	hw3$ ./homework -s -d -image test2.img dir
	FUSE library version: 2.9.7
	nullpath_ok: 0
	nopath: 0
	utime_omit_ok: 0
	unique: 1, opcode: INIT (26), nodeid: 0, insize: 56, pid: 0
	INIT: 7.26
	flags=0x001ffffb
	max_readahead=0x00020000
	   INIT: 7.19
	   flags=0x00000011
	   max_readahead=0x00020000
	   max_write=0x00020000
	   max_background=0
	   congestion_threshold=0
	   unique: 1, success, outsize: 40

It will hang there, so you'll have to go to another window to interact with your file system. Here's what it prints out by default when I run `cat dir/file.A` in another window:

	unique: 2, opcode: LOOKUP (1), nodeid: 1, insize: 47, pid: 10075
	LOOKUP /file.A
	getattr /file.A
	   NODEID: 2
	   unique: 2, success, outsize: 144
	unique: 3, opcode: OPEN (14), nodeid: 2, insize: 48, pid: 10075
	   unique: 3, success, outsize: 32
	unique: 4, opcode: READ (15), nodeid: 2, insize: 80, pid: 10075
	read[0] 4096 bytes from 0 flags: 0x8000
	   read[0] 1000 bytes from 0
	   unique: 4, success, outsize: 1016
	unique: 5, opcode: GETATTR (3), nodeid: 2, insize: 56, pid: 10075
	getattr /file.A
	   unique: 5, success, outsize: 120
	unique: 6, opcode: FLUSH (25), nodeid: 2, insize: 64, pid: 10075
	   unique: 6, error: -38 (Function not implemented), outsize: 16
	unique: 7, opcode: RELEASE (18), nodeid: 2, insize: 64, pid: 0
	   unique: 7, success, outsize: 16

EVEN IF IT CRASHES, you'll need to run `fusermount -u dir` to clean things up.

Finally, you can use `-s -d` to run under `valgrind`, as well. (here I unmounted it from another window before it had a chance to do anything)

	hw3$ valgrind ./homework -s -d -image test2.img dir
	==10096== Memcheck, a memory error detector
	==10096== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
	==10096== Using Valgrind-3.13.0 and LibVEX; rerun with -h for copyright info
	==10096== Command: ./homework -s -d -image test2.img dir
	==10096== 
	FUSE library version: 2.9.7
	nullpath_ok: 0
	nopath: 0
	utime_omit_ok: 0
	unique: 1, opcode: INIT (26), nodeid: 0, insize: 56, pid: 0
	INIT: 7.26
	flags=0x001ffffb
	max_readahead=0x00020000
	   INIT: 7.19
	   flags=0x00000011
	   max_readahead=0x00020000
	   max_write=0x00020000
	   max_background=0
	   congestion_threshold=0
	   unique: 1, success, outsize: 40
	==10096== 
	==10096== HEAP SUMMARY:
	==10096==     in use at exit: 6,223 bytes in 10 blocks
	==10096==   total heap usage: 54 allocs, 44 frees, 241,648 bytes allocated
	==10096== 
	==10096== LEAK SUMMARY:
	==10096==    definitely lost: 20 bytes in 1 blocks
	==10096==    indirectly lost: 21 bytes in 4 blocks
	==10096==      possibly lost: 0 bytes in 0 blocks
	==10096==    still reachable: 6,182 bytes in 5 blocks
	==10096==         suppressed: 0 bytes in 0 blocks
	==10096== Rerun with --leak-check=full to see details of leaked memory
	==10096== 
	==10096== For counts of detected and suppressed errors, rerun with: -v
	==10096== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)

Testing using shell scripts
---------------------------

To test your code in FUSE mode you'll need to write a shell script which uses various shell utilities to access the mounted HW3 file system. The following utilities are really useful in building these scripts:

* ls          (tests `getattr`, `readdir`)
* cat        (tests `read`)
* dd         (tests `read` with different block sizes)
* cksum  (checksum - use for verifying file contents)

`dd` is a weird command with a very non-Unix syntax. You'll use the following options:

    dd if=input of=output bs=NN status=none

where the arguments are:

* if=file          - specifies input file. (or standard in, if not specified)
* of=file         - specifies output file (or standard out if missing)
* bs=NN         - block size. Read NN bytes at a time
* status=none - don't print annoying messages

I've provided a file `test-q1.sh` with a few functions in it that make it easier to write tests. Note that these functions **only** work with `bash`, not `/bin/sh`, `zsh`, or any other version of the shell. Anyway, the functions use two global variables, `testnum` and `failed_test`:

	# usage: test_equal expr1 expr2 [string]
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

To use them you set `testnum` before each test, run `test_summary` after each test, and use `test_equal` and `test_not_equal` in the middle:

	testnum=1
	echo 'Test 1 - simple passing test'
	test_equal 0 0 'something is broken - 0 != 0?'
	test_summary

Running it gives us the following:
	hw3$ bash test-q1.sh
	Test 1 - simple passing test
	  PASSED!
    hw3$

You might want to create tables of data, just like for the python script; you can use Bash arrays to do this:

	test1dat=('/file.A -rwxrwxrwx 1000 1342177480' \
				  '/file.7 -rwxrwxrwx 6644 1342177580' \
                  '/file.10 -rwxrwxrwx 10234 1342177580' \
				  '/dir1 drwxr-xr-x 1024 1342177680' \
				  '/dir1/file.2 -rwxrwxrwx 2012 1342177480' \
				  '/dir1/file.0 -rwxrwxrwx 0 1342177480' \
				  '/dir1/file.270 -rwxrwxrwx 276177 1342177580')

	for line in "${test1dat[@]}"; do                # copy this *exactly*
		set $line
		path=$1; mode=$2; len=$3; time=$4
		echo path=$path mode=$mode len=$len time=$time
	done

The `set` command sets the possitional parameters $1, $2, ... to its arguments. Thus the first time through the loop, the command executed is `set '/file.A -rwxrwxrwx 1000 1342177480`, and then the next line sets path, mode, etc. to each part.

To capture the output of a command, you can use '$( cmd )':

	hw3$ ls -l -d --time-style=+%s .
	drwxrwxr-x 7 pjd pjd 4096 1543384230 .
	hw3$ set $(ls -l -d --time-style=+%s .)
	hw3$ echo $1 / $2 / $3 / $4 / $5 / $6
	drwxrwxr-x / 7 / pjd / pjd / 4096 / 1543384230

The arguments to `ls` are: don't list directory contents (-d), long format (-l), print numeric time (--time-style)

Here's a table of checksums of each file:

	test2dat=('/file.A 3509208153 1000' \
				  '/file.7 94780536 6644' \
				  '/file.10 3295410025 10234' \
				  '/dir1/file.0 4294967295 0' \
				  '/dir1/file.2 3106598394 2012' \
				  '/dir1/file.270 1733278825 276177')

	for line in "${test2dat[@]}"; do
		set $line
		path=$1; cksum=$2.$3
		echo path=$path cksum=$cksum
	done

You can use `cksum' to check the files:

	hw3$ cat dir/file.A | cksum
	3509208153 1000
	hw3$ set $(cat dir/file.A | cksum)
	hw3$ echo $1 $2
	3509208153 1000

Finally you can use `dd` to read files in various sized pieces - e.g. 17 bytes at a time:

    hw3$ dd if=dir/file.A status=none bs=17 | cksum
    3509208153 1000

