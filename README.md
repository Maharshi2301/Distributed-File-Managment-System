# Distributed File System – COMP-8567 Project

## Overview

This project implements a **Distributed File System** using **C** and **socket programming**, simulating a multi-client server environment across four nodes: **S1, S2, S3, and S4**. The design allows seamless file operations (upload, download, remove, list, tar) while abstracting the distribution logic from the clients.

## Key Features

- Multi-client support through process forking
- File-type-based distribution:
  - `.c` files stored on S1
  - `.pdf` files routed to S2
  - `.txt` files routed to S3
  - `.zip` files routed to S4
- Transparent communication: clients only interact with S1
- File operations: `uploadf`, `downlf`, `removef`, `downltar`, `dispfnames`
- Dynamic folder creation and inter-server communication

## Architecture

```
Clients
   │
   ▼
  S1 (Main Server)
   ├── S2 (.pdf files)
   ├── S3 (.txt files)
   └── S4 (.zip files)
```

## File Structure

- `S1.c` – Main server handling client connections, command parsing, and routing logic.
- `S2.c` – Secondary server that receives and stores `.pdf` files.
- `S3.c` – Secondary server that receives and stores `.txt` files.
- `S4.c` – Secondary server that receives and stores `.zip` files.
- `w25clients.c` – Client-side interface for issuing commands.

## Commands

### `uploadf <filename> <destination_path>`
Uploads a file to `~S1`, but depending on type:
- `.c` stored on S1
- `.pdf`, `.txt`, `.zip` forwarded to S2, S3, S4 respectively

### `downlf <filepath>`
Downloads a file from S1 (or retrieves from S2/S3/S4 if needed).

### `removef <filepath>`
Deletes a file from S1 or respective secondary server.

### `downltar <filetype>`
Creates a `.tar` archive of all files of a given type and sends it to the client:
- `.c` → from S1
- `.pdf` → from S2
- `.txt` → from S3

### `dispfnames <pathname>`
Displays a sorted list of `.c`, `.pdf`, `.txt`, and `.zip` files within the given S1 directory (merged from all servers).

## Setup & Execution

1. Compile each `.c` file on separate terminals:
    ```bash
    gcc S1.c -o S1
    gcc S2.c -o S2
    gcc S3.c -o S3
    gcc S4.c -o S4
    gcc w25clients.c -o w25clients
    ```

2. Launch each server in its terminal:
    ```bash
    ./S2
    ./S3
    ./S4
    ./S1
    ```

3. Run client(s):
    ```bash
    ./w25clients
    ```

## Learning Outcomes

- Mastered socket programming for inter-process communication
- Applied OS-level concepts like forking and process management
- Designed a scalable architecture with distributed routing logic
- Implemented robust client-server protocols with file handling

## Notes

- File routing is hidden from clients – they always interact with S1
- Only specific file types are supported
- Error handling and logging are included for major operations
- Ensure directory trees `~/S1`, `~/S2`, `~/S3`, and `~/S4` exist before execution
