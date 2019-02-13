#!usr/bin/python
import unittest
import hw3test as hw3

class test(unittest.TestCase):
	def test_1_statfs(self):
		print "Test 1 - statfs:"
		v, sfs = hw3.statfs()
		self.assertTrue(v == 0)
		self.assertTrue(sfs.f_bsize == 1024)
		self.assertTrue(sfs.f_blocks == 1024)
		self.assertTrue(sfs.f_bfree == 731)
		self.assertTrue(sfs.f_namemax == 27)

	def test_2_read(self):
		print "Test 2 - getattr:"
		table = [('/dir1', 0o040755, 1024, 0x50000190),
				('/file.A', 0o100777, 1000, 0x500000C8)]
		for path, mode, size, ctime in table:
			v, sb = hw3.getattr(path)
			self.assertTrue(v == 0)
			self.assertTrue(sb.st_mode == mode)
			self.assertTrue(sb.st_size == size)
			self.assertTrue(sb.st_ctime== ctime)

		v, sb = hw3.getattr('/not-a-file')
		self.assertTrue(v == -hw3.ENOENT)

		v, sb = hw3.getattr('/file.A/file.0')
		self.assertTrue(v == -hw3.ENOTDIR)

	def test_3_read(self):
		print "Test 3a - read:"
		data = 'K' * 276177 # see description above
		v,result = hw3.read("/dir1/file.270", len(data)+100, 0)  # offset=0
		self.assertTrue(v == len(data))
		self.assertTrue(result == data)
		print "Test 3b - read in small pieces:"
		result = ""
		i = 0
		while i < 276177:
			v, temp = hw3.read("/dir1/file.270", 1024, i) 
			i += 1024
			result += temp
		self.assertTrue(result == data)

	def test_4_readdir(self):
		print "Test 4 - readdir:"
		v,dirents = hw3.readdir("/")
		names = set([d.name for d in dirents])
		self.assertTrue(names == set(('file.A', 'dir1', 'file.7')))

if __name__ == '__main__':
	hw3.init('test.img')
unittest.main()
