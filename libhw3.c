/*
 */
#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

extern struct fuse_operations fs_ops;
int homework_part;

/* fuse context - global so we can reinitialize each time we call init.
 */
struct fuse_context ctx;

struct fuse_context *fuse_get_context(void)
{
    return &ctx;
}

extern void block_init(char *name);

void *hw3_init(char *image)
{
    block_init(image);
    memset(&ctx, 0, sizeof(ctx));
    return fs_ops.init(NULL);
}

int hw3_getattr(const char *path, struct stat *sb)
{
    return fs_ops.getattr(path, sb);
}

struct dirent {
    char name[64];
    struct stat sb;
};
struct dir_state {
    int max;
    int i;
    struct dirent *de;
};
    
static int filler(void *buf, const char *name, const struct stat *sb, off_t off)
{
    struct dir_state *d = buf;
    if (d->i >= d->max)
        return -ENOMEM;
    strncpy(d->de[d->i].name, name, 64);
    d->de[d->i].sb = *sb;
    d->i++;
    return 0;
}

/* returns number of directory entries, or error (<0) 
 */
int hw3_readdir(const char *path, int n, struct dirent *de, struct fuse_file_info *fi)
{
    struct dir_state ds = {.max = n, .i = 0, .de = de};
    int val = fs_ops.readdir(path, &ds, filler, 0, fi);
    if (val < 0)
        return val;
    return ds.i;
}
    
/* ignores dev_t */
int hw3_create(const char *path, unsigned int mode, struct fuse_file_info *fi)
{
    return fs_ops.create(path, mode, fi);
}

int hw3_mkdir(const char *path, unsigned int mode)
{
    return fs_ops.mkdir(path, mode);
}

int hw3_truncate(const char *path, unsigned int len)
{
    return fs_ops.truncate(path, len);
}

int hw3_unlink(const char *path)
{
    return fs_ops.unlink(path);
}

int hw3_rmdir(const char *path)
{
    return fs_ops.rmdir(path);
}

int hw3_rename(const char *path1, const char *path2)
{
    return fs_ops.rename(path1, path2);
}

int hw3_chmod(const char *path, unsigned int mode)
{
    return fs_ops.chmod(path, mode);
}

int hw3_utime(const char *path, unsigned int actime, unsigned int modtime)
{
    struct utimbuf u = {.actime = actime, .modtime = modtime};
    return fs_ops.utime(path, &u);
}

int hw3_read(const char *path, char *buf, unsigned int len, unsigned int offset,
             struct fuse_file_info *fi)
{
    return fs_ops.read(path, buf, len, offset, fi);
}

int hw3_write(const char *path, const char *buf, unsigned int  len,
	      unsigned int offset, struct fuse_file_info *fi)
{
    return fs_ops.write(path, buf, len, offset, fi);
}

int hw3_statfs(const char *path, struct statvfs *st)
{
    return fs_ops.statfs(path, st);
}

