#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

// for remote download
#include "proxy.h"

static const char *dir_path = "/home/yq/realdir";

void full_path(char fpath[1000], const char *path)
{
    strcpy(fpath, dir_path); // Assuming dir_path is the path to the base directory in the underlying file system
    if (strcmp(path, "/") != 0)
        strncat(fpath, path, 1000 - strlen(fpath) - 1); // Append path unless it's the root
}

static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    int res;
    char fpath[1000];
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    full_path(fpath, path);
    printf("getattr called on %s\n", path);
    res = lstat(fpath, stbuf);
    std::string full_path2 = fpath;
    if (res == -1 && errno == ENOENT) {
        if (strcmp(path, "/d.txt") == 0 || strcmp(path, "hello.so") == 0) { // 只对 d.txt 文件处理
            if (download_file(path, full_path2) == 0) {
                res = lstat(fpath, stbuf);
                return (res == -1) ? -errno : 0;
            }
        }
        return -errno;
    }
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_access(const char *path, int mask)
{
    char fpath[1000];
    full_path(fpath, path);
    int res;

    res = access(fpath, mask);
    if (res == -1)
        return -errno;

    return 0;
}


static int xmp_open(const char *path, struct fuse_file_info *fi)
{
    int res;
    char fpath[1000];
    full_path(fpath, path);
    res = open(fpath, fi->flags);
    if (res == -1)
        return -errno;

    close(res);
    printf("open called on %s\n", path);
    return 0;
}

static int do_open(const char *path, struct fuse_file_info *fi) {
    int fd;
    std::string full_path = std::string(dir_path) + path;

    fd = open(full_path.c_str(), fi->flags);
    if (fd == -1)
        return -errno;

    if (access(full_path.c_str(), F_OK) != -1) {
        // File exists
        fi->fh = fd;
    } else {
        // File doesn't exist, try to download it
        if (download_file(path, full_path) == 0) {
            fd = open(full_path.c_str(), fi->flags);
            fi->fh = fd;
        } else {
            return -ENOENT;
        }
    }

    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    int fd;
    int res;
    char fpath[1000];
    full_path(fpath, path);
    (void) fi;
    fd = open(fpath, O_RDONLY);
    if (fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    printf("read called on %s\n", path);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    int fd;
    int res;
    char fpath[1000];
    full_path(fpath, path);
    (void) fi;
    fd = open(fpath, O_WRONLY);
    if (fd == -1)
        return -errno;

    res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    printf("write called on %s\n", path);
    return res;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    DIR *dp;
    struct dirent *de;
    char fpath[1000];
    full_path(fpath, path);
    (void) offset;
    (void) fi;
    (void) flags;

    dp = opendir(fpath);
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, static_cast<fuse_fill_dir_flags>(0)))
            break;
    }

    closedir(dp);
    printf("readdir called on %s\n", path);
    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
    char fpath[1000];
    full_path(fpath, path);
    int res;

    res = mkdir(fpath, mode);
    if (res == -1)
        return -errno;

    printf("mkdir called on %s\n", path);
    return 0;
}

static int xmp_rmdir(const char *path)
{
    char fpath[1000];
    full_path(fpath, path);
    int res;

    res = rmdir(fpath);
    if (res == -1)
        return -errno;

    printf("rmdir called on %s\n", path);
    return 0;
}

static int xmp_unlink(const char *path)
{
    char fpath[1000];
    full_path(fpath, path);
    int res;

    res = unlink(fpath);
    if (res == -1)
        return -errno;

    printf("unlink called on %s\n", path);
    return 0;
}

static int xmp_rename(const char *from, const char *to, unsigned int flags)
{
    char fpath_from[1000], fpath_to[1000];
    full_path(fpath_from, from);
    full_path(fpath_to, to);
    int res;

    if (flags)
        return -EINVAL;

    res = rename(fpath_from, fpath_to);
    if (res == -1)
        return -errno;

    printf("rename called from %s to %s\n", from, to);
    return 0;
}

static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .mkdir = xmp_mkdir,
    .unlink = xmp_unlink,
    .rmdir = xmp_rmdir,
    .rename = xmp_rename,
    .open = do_open,
    .read = xmp_read,
    .write = xmp_write,
    .readdir = xmp_readdir,
    .access = xmp_access,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &xmp_oper, NULL);
}

