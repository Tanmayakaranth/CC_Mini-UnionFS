#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

extern int resolve_path(const char *path, char *resolved);
extern void build_path(char *dest, const char *dir, const char *path);

struct mini_unionfs_state {
    char *lower_dir;
    char *upper_dir;
};

#define UNIONFS_DATA ((struct mini_unionfs_state *) fuse_get_context()->private_data)

/* ---------------- COPY FUNCTION ---------------- */
int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in) return -errno;

    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -errno;
    }

    char buf[4096];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);

int unionfs_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}    fclose(in);
    fclose(out);
    return 0;
}

/* ---------------- OPEN (CoW LOGIC) ---------------- */
int unionfs_open(const char *path, struct fuse_file_info *fi)
{
    char resolved[PATH_MAX];
    int res = resolve_path(path, resolved);

    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[PATH_MAX];
    char lower_path[PATH_MAX];

    build_path(upper_path, state->upper_dir, path);
    build_path(lower_path, state->lower_dir, path);

    /* If writing */
    if ((fi->flags & O_WRONLY) || (fi->flags & O_RDWR)) {

        struct stat st_upper, st_lower;

        int upper_exists = (lstat(upper_path, &st_upper) == 0);
        int lower_exists = (lstat(lower_path, &st_lower) == 0);

        /* File only in lower → Copy-on-Write */
        if (!upper_exists && lower_exists) {
            int cpy = copy_file(lower_path, upper_path);
            if (cpy < 0)
                return cpy;
        }
    }

    return 0;
}

/* ---------------- WRITE ---------------- */
int unionfs_write(const char *path, const char *buf, size_t size,
                  off_t offset, struct fuse_file_info *fi)
{
    (void) fi;

    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[PATH_MAX];
    char lower_path[PATH_MAX];

    build_path(upper_path, state->upper_dir, path);
    build_path(lower_path, state->lower_dir, path);

    struct stat st_upper, st_lower;

    int upper_exists = (lstat(upper_path, &st_upper) == 0);
    int lower_exists = (lstat(lower_path, &st_lower) == 0);

    /* 🔥 FORCE COPY BEFORE ANY WRITE */
    if (!upper_exists && lower_exists) {
        FILE *in = fopen(lower_path, "rb");
        FILE *out = fopen(upper_path, "wb");

        if (!in || !out) {
            if (in) fclose(in);
            if (out) fclose(out);
            return -errno;
        }

        char buffer[4096];
        size_t n;

        while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0)
            fwrite(buffer, 1, n, out);

        fclose(in);
        fclose(out);
    }

    /* 🔥 ALWAYS WRITE ONLY TO UPPER */
    int fd = open(upper_path, O_WRONLY | O_CREAT, 0644);
    if (fd < 0)
        return -errno;

    ssize_t res = pwrite(fd, buf, size, offset);

    close(fd);

    return res;
}

/* ---------------- CREATE ---------------- */
int unionfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void) fi;

    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[PATH_MAX];
    build_path(upper_path, state->upper_dir, path);

    FILE *fp = fopen(upper_path, "wb");

    if (!fp)
        return -errno;

    fclose(fp);

    chmod(upper_path, mode);

    return 0;
}

/* ---------------- MKDIR ---------------- */
int unionfs_mkdir(const char *path, mode_t mode)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[PATH_MAX];
    build_path(upper_path, state->upper_dir, path);

    if (mkdir(upper_path, mode) == -1)
        return -errno;

    return 0;
}
