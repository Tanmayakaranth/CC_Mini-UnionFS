#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

/* Use helpers from other files */
extern void build_path(char *dest, const char *dir, const char *path);
extern void build_whiteout_path(char *dest, const char *upper, const char *path);

/* Shared state */
struct mini_unionfs_state {
    char *lower_dir;
    char *upper_dir;
};

#define UNIONFS_DATA ((struct mini_unionfs_state *) fuse_get_context()->private_data)

/* ---------------- UNLINK (Delete File) ---------------- */
int unionfs_unlink(const char *path)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[PATH_MAX];
    char lower_path[PATH_MAX];
    char whiteout_path[PATH_MAX];

    struct stat st;

    build_path(upper_path, state->upper_dir, path);
    build_path(lower_path, state->lower_dir, path);
    build_whiteout_path(whiteout_path, state->upper_dir, path);

    /* If file exists in upper → delete it */
    if (lstat(upper_path, &st) == 0) {
        if (unlink(upper_path) == -1)
            return -errno;
    }

    /* If file exists in lower → create whiteout */
    if (lstat(lower_path, &st) == 0) {
        FILE *fp = fopen(whiteout_path, "w");
        if (!fp)
            return -errno;

        fclose(fp);
    }

    return 0;
}

/* ---------------- RMDIR (Delete Directory) ---------------- */
int unionfs_rmdir(const char *path)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[PATH_MAX];
    char lower_path[PATH_MAX];
    char whiteout_path[PATH_MAX];

    struct stat st;

    build_path(upper_path, state->upper_dir, path);
    build_path(lower_path, state->lower_dir, path);
    build_whiteout_path(whiteout_path, state->upper_dir, path);

    /* If directory exists in upper → remove it */
    if (lstat(upper_path, &st) == 0) {
        if (rmdir(upper_path) == -1)
            return -errno;
    }

    /* If directory exists in lower → create whiteout */
    if (lstat(lower_path, &st) == 0) {
        FILE *fp = fopen(whiteout_path, "w");
        if (!fp)
            return -errno;

        fclose(fp);
    }

    return 0;
}