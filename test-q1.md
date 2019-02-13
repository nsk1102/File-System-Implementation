How to test your code (read-only)
=================================

The files libhw3.c and hw3test.py implement a simple interface from Python to your Homework 3 code, allowing you to write tests much more easily than in C.  These instructions describe how to use the Python 'unittest' framework; however if you wish you can use another one.

If you're curious: libhw3.c implements some simple C functions that call the methods in homework.c (via their entries in the `fs_ops` structure), and is compiled into a dynamic library that can be loaded into Python. The hw3test.py module uses the Python 'ctypes' framework to call these functions, returning results in a somewhat Python-friendly form. 

Note that this set of instructions only covers the read-only part of testing, mostly because there are a lot more functions to describe for the write part.

Using hw3test.py
--------------

First, import it into python. Here we're renaming it to 'hw3', so we don't have to type 'hw3test' a zillion times:
        import hw3test as hw3

Then initialize it with the name of an image file. (assuming you created a file 'test.img' with the command './mktest test.img'):

        hw3.init('test.img')

File type constants - the following constants are described in 'man 2 stat' (plus others we don't use):

* `hw3.S_IFMT`
* `hw3.S_IFREG`
* `hw3.S_IFDIR`

Error number constants - note that the comments in homework.c will tell you which errors are legal to return from which methods.

* hw3.EPERM - permission error
* hw3.ENOENT - file/dir not found
* hw3.EIO - if `block_read` or `block_write` fail
* hw3.ENOMEM  - out of memory
* hw3.ENOTDIR - what it says
* hw3.EISDIR - what it says
* hw3.EINVAL - invalid parameter (see comments in homework.c)
* hw3.ENOSPC - out of space on disk
* hw3.EOPNOTSUPP - not finished with the homework yet
* hw3.errors[] - maps error code to string (e.g. "if err < 0: print hw3.errors[-err]")

Get file attributes:

        sb = getattr(path)

Returns a structure with the following fields (see 'man 2 stat')

 * `sb.st_mode` - file mode (`S_IFREG` or `S_IFDIR`, ORed with 9-bit permissions)
 * `sb.st_uid`, `sb.st_gid` - file owner, file group
 * `sb.st_size` - size in bytes
 * `sb.st_mtime`, `sb.st_ctime` - modification time, creation time

Read from a file:

        val,data = hw3.read(path, len, offset)

Read up to 'len' bytes starting at 'offset'. 'val' is the number of bytes read, or <0 in the case of errors

List a directory:

        val, entries = hw3.readdir(path)

Read directory entries. 'val' is number of entries returned - i.e.len(entries) - and 'entries' is an array of structures with the following fields. (all but 'name' are the same as for 'getattr')
 * `entries[i].name`
 * `entries[i].st_mode`
 * `entries[i].st_uid`, `st_gid`
 * `entries[i].st_size`
 * `entries[i].st_mtime`, `st_ctime`

Get file system attributes:

        sv = hw3.statvfs()

Where the return value has the following fields
 * `sv.f_bsize` - 1024
 * `sv.f_blocks` - total number of blocks (1024 for test image)
 * `sv.f_bfree` - free blocks (731 for test image)
 * `sv.f_namemax` - 27

Python unittest
---------------

The tests below all use the Python [unittest] [1] framework. Import 'unittest', create a class that subclasses unittest.TestCase, and define methods with names starting with 'test'. Within those methods, verify results using `self.assertTrue(expression)` and optionally `self.assertEqual(a,b)`. As an example:

		#!/usr/bin/python
		import unittest
		import hw3test as hw3

		class tests(unittest.TestCase):
		def test_1_succeeds(self):
			print 'Test 1 - always succeeds'
			self.assertTrue(True)

		def test_2_fails(self):
			print 'test 2 - always fails'
			self.assertTrue(False)

		if __name__ == '__main__':
			hw3.init('test.img')
		unittest.main()

(if you don't know python, [here's] [1] an explanation of the last few lines)

[1]: https://stackoverflow.com/questions/8228257/what-does-if-name-main-mean-in-python

