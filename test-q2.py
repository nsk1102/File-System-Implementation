#!usr/bin/python
import unittest
import hw3test as hw3
import os

class test(unittest.TestCase):
	def fsck(self):
		v = os.system('./fsck.hw3 test.img > /dev/null')
		if v != 0:
			print 'ERROR: fsck failed:'
			os.system('./fsck.hw3 test.img')
			self.assertTrue(False)

	def test_1_mkdir(self):
		print "Test 1 - mkdir:"
		table = [('/dir1', 0o000755),
				('/dir1/dir2', 0o000755)]
		for path, mode in table:
			v = hw3.mkdir(path, mode);
			self.assertTrue(v == 0)
			v, sb = hw3.getattr(path)
			self.assertTrue(v == 0)
			self.assertTrue(sb.st_mode == hw3.S_IFDIR | mode)
			v,dirents = hw3.readdir(path)
			self.assertTrue(v == 0)
			self.assertTrue(len(dirents) == 0)

	def test_2_create(self):
		print "Test 2a - create:"
		table = [('/file.B', 0o100777),
				('/dir1/file.A', 0o100777)]
		for path, mode in table:
			v = hw3.create(path, mode);
			self.assertTrue(v == 0)
			v, sb = hw3.getattr(path)
			self.assertTrue(v == 0)
			self.assertTrue(sb.st_mode == hw3.S_IFREG | mode)
			self.assertTrue(sb.st_size == 0)

		print "Test 2b - create wired filename:"
		hw3.create('/thisisatwenty-eight-byte-name', 0o100777)
		v, dirents = hw3.readdir('/')
		for item in dirents:
			print item.name
		self.fsck()
		
	def test_3_create_mkdir_fail(self):
		table = [('directory-doesnt-exist/name', 0o100777, -hw3.ENOENT),
			('file.B/name', 0o100777, -hw3.ENOTDIR),
			('dir1', 0o000755, -hw3.EEXIST), 
			('file.B', 0o100777, -hw3.EEXIST)]
		print "Test 3 - mkdir/create, bad pathnames:"
		for path, mode, error in table:
			v = hw3.create(path, mode)
			self.assertTrue(v == error)
			v = hw3.mkdir(path, mode)
			self.assertTrue(v == error)
		self.fsck()

	def test_4_chmod(self):
		print "Test 4 - chmod:"
		table = [('/file.B', 0o000755),
				('/dir1', 0o000777)]
		for path, mode in table:
			v = hw3.chmod(path, mode);
			self.assertTrue(v == 0)
			v, sb = hw3.getattr(path)
			self.assertTrue(v == 0)
			self.assertEqual((sb.st_mode & ~hw3.S_IFMT), mode)
		self.fsck()

	def test_5_utime(self):
		print "Test 5 - utime:"
		table = [('/file.B', 0x500000C8),
				('/dir1/file.A', 0x500000C8)]
		for path, time in table:
			v = hw3.utime(path, time, time);
			self.assertTrue(v == 0)
			v, sb = hw3.getattr(path)
			self.assertTrue(v == 0)
			self.assertTrue(sb.st_mtime == time)
		self.fsck()

	def test_6_write(self):
		print "Test 6a - write:"
		data = 'K' * 289150
		v = hw3.write("/dir1/file.A", data, 0)  # offset=0
		self.assertTrue(v == len(data))
		v,result = hw3.read("/dir1/file.A", len(data)+100, 0)  # offset=0
		self.assertTrue(v == len(data))
		self.assertTrue(result == data)
		print "Test 6b - write in small pieces:"
		data = "B" * 1000
		i = 0
		while i < 280:
			v = hw3.write("/dir1/file.A", data, i * 1000) 
			i += 1
		v,result = hw3.read("/dir1/file.A", 280000, 0)
		self.assertTrue(result == "B" * 280000)
		self.fsck()

	def test_7_truncate(self):
		print "Test 7 - truncate:"
		v, sfs = hw3.statfs()
		f_bfree = sfs.f_bfree
		hw3.create('/to_be_truncate', 0o100777)
		hw3.write('/to_be_truncate', 10000 * 'K', 0)
		v = hw3.truncate("/to_be_truncate", 0)
		self.assertTrue(v == 0)
		v, sb = hw3.getattr("/to_be_truncate")
		self.assertTrue(v == 0)
		self.assertTrue(sb.st_size == 0)
		v, sfs = hw3.statfs()
		self.assertTrue(sfs.f_bfree == f_bfree)
		self.fsck()

	def test_8_unlink(self):
		print "Test 8a - unlink:"
		v, sfs = hw3.statfs()
		f_bfree = sfs.f_bfree
		path = 'to_be_delete'
		hw3.create(path, 0o100777)
		hw3.write(path, 10000 * 'K', 0)
		v = hw3.unlink(path)
		self.assertTrue(v == 0)
		v, sb = hw3.getattr(path)
		self.assertTrue(v == -hw3.ENOENT)
		v, sfs = hw3.statfs()
		self.assertTrue(sfs.f_bfree == f_bfree)

		print "Test 8b - cannot unlink a directory:"

		v = hw3.unlink('dir1/dir2')
		self.assertTrue(v == -hw3.EISDIR)
		self.fsck()

	def test_9_rmdir(self):
		print "Test 9a - rmdir:"
		v = hw3.rmdir('dir1/dir2')
		v, sb = hw3.getattr('dir1/dir2')
		self.assertTrue(v == -hw3.ENOENT)
		
		print "Test 9b - cannot rmdir a file:"
		v = hw3.rmdir('dir1/file.A')
		self.assertTrue(v == -hw3.ENOTDIR)

		print "Test 9c - cannot rmdir a non-empty directory:"
		v = hw3.rmdir('dir1')
		self.assertTrue(v == -hw3.ENOTEMPTY)
		v = hw3.unlink('dir1/file.A')
		v = hw3.rmdir('dir1')
		self.assertTrue(v == 0)
		self.fsck()

	def test_10_rename(self):
		v = hw3.mkdir('dir1', 0o000755)
		v = hw3.create('dir1/file.A', 0o100777)
		v = hw3.create('dir1/file.C', 0o100777)
		print "Test 10a - rename a file:"
		v = hw3.rename('dir1/file.A', 'dir1/file.B')
		v, sb = hw3.getattr('dir1/file.A')
		self.assertTrue(v == -hw3.ENOENT)
		v, sb = hw3.getattr('dir1/file.B')
		self.assertTrue(v == 0)
		
		print "Test 10b - rename a directory:"
		v = hw3.rename('dir1', 'dir2')
		v, sb = hw3.getattr('dir1')
		self.assertTrue(v == -hw3.ENOENT)
		v, sb = hw3.getattr('dir2')
		self.assertTrue(v == 0)

		print "Test 10c - cannot a rename to a current:"
		v = hw3.rename('dir2/file.B', 'dir2/file.C')
		self.assertTrue(v == -hw3.EEXIST)

		print "Test 10d - rename a bogus name fails:"
		v = hw3.rename('dir2/file.D', 'dir2/file.E')
		self.assertTrue(v == -hw3.ENOENT)

		self.fsck()

if __name__ == '__main__':  
	try:
		os.unlink('test.img')
	except OSError:
		pass
	os.system('./mkfs-hw3 -size 10m test.img')
	hw3.init('test.img')
	unittest.main()