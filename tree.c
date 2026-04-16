// tree.c — Tree object serialization and construction
//
// PROVIDED functions: get_file_mode, tree_parse, tree_serialize
// TODO functions:     tree_from_index
//
// Binary tree format (per entry, concatenated with no separators):
//   "<mode-as-ascii-octal> <name>\0<32-byte-binary-hash>"
//
// Example single entry (conceptual):
//   "100644 hello.txt\0" followed by 32 raw bytes of SHA-256

#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "index.h"

// ─── Mode Constants ─────────────────────────────────────────────────────────

#define MODE_FILE      0100644
#define MODE_EXEC      0100755
#define MODE_DIR       0040000

// tree structure
typedef struct TreeNode {
    char name[256];
    ObjectID id;
    uint32_t mode;
    int is_dir;

    struct TreeNode *children[128];
    int child_count;
} TreeNode;

static TreeNode *create_node(const char *name) 
{
    TreeNode *n = calloc(1, sizeof(TreeNode));
    strcpy(n->name, name);
    n->child_count = 0;
    n->is_dir = 1;
    return n;
}

static TreeNode *find_or_create(TreeNode *root, const char *name) 
{
    for (int i = 0; i < root->child_count; i++) 
    {
        if (strcmp(root->children[i]->name, name) == 0)
            return root->children[i];
    }
    TreeNode *n = create_node(name);
    root->children[root->child_count++] = n;
    return n;
}

// ─── PROVIDED ───────────────────────────────────────────────────────────────

uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;

    if (S_ISDIR(st.st_mode))  return MODE_DIR;
    if (st.st_mode & S_IXUSR) return MODE_EXEC;
    return MODE_FILE;
}

int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;
    const uint8_t *ptr = (const uint8_t *)data;
    const uint8_t *end = ptr + len;

    while (ptr < end && tree_out->count < MAX_TREE_ENTRIES) {
        TreeEntry *entry = &tree_out->entries[tree_out->count];

        const uint8_t *space = memchr(ptr, ' ', end - ptr);
        if (!space) return -1;

        char mode_str[16] = {0};
        size_t mode_len = space - ptr;
        if (mode_len >= sizeof(mode_str)) return -1;
        memcpy(mode_str, ptr, mode_len);
        entry->mode = strtol(mode_str, NULL, 8);

        ptr = space + 1;

        const uint8_t *null_byte = memchr(ptr, '\0', end - ptr);
        if (!null_byte) return -1;

        size_t name_len = null_byte - ptr;
        if (name_len >= sizeof(entry->name)) return -1;
        memcpy(entry->name, ptr, name_len);
        entry->name[name_len] = '\0';

        ptr = null_byte + 1;

        if (ptr + HASH_SIZE > end) return -1; 
        memcpy(entry->hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;

        tree_out->count++;
    }
    return 0;
}

static int compare_tree_entries(const void *a, const void *b) {
    return strcmp(((const TreeEntry *)a)->name, ((const TreeEntry *)b)->name);
}

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    size_t max_size = tree->count * 296; 
    uint8_t *buffer = malloc(max_size);
    if (!buffer) return -1;

    Tree sorted_tree = *tree;
    qsort(sorted_tree.entries, sorted_tree.count, sizeof(TreeEntry), compare_tree_entries);

    size_t offset = 0;
    for (int i = 0; i < sorted_tree.count; i++) {
        const TreeEntry *entry = &sorted_tree.entries[i];

        int written = sprintf((char *)buffer + offset, "%o %s", entry->mode, entry->name);
        offset += written + 1;

        memcpy(buffer + offset, entry->hash.hash, HASH_SIZE);
        offset += HASH_SIZE;
    }

    *data_out = buffer;
    *len_out = offset;
    return 0;
}

// ─── TODO: Implement these ──────────────────────────────────────────────────

// Build a tree hierarchy from the current index and write all tree
// objects to the object store.
//
// HINTS - Useful functions and concepts for this phase:
//   - index_load      : load the staged files into memory
//   - strchr          : find the first '/' in a path to separate directories from files
//   - strncmp         : compare prefixes to group files belonging to the same subdirectory
//   - Recursion       : you will likely want to create a recursive helper function 
//                       (e.g., `write_tree_level(entries, count, depth)`) to handle nested dirs.
//   - tree_serialize  : convert your populated Tree struct into a binary buffer
//   - object_write    : save that binary buffer to the store as OBJ_TREE
//
// Returns 0 on success, -1 on error.
int tree_from_index(ObjectID *id_out) 
{
    Index index;
    if (index_load(&index) != 0) 
        return -1;

    // sanity check
    if (index.count == 0) 
        return -1;

    Tree tree;
    tree.count = 0;

    for (int i = 0; i < index.count; i++) 
    {
        TreeEntry *e = &tree.entries[tree.count++];
        e->mode = index.entries[i].mode;
        const char *path = index.entries[i].path;
        const char *name = strrchr(path, '/');
        if (name)
            name++;
        else
            name = path;

        strncpy(e->name, name, sizeof(e->name));
        e->name[sizeof(e->name) - 1] = '\0';
        memcpy(e->hash.hash, index.entries[i].hash.hash, HASH_SIZE);
    }
    void *data;
    size_t len;

    if (tree_serialize(&tree, &data, &len) != 0)
        return -1;

    ObjectID root_id;
    if (object_write(data, len, OBJ_TREE, &root_id) != 0)
        return -1;

    *id_out = root_id;

    return 0;
}

// phase 2 
// 2A is done
// 2B has dependency on phase 4