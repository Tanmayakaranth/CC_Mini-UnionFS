# CC_Mini-UnionFS
# Mini-UnionFS

A simplified Union File System implementation using FUSE (Filesystem in Userspace) for Linux.

## Overview

Mini-UnionFS stacks multiple directories (branches) into a single unified virtual file system. It merges a read-only lower layer (base) with a read-write upper layer (container), similar to how Docker containers work.

## Features

- **Layer Stacking**: Merge two directories into one virtual filesystem
- **Copy-on-Write (CoW)**: Automatically copy modified files from lower to upper layer
- **Whiteout Support**: Delete files from lower layer using whiteout files
- **Basic File Operations**: read, write, create, mkdir, and file deletion

## Requirements

- Linux environment (Ubuntu 22.04 LTS or similar)
- FUSE 3.x development libraries
- GCC compiler
- C Standard Library

### Installation

```bash
# Ubuntu/Debian
sudo apt-get install libfuse3-dev fuse3 build-essential
```

## Project Structure

```
.
├── tests/test_unionfs.c
├── delete_ops.c
├── mini_unionfs.c       # Core FUSE implementation (read operations)
├── write_ops.c          # Write operations (CoW, create, mkdir, unlink)
├── Makefile             # Build configuration
└── README.md            # This file
```

## Building

```bash
make
```

This generates the `mini_unionfs` binary.

## Usage

```bash
./mini_unionfs <lower_dir> <upper_dir> <mount_point>
```

### Example

```bash
mkdir -p lower upper mnt
echo "base content" > lower/file.txt

./mini_unionfs ./lower ./upper ./mnt

# Now 'mnt' shows merged view of lower and upper
cat mnt/file.txt

# Modify a file
echo "new content" >> mnt/file.txt  # Copies to upper, original lower unchanged
```

### Unmounting

```bash
fusermount -u <mount_point>
# or
sudo umount <mount_point>
```

## How It Works

### Layer Resolution

Files are resolved in this order:
1. **Whiteout Check**: Is there a `.wh.<filename>` in upper? → File is deleted
2. **Upper Layer**: Does the file exist in upper_dir? → Use it
3. **Lower Layer**: Does the file exist in lower_dir? → Use it
4. **Not Found**: Return error

### Copy-on-Write (CoW)

When writing to a file that exists only in the lower layer:
1. Copy the entire file to upper_dir
2. Apply modifications to the upper copy
3. Leave lower_dir untouched

### Whiteout Mechanism

Deleting a file from the lower layer:
1. Create a hidden `.wh.<filename>` file in upper_dir
2. This acts as a deletion marker
3. The original file remains in lower_dir but is hidden from users

## Code Overview

### mini_unionfs.c

**Key Functions:**

- `build_path()`: Constructs full path by combining directory + relative path
- `build_whiteout_path()`: Creates whiteout file path (e.g., `.wh.config.txt`)
- `resolve_path()`: Finds where a file actually exists (checks whiteout → upper → lower)
- `unionfs_getattr()`: Returns file attributes and metadata
- `unionfs_readdir()`: Lists merged directory contents
- `unionfs_read()`: Reads file data

### write_ops.c

**Key Functions:**

- `copy_file()`: Copies file from lower to upper
- `unionfs_open()`: Triggers CoW when opening files for writing
- `unionfs_write()`: Writes data to upper layer
- `unionfs_create()`: Creates new files in upper layer
- `unionfs_mkdir()`: Creates directories in upper layer

## Data Structures

```c
struct mini_unionfs_state {
    char *lower_dir;    // Read-only base layer
    char *upper_dir;    // Read-write container layer
};
```

Accessed globally via `UNIONFS_DATA` macro.

## Testing

Run the included test suite:

```bash
bash test_unionfs.sh
```

Tests verify:
1. **Layer Visibility**: Files from lower layer are visible
2. **Copy-on-Write**: Modified files are copied to upper layer
3. **Whiteout**: Deleted files create proper markers

## Supported Operations

| Operation | Status | Notes |
|-----------|--------|-------|
| Read files | ✓ | From any layer |
| List directories | ✓ | Merged view |
| Create files | ✓ | In upper layer |
| Write files | ✓ | With CoW |
| Create directories | ✓ | In upper layer |
| Delete files | ✓ | Via whiteout |

## Limitations

- No directory deletion (`rmdir`) yet
- No permission management
- No symlink support
- Single upper/lower layer only

## Troubleshooting

**Mount fails with "Operation not permitted"**
- Ensure FUSE is installed and loaded
- Check directory permissions

**Files not appearing after creation**
- Verify upper_dir has write permissions
- Check if file was written to correct layer

**Unmount fails**
- Kill any processes accessing the mount point first
- Use `fusermount -u` instead of `umount`

## References

- [FUSE Documentation](https://github.com/libfuse/libfuse)
- [Union Filesystems Overview](https://en.wikipedia.org/wiki/Union_mount)
- [Docker Storage Drivers](https://docs.docker.com/storage/storagedriver/)

## Author

College Project - Cloud Computing

## License

Educational purposes only
