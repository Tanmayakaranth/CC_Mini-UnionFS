#include <dirent.h>
#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

int unionfs_open(const char *, struct fuse_file_info *);
int unionfs_write(const char *, const char *, size_t, off_t, struct fuse_file_info *);
int unionfs_create(const char *, mode_t, struct fuse_file_info *);
int unionfs_mkdir(const char *, mode_t);

struct mini_unionfs_state {
    char *lower_dir;
    char *upper_dir;
};

#define UNIONFS_DATA ((struct mini_unionfs_state *) fuse_get_context()->private_data)
// shortcut to get the filesystem's global configuration (lower_dir and upper_dir)
// i.e, it can access both the upper and lower dir when a new function is defined


void build_path(char *dest, const char *dir, const char *path)
{
    strcpy(dest, dir);
    strcat(dest, path);
    //dest = dir+path
//function used to access upper/lower path file
}

/* Build whiteout path: upper_dir/.wh.filename */
void build_whiteout_path(char *dest, const char *upper, const char *path)
{
    char *filename = strrchr(path, '/');

    if (filename)
        snprintf(dest, PATH_MAX, "%s/.wh.%s", upper, filename + 1);
}

//to check if a file exists
int resolve_path(const char *path, char *resolved)
{
    if (strcmp(path, "/") == 0) {
        strcpy(resolved, UNIONFS_DATA->upper_dir);
        return 0;
    }
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[PATH_MAX];
    char lower_path[PATH_MAX];
    char whiteout_path[PATH_MAX];

    struct stat st;

    build_path(upper_path, state->upper_dir, path);
    build_path(lower_path, state->lower_dir, path);
    build_whiteout_path(whiteout_path, state->upper_dir, path);
	
	//should be in layerwise order -- whiteout, then upper then lower
    /* 1. Check whiteout */
    if (lstat(whiteout_path, &st) == 0)
        return -ENOENT;

    /* 2. Check upper layer */
    if (lstat(upper_path, &st) == 0) {
        strcpy(resolved, upper_path);
        return 0;
    }

    /* 3. Check lower layer */
    if (lstat(lower_path, &st) == 0) {
        strcpy(resolved, lower_path);
        return 0;
    }

    return -ENOENT;
}


//getattr to mount FUSE (temp function)
static int unionfs_getattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info *fi)
{
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    /* Root directory */
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    char resolved[PATH_MAX];
    int res = resolve_path(path, resolved);

    if (res < 0)
        return res;

    if (lstat(resolved, stbuf) == -1)
        return -errno;

    return 0;
}

static int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi,
                           enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;

    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[PATH_MAX];
    char lower_path[PATH_MAX];

    build_path(upper_path, state->upper_dir, path);
    build_path(lower_path, state->lower_dir, path);

    DIR *dp;
    struct dirent *de;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    dp = opendir(upper_path);
    if (dp) {
        while ((de = readdir(dp)) != NULL) {

            if (strncmp(de->d_name, ".wh.", 4) == 0)
                continue;

            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    dp = opendir(lower_path);
    if (dp) {
        while ((de = readdir(dp)) != NULL) {

            char whiteout[PATH_MAX];
            snprintf(whiteout, PATH_MAX, "%s/.wh.%s", upper_path, de->d_name);

            struct stat st;

            if (lstat(whiteout, &st) == 0)
                continue;

            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    return 0;
}

static int unionfs_read(const char *path, char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi)
{
    (void) fi;

    char resolved[PATH_MAX];
    int res = resolve_path(path, resolved);

    if (res < 0)
        return res;

    FILE *fp = fopen(resolved, "rb");

    if (!fp)
        return -errno;

    fseek(fp, offset, SEEK_SET);

    size_t bytes = fread(buf, 1, size, fp);

    fclose(fp);

    return bytes;
}


static struct fuse_operations unionfs_oper = {
    .getattr = unionfs_getattr,
    .readdir = unionfs_readdir,
    .read = unionfs_read,

    .open = unionfs_open,
    .write = unionfs_write,
    .create = unionfs_create,
    .mkdir = unionfs_mkdir,

};


int main(int argc, char *argv[])
{
    if (argc < 4) {
        printf("Usage: %s <lower_dir> <upper_dir> <mount_point>\n", argv[0]);
        return 1;
    }

    struct mini_unionfs_state *state = malloc(sizeof(struct mini_unionfs_state));
    state->lower_dir = realpath(argv[1], NULL);
    state->upper_dir = realpath(argv[2], NULL);

    argv[1] = argv[3];
    argc = 2;

    return fuse_main(argc, argv, &unionfs_oper, state);
}
