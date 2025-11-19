

# ⚙️ C SF File Parser and Directory Utility (Variant 59592)

A command-line utility written in C for **Linux/Unix systems** that processes files adhering to a custom binary structure (referred to here as **SF format**). The utility provides robust features for file validation, binary header parsing, content extraction, and recursive directory listing with advanced filtering.

-----

## ✨ Features

This program is designed to be a single executable with multiple commands, demonstrating low-level file I/O using Unix system calls (`open`, `read`, `lseek`, `stat`).

  * **SF File Validation:** Checks files against a strict set of rules for magic value, version, section count, and section types.
  * **Directory Traversal:** Lists files in a directory (recursively or non-recursively) with size and name filters.
  * **Binary Parsing:** Reads and interprets the custom SF binary header structure.
  * **Content Extraction:** Extracts a specific line from a specific section of a valid SF file.
  * **Special File Search (`findall`):** Recursively searches for SF files containing a specific content requirement (at least two sections with 13 lines).

-----

## 🛠️ Build and Dependencies

This project requires a C compiler (like GCC) and is designed for operating systems that support standard POSIX file I/O and directory traversal functions (`<unistd.h>`, `<sys/stat.h>`, `<dirent.h>`).


## 📝 SF File Format Specification

The custom **SF** binary file structure is validated against the following hardcoded constraints (`VARIANT 59592`):

| Component | Offset/Position | Size (Bytes) | Constraints |
| :--- | :--- | :--- | :--- |
| **Magic** | Last Byte | 1 | Must be the ASCII value: `'z'` |
| **Header Size** | `File Size - 3` | 2 | Denotes the size of the header block. |
| **Version** | Start of Header | 4 | Must be between **84** and **163** (inclusive). |
| **Number of Sections**| After Version | 1 | Must be **2** OR between **6** and **14** (inclusive). |
| **Section Type** | Per Section Header | 2 | Must be one of the valid types: **80, 43, 38, 23, 81**. |


-----

## 🚀 Commands

### 1\. `variant`

Displays the predefined variant number used for the project's constraints.

```bash
./sf_utility variant
# Output:
# 59592
```

### 2\. `list`

Lists files and directories recursively or non-recursively, with optional filtering.

| Option | Format | Description |
| :--- | :--- | :--- |
| `path` | `path=<directory_path>` | **Required.** Specifies the directory to list. |
| `recursive` | `recursive` | If present, traverses subdirectories. |
| `size_smaller`| `size_smaller=<N>` | Filters regular files with size `< N` bytes. |
| `name_starts_with`| `name_starts_with=<prefix>` | Filters entries whose name starts with `<prefix>`. |

**Example Usage:**

```bash
# List all files and directories recursively
./sf_utility list recursive path=/tmp/test_dir
# Output:
# SUCCESS
# /tmp/test_dir/file1.txt
# /tmp/test_dir/subdir
# /tmp/test_dir/subdir/file2.sf

# List files smaller than 1024 bytes
./sf_utility list path=/home/user/files size_smaller=1024
# Output:
# SUCCESS
# /home/user/files/small_log.txt
```

### 3\. `parse`

Parses a single SF file, validates its structure, and prints its detailed header information.

```bash
./sf_utility parse path=<file_path>
# Output on Success:
# SUCCESS
# version=90
# nr_sections=8
# section1: SECTION_A 80 1024
# section2: BINARY_B 43 512
# ...
```

*(Output will show validation errors like `wrong magic`, `wrong version`, etc., on failure)*

### 4\. `extract`

Extracts and prints the **N-th line from the end** of a specified section. The content is filtered to display only printable ASCII characters (32 to 126).

| Option | Format | Description |
| :--- | :--- | :--- |
| `path` | `path=<file_path>` | **Required.** The path to the SF file. |
| `section` | `section=<N>` | **Required.** The 1-based index of the section to read (e.g., `section=1`). |
| `line` | `line=<N>` | **Required.** The 1-based index of the line to extract, counted **from the end** of the section content. |

**Example Usage:**

```bash
# Extract the 3rd line from the end of Section 2
./sf_utility extract path=./file.sf section=2 line=3
# Output:
# SUCCESS
# This is the 3rd line from the end.
```

### 5\. `findall`

Recursively traverses a specified directory and lists the full paths of all regular files that are valid SF files and meet the special content requirement.

**Special Content Requirement:** The file must contain **at least two sections** where the section content has **exactly 13 newline characters (`\n`)** (resulting in 13 lines).

```bash
./sf_utility findall path=<directory_path>
# Output:
# SUCCESS
# /tmp/data/valid_file_a.sf
# /tmp/data/sub/another_valid.sf
```