First test - statfs
-------------------

All you need to implement the `statfs` method is to read in the superblock and the block bitmap. (you might as well read in the inode bitmap and the inode table, as well) You can calculate the free blocks by iterating through the block bitmap (up to `n_blocks` from the superblock) and counting the entries that aren't set.

Once you've implemented it you can create a simple test to see if it works, since the return values for the mktest image are known values:

		def test_1_statfs(self):
			print 'Test 1 - statfs:'
			v,sfs = hw3.statfs()
			self.assertTrue(v == 0)
			self.assertTrue(sfs.f_bsize == 1024)    # any fsx600 file system
			self.assertTrue(sfs.f_blocks == 1024)   # only if image size = 1MB
			self.assertTrue(sfs.f_bfree == 731)         # for un-modified mktest output
			self.assertTrue(sfs.f_namemax == 27)    # any fsx600 file system

Now you can run your test script; hopefully the results will be boring like this:

		hw3$ python test-q1.py 
		Test 1 - statfs:
		.
		----------------------------------------------------------------------
		Ran 1 test in 0.001s

		OK
		hw3$

If you want to run only a single test, you can specify that test on the command line by giving its class and method name:

		hw3$ python test-q1b.py tests.test_1_statfs
		Test 1 - statfs:
		.
		----------------------------------------------------------------------
		Ran 1 test in 0.000s

		OK
		hw3$ 

Debugging
---------

In a perfect world you would write your code, writing your tests as you go along, and the tests would never fail or crash. Things don't work that way:

		hw3$ python test-q1.py
		Test 1 - statfs:
		Segmentation fault (core dumped)
		hw3$ 

It's not immediately obvious how to debug your code when this happens - what you need to do is to run python under GDB:

		hw3$ gdb --args python test-q1b.py 
		GNU gdb (Ubuntu 7.11.1-0ubuntu1~16.5) 7.11.1
		 [yada, yada...]
		Reading symbols from python...(no debugging symbols found)...done.
		(gdb) run
		Starting program: /usr/bin/python test-q1b.py
		[Thread debugging using libthread_db enabled]
		Using host libthread_db library "/lib/i386-linux-gnu/libthread_db.so.1".
		Test 1 - statfs:

		Program received signal SIGSEGV, Segmentation fault.
		0xb7fca234 in fs_statfs (path=0xb7d33634 "/", st=0x8478ab0)
		at homework-soln.c:936
		936	    *(char*)0 = 0; /* crash to demonstrate debugging */
		(gdb) 

But what if you want to set a breakpoint? Just set it before typing 'run' in Gdb, and it should work:

		hw3$ gdb --args python test-q1b.py 
		GNU gdb (Ubuntu 7.11.1-0ubuntu1~16.5) 7.11.1
		  [...]
		Reading symbols from python...(no debugging symbols found)...done.
		(gdb) b fs_statfs
		Function "fs_statfs" not defined.
		Make breakpoint pending on future shared library load? (y or [n]) y
		Breakpoint 1 (fs_statfs) pending.
		(gdb) run
		Starting program: /usr/bin/python test-q1b.py
		[Thread debugging using libthread_db enabled]
		Using host libthread_db library "/lib/i386-linux-gnu/libthread_db.so.1".
		Test 1 - statfs:

		Breakpoint 1, fs_statfs (path=0xb7d33634 "/", st=0x8478ab0)
		at homework-soln.c:936
		936	    *(char*)0 = 0; /* crash to demonstrate debugging */
		(gdb)

The mktest image
----------------

For read-only tests it's helpful to know all the information about the test image generated by mktest. Here is the output from `ls -ln`, along with the steps involved in mounting the image and unmounting it afterwards.

		hw3$ mkdir dir
		hw3$ ./mktest test.img
		hw3$ ./homework -image test.img dir
		hw3$ ls -ln dir
		total 5
		drwxr-xr-x 1 1000 1000 1024 Jul 13  2012 dir1
		-rwxrwxrwx 1 1000 1000 6644 Jul 13  2012 file.7
		-rwxrwxrwx 1 1000 1000 1000 Jul 13  2012 file.A
		hw3$ ls -ln dir/dir1
		total 136
		-rwxrwxrwx 1 1000 1000      0 Jul 13  2012 file.0
		-rwxrwxrwx 1 1000 1000   2012 Jul 13  2012 file.2
		-rwxrwxrwx 1 1000 1000 276177 Jul 13  2012 file.270
		hw3$ fusermount -u dir
		hw3$

