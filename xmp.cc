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
#include <vector>
#include <string>
#include <sys/xattr.h>
#include <unordered_map>

// for remote download
#include "proxy.h"
#include "library.h"

// for json works
#include "include/rapidjson/document.h"
#include "include/rapidjson/filereadstream.h"
#include <cstdio>

// for config
#include "config.h"

/********************************protocol********************************************/
std::string remove_prefix(const char *input, const std::string &prefix);

bool has_prefix(const char* str, const char* prefix);

/************************************************************************************/

/********************************varible*********************************************/
// key is path, value is type
static std::unordered_map<std::string, std::string> mp;

static Config *config;
/************************************************************************************/

void full_path(char fpath[1000], const char *path)
{
    strcpy(fpath, config->rw_dir_.c_str()); // Assuming dir_path is the path to the base directory in the underlying file system
    if (strcmp(path, "/") != 0)
        strncat(fpath, path, 1000 - strlen(fpath) - 1); // Append path unless it's the root
}

static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    int res;
    char fpath[1000];
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    full_path(fpath, path);
    printf("[DEBUG] getattr called on %s\n", path);
    res = lstat(fpath, stbuf);

    if (res == -1 && errno == ENOENT)
    {
        std::string file_name = remove_prefix(path, "/upperdir");
        // 确保下层中没有这样的文件，当且仅当upperdir与lowerdirs中均不存在所需的文件，且该文件不为白化文件，
        // 才进行拉取
        if (!has_prefix(file_name.c_str(), ".wh.") && mp.find(file_name) == mp.end())
        {
            if (download_file(config->address_, config->image_, path, fpath) == 0)
            {
                res = lstat(fpath, stbuf);
                return (res == -1) ? -errno : 0;
            }
        }
        return -ENOENT;
    }

    if (res == -1)
        return -errno;

    // 检查所访问对象是否为可执行文件，如果是可执行文件，则分析其所需的动态库
    if (S_ISREG(stbuf->st_mode) && ((stbuf->st_mode & S_IXUSR) || (stbuf->st_mode & S_IXGRP) || (stbuf->st_mode & S_IXOTH)))
    {
        std::cout << "[DEBUG] is executable file " << path << std::endl;

        std::vector<std::string> libs;
        int lib_res = analyze_executable_libraries(fpath, libs);
        if (lib_res == -1)
        {
            std::cout << "[DEBUG] executable file1 " << fpath << std::endl;
            return -errno;
        }
        for (int i = 0; i < libs.size(); ++i)
        {
            std::cout << "[DEBUG] executable file2 " << fpath << std::endl;
            char lib_path[1000];
            libs[i] = "/" + libs[i];
            full_path(lib_path, libs[i].c_str());
            struct stat *lib_stbuf;
            res = lstat(lib_path, lib_stbuf);
            if (res == -1 && errno == ENOENT)
            {
                std::cout << "[DEBUG] executable file3 " << path << std::endl;
                if (download_file(config->address_, config->image_, libs[i].c_str(), lib_path) == 0)
                {
                    std::cout << "[FATAL] fetch动态库失败 " << path << std::endl;
                    res = lstat(lib_path, lib_stbuf);
                    return (res == -1) ? -errno : 0;
                }
                return -errno;
            }
        }
    }

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
    int fd;
    char fpath[1000];
    full_path(fpath, path);

    fd = open(fpath, fi->flags);
    if (fd == -1)
        return -errno;

    if (access(fpath, F_OK) != -1)
    {
        // File exists, just return fd
        fi->fh = fd;
    }
    else
    {
        // File doesn't exist, try to download it
        std::cout << "[DEBUG] Try to download file" << fpath << std::endl;
        if (download_file(config->address_, config->image_, path, fpath) == 0)
        {
            std::cout << "[DEBUG] Download success" << fpath << std::endl;
            fd = open(fpath, fi->flags);
            fi->fh = fd;
        }
        else
        {
            std::cout << "[DEBUG] Download failed" << fpath << std::endl;
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
    (void)fi;
    std::cout << "[DEBUG] Try to read " << fpath << std::endl;
    if (strstr(path, ".so"))
    {
        std::cout << "Dynamic library access attempt: " << path << std::endl;
    }
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
    (void)fi;
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
    (void)offset;
    (void)fi;
    (void)flags;

    dp = opendir(fpath);
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL)
    {
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

static int xmp_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    char fpath[1000];
    full_path(fpath, path);
    int res = lsetxattr(fpath, name, value, size, flags);
    if (res == -1)
        return -errno;
    return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value, size_t size)
{
    char fpath[1000];
    full_path(fpath, path);
    int res = lgetxattr(fpath, name, value, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
    char fpath[1000];
    full_path(fpath, path);
    int res = llistxattr(fpath, list, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
    char fpath[1000];
    full_path(fpath, path);
    int res = lremovexattr(fpath, name);
    if (res == -1)
        return -errno;
    return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
    char fpath[1000];
    full_path(fpath, to);
    int res = symlink(from, fpath);
    if (res == -1)
        return -errno;
    return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
    char fpath[1000];
    full_path(fpath, path);
    int res = readlink(fpath, buf, size - 1);
    if (res == -1)
        return -errno;
    buf[res] = '\0'; // Null-terminate the string
    return 0;
}

static int xmp_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
    (void)path;
    int res;
    if (isdatasync)
        res = fdatasync(fi->fh);
    else
        res = fsync(fi->fh);
    if (res == -1)
        return -errno;
    return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
    (void)path;
    close(fi->fh);
    return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
    char fpath[1000];
    full_path(fpath, path);
    int res;

    // 根据 mode 参数判断文件类型
    if (S_ISREG(mode))
    {
        // 创建普通文件
        res = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0)
            res = close(res);
    }
    else if (S_ISFIFO(mode))
    {
        // 创建管道
        res = mkfifo(fpath, mode);
    }
    else
    {
        // 创建设备文件
        res = mknod(fpath, mode, rdev);
    }
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_truncate(const char *path, off_t size, struct fuse_file_info *fi)
{
    char fpath[1000];
    full_path(fpath, path);
    int res;

    if (fi != NULL)
        res = ftruncate(fi->fh, size);
    else
        res = truncate(fpath, size);

    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_chmod(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    char fpath[1000];
    full_path(fpath, path);
    int res;

    res = chmod(fpath, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi)
{
    char fpath[1000];
    full_path(fpath, path);
    int res;

    res = chown(fpath, uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi)
{
    char fpath[1000];
    full_path(fpath, path);
    int res;

    res = utimensat(0, fpath, ts, AT_SYMLINK_NOFOLLOW);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    char fpath[1000];
    int fd;

    // 正常创建文件
    full_path(fpath, path);
    fd = open(fpath, fi->flags, mode);

    if (fd == -1)
    {
        return -errno;
    }

    fi->fh = fd;
    return 0;
}

static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readlink = xmp_readlink,
    .mknod = xmp_mknod,
    .mkdir = xmp_mkdir,
    .unlink = xmp_unlink,
    .rmdir = xmp_rmdir,
    .symlink = xmp_symlink,
    .rename = xmp_rename,
    .chmod = xmp_chmod,
    .chown = xmp_chown,
    .truncate = xmp_truncate,

    .open = xmp_open,
    .read = xmp_read,
    .write = xmp_write,
    .release = xmp_release,
    .fsync = xmp_fsync,
    .setxattr = xmp_setxattr,
    .getxattr = xmp_getxattr,
    .listxattr = xmp_listxattr,
    .removexattr = xmp_removexattr,
    .readdir = xmp_readdir,
    .access = xmp_access,
    .create = xmp_create,
    .utimens = xmp_utimens,
};

void xmp_init()
{
    // 从镜像的JSON元信息中读取文件信息，并放至内存中
    FILE *fp = fopen("./files.json", "r"); // 非 Windows 平台使用 "r"

    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

    rapidjson::Document d;
    d.ParseStream(is);

    assert(d.IsArray());
    for (rapidjson::SizeType i = 0; i < d.Size(); i++)
    {
        if (d[i].HasMember("path") && d[i].HasMember("type") && d[i]["path"].IsString())
        {
            mp.insert(std::pair<std::string, std::string>(d[i]["path"].GetString(), d[i]["type"].GetString()));
            // printf("a[%u] = %s\n", i, d[i]["path"].GetString());
        }
    }

    // std::cout << "[DEBUG] mp.size" << mp.size() << std::endl;

    // 从文件系统的JSON配置信息中读取文件信息，并放至内存中
    FILE *config_fp = fopen("./config.json", "r"); // 非 Windows 平台使用 "r"

    char configBuffer[65536];
    rapidjson::FileReadStream config_is(config_fp, configBuffer, sizeof(configBuffer));

    rapidjson::Document config_docu;
    config_docu.ParseStream(config_is);

    const rapidjson::Value &protocol = config_docu["Protocol"];
    const rapidjson::Value &address = config_docu["Address"];
    const rapidjson::Value &user_name = config_docu["Username"];
    const rapidjson::Value &password = config_docu["Password"];
    const rapidjson::Value &rw_dir = config_docu["RWDir"];
    const rapidjson::Value &image = config_docu["Image"];

    if (protocol.IsString() && address.IsString() && user_name.IsString() &&
            password.IsString() && rw_dir.IsString() && image.IsString()) {
                config = new Config(protocol.GetString(),address.GetString(),user_name.GetString(),
                password.GetString(),rw_dir.GetString(),image.GetString());
    }
    assert(config != nullptr);

    // std::cout << "[DEBUG] image " << config->image_ << std::endl; 
    
}

static void parse_args(int &argc, char *argv[], std::vector<char*>& fuse_argv) {
    // Include the program name as the first argument for FUSE
    fuse_argv.push_back(argv[0]);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            // Handle the custom '-r' argument
            config->rw_dir_ = argv[i + 1];
            i++;  // Skip the next argument since it's the directory after '-r'
        } else {
            // These are potentially FUSE options or other valid parameters
            fuse_argv.push_back(argv[i]);
        }
    }
}
int main(int argc, char *argv[])
{
    std::vector<char*> fuse_argv;
    // use config default
    xmp_init();
    // if program has args, use agrs
    parse_args(argc, argv, fuse_argv);

    std::cout << "RW Directory: " << config->rw_dir_ << std::endl;

    // Convert vector back to array for fuse_main
    int fuse_argc = static_cast<int>(fuse_argv.size());

    for(int i = 0; i < fuse_argc; ++i) {
        std::cout << fuse_argv[i] << std::endl;
    }

    return fuse_main(fuse_argc, fuse_argv.data(), &xmp_oper, NULL);
}
