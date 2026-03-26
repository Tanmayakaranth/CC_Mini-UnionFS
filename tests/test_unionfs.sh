#!/bin/bash

FUSE_BINARY="../unionfs"
TEST_DIR="../unionfs_test_env"
LOWER_DIR="$TEST_DIR/lower"
UPPER_DIR="$TEST_DIR/upper"
MOUNT_DIR="$TEST_DIR/mnt"

echo "Starting Mini-UnionFS Test Suite..."

# Clean previous runs
fusermount3 -u "$MOUNT_DIR" 2>/dev/null
rm -rf "$TEST_DIR"

mkdir -p "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR"

echo "base_only_content" > "$LOWER_DIR/base.txt"
echo "to_be_deleted" > "$LOWER_DIR/delete_me.txt"

# Run filesystem in background
$FUSE_BINARY "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR" &
sleep 2

# ---------------- TEST 1 ----------------
echo -n "Test 1: Layer Visibility... "
if grep -q "base_only_content" "$MOUNT_DIR/base.txt"; then
    echo "PASSED"
else
    echo "FAILED"
fi

# ---------------- TEST 2 ----------------
echo -n "Test 2: Copy-on-Write... "
echo "modified_content" >> "$MOUNT_DIR/base.txt" 2>/dev/null

A=$(grep -c "modified_content" "$MOUNT_DIR/base.txt" 2>/dev/null)
B=$(grep -c "modified_content" "$UPPER_DIR/base.txt" 2>/dev/null)
C=$(grep -c "modified_content" "$LOWER_DIR/base.txt" 2>/dev/null)

if [ "$A" -eq 1 ] && [ "$B" -eq 1 ] && [ "$C" -eq 0 ]; then
    echo "PASSED"
else
    echo "FAILED"
fi

# ---------------- TEST 3 ----------------
echo -n "Test 3: Whiteout mechanism... "
rm "$MOUNT_DIR/delete_me.txt" 2>/dev/null

if [ ! -f "$MOUNT_DIR/delete_me.txt" ] && \
   [ -f "$LOWER_DIR/delete_me.txt" ] && \
   [ -f "$UPPER_DIR/.wh.delete_me.txt" ]; then
    echo "PASSED"
else
    echo "FAILED"
fi

# Cleanup
fusermount3 -u "$MOUNT_DIR" 2>/dev/null
rm -rf "$TEST_DIR"

echo "Test Suite Completed."