Translating file/directory plus permissions into octal (because permission data comes in groups of 3 bits; note that in Python octal is indicated by `0o` instead of the confusing initial zero used in C):

		drwxr-xr-x = S_IFDIR | 0o755 = 0o040755
		-rwxrwxrwx = S_IFREG | 0o777 = 0o100777

Each file contains a single byte, repeated. As a Python string, the contents are:

		/file.A         'A' * 1000
		/file.7         '4' * 6644
		/dir1/file.0    ''
		/dir1/file.2    '2' * 2012
		/dir1/file.270  'K' * 276177

Oh, and those dates are actually the following timestamps:

* `/dir1` - 0x50000190 
* `/file.A` - 0x500000C8
* `/dir1/file.2` - same
* `/dir1/file.0` - same
* `/dir1/file.270` - 0x5000012C
* `/file.7` - same

Testing getattr()
------------------

For each of the files and directories, call `getattr` and verify the results - e.g.

		def test_2_read(self):
			v,sb = hw3.getattr('/dir1')
			self.assertTrue(v == 0)
			self.assertTrue(sb.st_mode == 0o040755)
			self.assertTrue(sb.st_uid == 1000)
			 ...

You'll probably want to make a list of paths and attributes. For those who aren't skilled at Python, here's a suggestion:

		table = [('/dir1', 0o040755, 1024, 0x50000190),
				 ('/file.A, 0o100777, 1000, 0x50000190)]
		for path,mode,size,ctime in table:
			v,sb = hw3.getattr(path)
			self.assertTrue(sb.mode == mode)
			...

In addition you should test the various possible path translation errors - a good list to test is:
* ENOENT - "/not-a-file"
* ENOTDIR - "/file.A/file.0"
* ENOENT on a middle part of the path - "/not-a-dir/file.0"
* ENOENT in a subdirectory "/dir1/not-a-file"

In each case make sure that the first return value from `hw3.getattr` is `-hw3.ENOENT` or `-hw3.ENOTDIR` as appropriate.

Testing read()
--------------

**Single big read:** The simplest test is to read the entire file in a single operation and make sure that it matches the correct value:

		def test_3_read(self):
			data = 'K' * 276177 # see description above
			v,result = hw3.read("/dir1/file.270", len(data)+100, 0)  # offset=0
			self.assertTrue(v == len(data))
			self.assertTrue(result == data)

Note that I'm asking it to read a few more bytes than the actual size of the file, just in case there's a bug in dealing with end-of-file. To actually implement this, remember that you're dealing with programmable machines, and you can create a table of all the cases to iterate through.

**Multiple smaller reads:** Next you should write a function that reads a file in pieces of *n* bytes (remember to update the offset after reading each one; if you're not familiar with Python, the string concatenation operator is `+`), and verify that you get the correct results for different values of *n*. (the grading test uses n=17, 100, 1000, 1024, 1970, and 3000)

Testing readdir()
-----------------

Here's a Python trick for testing `readdir`, using a [list comprehension][2], the [set][3]  data type, and the fact that Python `==` does a deep comparison

		v,dirents = hw3.readdir(path)
		names = set([d.name for d in dirents])
		self.assertTrue(names == set(('file.A', 'dir1', 'file.7')))

[2]: https://docs.python.org/2.7/tutorial/datastructures.html#list-comprehensions
[3]: https://docs.python.org/2.7/library/stdtypes.html#set

You can also test the `stat` results returned from readdir - if `getattr` is already working, the easiest way is probably to use it to retrieve the correct values. (you might want to compare field-by-field here, as the structure returned from `hw3.readdir()` contains some unused fields that might have different values.)
