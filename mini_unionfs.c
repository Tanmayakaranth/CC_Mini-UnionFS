#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

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
        sprintf(dest, "%s/.wh.%s", upper, filename + 1);
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

static struct fuse_operations unionfs_oper = {
    .getattr = unionfs_getattr,
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
