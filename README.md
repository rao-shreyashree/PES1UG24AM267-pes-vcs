# PES-VCS — My Version Control System

## Overview

This project is my implementation of a lightweight version control system, inspired by how Git works internally.

Instead of tracking file differences, this system stores complete snapshots of a project using:
- Content-based hashing
- Efficient object storage
- A staging area
- Commit history

The goal was to understand how version control systems actually work at a low level — especially how files are stored, referenced, and reconstructed.

---

## What I Built

I implemented a CLI tool called `pes` that supports:

```bash
pes init              # initialize repository
pes add <file>        # stage files
pes status            # show file states
pes commit -m "msg"   # create snapshot
pes log               # show commit history
```

---

## Core Concepts

### 1. Content-Addressable Storage

Every file is stored based on its SHA-256 hash, not its name.

- Same content → same hash → stored only once
- Ensures deduplication + integrity
- Objects are stored like:

```
.pes/objects/ab/cdef1234...
```

### 2. Object Types

**Blob**
- Stores raw file content
- No metadata (no filename, no path)

**Tree**
- Represents directories
- Maps filenames → blob/tree hashes

**Commit**
- Represents a snapshot
- Contains:
  - Root tree hash
  - Parent commit
  - Author + timestamp
  - Commit message

### 3. Index (Staging Area)

The `.pes/index` file tracks files that are staged for the next commit.

Each entry stores:
```
<mode> <hash> <mtime> <size> <path>
```

This acts as the bridge between the working directory and commits.

---

## My Implementation (Phase-wise)

### Phase 1 — Object Store

Implemented: `object_write`, `object_read`

Key ideas:
- Prepend header (type + size)
- Compute SHA-256
- Store objects using directory sharding
- Verify integrity during reads

### Phase 2 — Trees

Implemented: `tree_from_index`

What it does:
- Converts staged files into a directory structure
- Handles nested paths like `src/main.c`
- Recursively builds tree objects

### Phase 3 — Index

Implemented: `index_load`, `index_save`, `index_add`

Features:
- Text-based index format
- Sorted entries
- Atomic writes (temp file → rename)
- Detects file changes using metadata

### Phase 4 — Commits

Implemented: `commit_create`

Flow:
1. Build tree from index
2. Read parent commit from HEAD
3. Create commit object
4. Update branch reference

This creates a linked history of commits.

---

## Internal Structure

```
.pes/
├── objects/        # all blobs, trees, commits
├── index           # staging area
├── HEAD            # current branch
└── refs/heads/     # branch pointers
```

---

## How It Works (Flow)

```
Working Directory
        │
        ▼
   pes add
        │
        ▼
      Index
        │
        ▼
   pes commit
        │
        ▼
  Object Store (blob → tree → commit)
        │
        ▼
     HEAD / branch updated
```

---

## Key Learnings

- How Git stores data internally (not diffs, but snapshots)
- Why hashing is powerful for storage + integrity
- How directories can be represented as trees
- How commit history is just a linked structure
- Importance of atomic file operations in systems

---

## Testing

Functionality verified using:
- Provided test files (`test_objects`, `test_tree`)
- Manual CLI testing (`pes init`, `add`, `commit`, `log`)
- Integration test (`make test-integration`)

---

## Final Thoughts

This project helped me understand that Git is essentially:

> **A content-addressable filesystem with a few smart abstractions on top.**

Building this from scratch made concepts like commits, trees, and staging way more intuitive.
