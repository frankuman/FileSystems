#include <iostream>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    memset(fat, 0, sizeof(fat));
}

FS::~FS()
{

}

// read FAT into memory, and make sure that it's formatted or not.
int
FS::readFAT(bool assumeFormatted)
{
    int sts = disk.read(FAT_BLOCK, (uint8_t*)fat);
    if (sts)
        return sts;
    
    if (assumeFormatted)
    {
        if (fat[FAT_BLOCK] != FAT_EOF)
        {
            std::cout << "Error: Disk is not formatted\n";
            sts = 1;
        }
    }
    else
    {
        if (fat[FAT_BLOCK] == FAT_EOF)
        {
            std::cout << "Disk will be re-formatted\n";
        }
    }

    return sts;
}

// write FAT from memory to disk
int
FS::writeFAT()
{
    int sts = disk.write(FAT_BLOCK, (uint8_t*)fat);
    return sts;
}

// find next free block in FAT. Returns -1 if no more FAT entries exists.
int
FS::findFreeBlock()
{
    int fat_index;
    for (fat_index = 2; fat_index < (BLOCK_SIZE / 2); fat_index++)
    {
        if (fat[fat_index] == FAT_FREE)
        {
            break;
        }
    }
    if (fat_index == (BLOCK_SIZE / 2)) {
        fat_index = -1;
    }

    return fat_index;
}

// get the filename from a full filepath, e.g. /dir1/dir2/test.txt
std::string
FS::getFileName(std::string filepath)
{
    std::string::size_type idx;

    idx = filepath.rfind('/');

    if (idx != std::string::npos)
    {
        filepath = filepath.substr(idx + 1);
    }
    return filepath;
}

// get the directory path from a full filepath, e.g. /dir1/dir2/test.txt
std::string
FS::getDirPath(std::string filepath)
{
    std::string::size_type idx;

    idx = filepath.rfind('/');

    std::string dirpath;
    if (idx != std::string::npos)
        dirpath = filepath.substr(0, idx);
    else
        dirpath = "";
    return dirpath;
}

// return the access rights as a string with the format "rwx" for read/write/execute
std::string
FS::getAccessString(uint8_t access_rights)
{
    std::string access = "";
    access += (access_rights & READ) ? "r" : "-";
    access += (access_rights & WRITE) ? "w" : "-";
    access += (access_rights & EXECUTE) ? "x" : "-";
    return access;
}

// find the directory block where the given file resides (note: filepath may contain a relative or absolute path)
int
FS::getDirBlock(std::string filepath, int& dir_block)
{
    std::string dirpath = getDirPath(filepath); // -> dir/godis/tast.txt   -> dir/godis/
    if (dirpath.empty()) {

        if (filepath.front() == '/') {
            // absolute path - starts at the root dir
            dir_block = ROOT_BLOCK;
        }
        else {
            dir_block = current_block;
        }

    }
    else {

        if (dirpath.front() == '/') {
            // absolute path - starts at the root dir
            dir_block = ROOT_BLOCK;
            dirpath = dirpath.substr(1);
        }
        else {
            // relative path - starts at the current dir
            dir_block = current_block;
        }

        // traverse dirpath to find the correct dir block
        while (!dirpath.empty()) {
            std::string::size_type idx = dirpath.find('/');
            std::string dirname;
            if (idx != std::string::npos) {
                dirname = dirpath.substr(0, idx);
                dirpath = dirpath.substr(idx + 1);
            }
            else {
                dirname = dirpath;
                dirpath = "";
            }

            int dir_index;
            dir_entry dir_entries[MAX_DIR_ENTRIES];
            int sts = findFileEntry(dir_block, dirname, EXISTING_FILE, dir_index, dir_entries, READ);
            if (sts)
                return sts;

            if (dir_entries[dir_index].type != TYPE_DIR) {
                std::cout << "Error: Filepath is not a directory: " << dirname << "\n";
                return 1;
            }
            dir_block = dir_entries[dir_index].first_blk;
        }
    }

    return 0;
}

int
FS::findFileEntry(std::string filepath, uint8_t findNewOrExisting, dir_info& dir, uint8_t access_rights)
{
    int sts = getDirBlock(filepath, dir.block);
    if (sts)
        return sts;

    return findFileEntry(dir.block, filepath, findNewOrExisting, dir.index, dir.entries, access_rights);
}

// find file index in a given directory block - based on a file name
// depending on the given parameter 'findNewOrExisting' -> find the given file, or find a free entry for the file.
int
FS::findFileEntry(int dir_block, std::string filepath, uint8_t findNewOrExisting, int& dir_index, dir_entry* dir_entries, uint8_t access_rights)
{
    int sts = disk.read(dir_block, (uint8_t*)dir_entries);
    if (sts)
        return sts;

    std::string filename = getFileName(filepath);
    dir_index = -1;
    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++)
    {
        if (dir_entries[i].file_name[0] == 0)
        {
            if (findNewOrExisting == NEW_FILE) {
                // We're looking for a free entry - and this is a free dir entry
                if (dir_index < 0)
                    dir_index = i;
            }
        }
        else if (strcmp(dir_entries[i].file_name, filename.c_str()) == 0) {

            if (findNewOrExisting == NEW_FILE) {
                dir_index = -2;
            }
            else {
                dir_index = i;
            }
            break;
        }
    }

    if (dir_index < 0) {
        if (dir_index == -2) {
            std::cout << "Error: File already exists: " << filepath << "\n";
        }
        else {
            if (findNewOrExisting == NEW_FILE)
                std::cout << "Error: No more free directory entries\n";
            else
                std::cout << "Error: File not found: " << filepath << "\n";
        }
        sts = 1;
    }
    else if (access_rights > 0) {

        // Check file access

        if (findNewOrExisting == EXISTING_FILE) {
            // Existing file: check the requested access to the file
            bool access_is_ok = (dir_entries[dir_index].access_rights & access_rights) == access_rights;
            if (!access_is_ok) {
                std::cout << "Error: The file access of '" << dir_entries[dir_index].file_name <<
                    "' is '" << getAccessString(dir_entries[dir_index].access_rights) << "' which doesn't match the required access '" <<
                    getAccessString(access_rights) << "'\n";
                return 1;
            }
        }
    }

    return sts;
}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    std::cout << "FS::format()\n";

    // read FAT from disk to memory
    int sts = readFAT(false);
    if (sts) return sts;

    // initialize FAT
    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK] = FAT_EOF;
    for (int i = 2; i < (BLOCK_SIZE / 2); i++)
    {
        fat[i] = FAT_FREE;
    }

    // write FAT to disk
    sts = writeFAT();
    if (sts) return sts;

    // initialize root block to all zeroes
    uint8_t block[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        block[i] = 0;
    }
    sts = disk.write(ROOT_BLOCK, block);
    return sts;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";

    std::string filename = getFileName(filepath);
    if (filename.length() > MAX_FILE_NAME_LENGTH)
    {
        std::cout << "Error: File name length (" << filename.length() << " characters) must not exceed max file name length of " << MAX_FILE_NAME_LENGTH << " characters\n";
        return 1;
    }
    if (filename == PARENT_DIR)
    {
        std::cout << "Error: '" << PARENT_DIR << "' is reserver for the parent directory name.\n";
        return 1;
    }

    // read FAT from disk to memory
    int sts = readFAT(true);
    if (sts) return sts;

    // Find free directory entry for the new file. Plus make sure that only one file with the same name can exist.
    dir_info dir;
    sts = findFileEntry(filepath, NEW_FILE, dir, WRITE);
    if (sts) return sts;

    uint32_t size = 0;      // Current block size counter
    uint32_t tot_size = 0;  // File size counter

    // Find free FAT entry
    int curr_blk = findFreeBlock();
    if (curr_blk < 0) {
        std::cout << "Error: No more free blocks in FAT\n";
        return 1;
    }
    fat[curr_blk] = FAT_EOF;   // initially mark FAT index as used
    int first_blk = curr_blk;

    char data[BLOCK_SIZE];
    memset(data, 0, BLOCK_SIZE);

    bool done = false;
    std::string line;
    while (!done) {

        // Get file content ... line by line entered by user. EOF when user enters a blank line.
        std::getline(std::cin, line);

        if (line.empty()) {

            done = true;

            // Write the last file block.
            sts = disk.write(curr_blk, (uint8_t*)data);
            if (sts)
                return sts;
        }
        else {

            while (line.length() > 0) {

                // check block max-length, and handle multi-block file sizes.
                if ((line.length() + 1 + size) > BLOCK_SIZE) {
                    // Not enough space for the complete line. Need to write as much as possible of the line and then continue on a new file block.
                    int len = BLOCK_SIZE - size;
                    tot_size += len;
                    memcpy(&data[size], line.c_str(), len);

                    sts = disk.write(curr_blk, (uint8_t*)data);
                    if (sts)
                        return sts;

                    // Start a new block and write the remaining characters to that block.

                    // Find free FAT entry
                    int new_blk = findFreeBlock();
                    if (new_blk < 0) {
                        std::cout << "Error: No more free blocks in FAT\n";
                        return 1;
                    }

                    fat[curr_blk] = new_blk;   // set current FAT index to point to next block in chain
                    curr_blk = new_blk;
                    fat[curr_blk] = FAT_EOF;   // initially mark new FAT as used
                    memset(data, 0, BLOCK_SIZE);
                    size = 0;

                    // Remove the first part of the string buffer - which has been written to file above.
                    line = line.substr(len);
                    if (line.empty())
                        tot_size++;

                }
                else {
                    // Current line fits into the file block. Copy it to the block (including the null-termination character '\0')
                    memcpy(&data[size], line.c_str(), line.length() + 1);
                    size += line.length() + 1;
                    tot_size += line.length() + 1;
                    line.clear();
                }
            }
        }
    }

    memcpy(dir.entries[dir.index].file_name, filename.c_str(), filename.length() + 1);
    dir.entries[dir.index].first_blk = first_blk;
    dir.entries[dir.index].size = tot_size;
    dir.entries[dir.index].type = TYPE_FILE;
    dir.entries[dir.index].access_rights = READ | WRITE;

    sts = disk.write(dir.block, (uint8_t*)dir.entries);
    if (sts)
        return sts;

    sts = writeFAT();
    if (sts)
        return sts;

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    // read FAT from disk to memory
    int sts = readFAT(true);
    if (sts) return sts;

    // Find the directory index for the passed file and read directory block into memory.
    dir_info dir;
    sts = findFileEntry(filepath, EXISTING_FILE, dir, READ);
    if (sts) return sts;

    if (dir.entries[dir.index].type == TYPE_DIR) {
        std::cout << "Error: '" << filepath << "' is a directory\n";
        return 1;
    }

    int file_blk = dir.entries[dir.index].first_blk;
    uint32_t file_size = dir.entries[dir.index].size;

    uint32_t size = 0;
    uint32_t tot_size = 0;
    char data[BLOCK_SIZE];

    sts = disk.read(file_blk, (uint8_t*)data);
    if (sts)
        return sts;

    while (tot_size < file_size) {

        // A line can potentially span over multiple blocks, so check block boundaries.
        int len = 0;
        for (int i = size; i < BLOCK_SIZE; i++, len++)
            if (data[i] == '\0') break;

        printf("%-*.*s", len, len, &data[size]);
        size += len;
        tot_size += len;
        if (size == BLOCK_SIZE) {
            // line continues in next block
            size = 0;
            file_blk = fat[file_blk];
            if (file_blk == FAT_EOF) {
                printf("Programming error ... unexpected EOF detected\n");
                return 1;
            }

            sts = disk.read(file_blk, (uint8_t*)data);
            if (sts)
                return sts;

        }
        else {
            size++;
            tot_size++;
            printf("\n");
        }
    }

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n";

    // read FAT from disk to memory
    int sts = readFAT(true);
    if (sts) return sts;

    // Get the directory entries for the current dir
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    sts = disk.read(current_block, (uint8_t*)dir_entries);
    if (sts)
        return sts;

    // Use printf to make the output prettier and more readable
    // Design decision: Show also the used FAT blocks - to make it easier to test the program
    char underline[MAX_FILE_NAME_LENGTH];
    memset(underline, '-', MAX_FILE_NAME_LENGTH - 1);
    underline[MAX_FILE_NAME_LENGTH - 1] = '\0';
    printf("%-*.*s %-*.*s  %-*.*s  %*.*s  %-*.*s\n", MAX_FILE_NAME_LENGTH, MAX_FILE_NAME_LENGTH, "filename", 5, 5, "type", 6, 6, "access", 9, 9, "size", 15, 15, "file-blocks");
    printf("%-*.*s %-*.*s  %-*.*s  %*.*s  %-*.*s\n", MAX_FILE_NAME_LENGTH, MAX_FILE_NAME_LENGTH, underline, 5, 5, underline, 6, 6, underline, 9, 9, underline, 15, 15, underline);
    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++)
    {
        if (dir_entries[i].file_name[0] != 0)
        {
            // this is a valid dir entry
            int curr_blk = dir_entries[i].first_blk;
            std::string blocks = std::to_string(curr_blk);
            while (curr_blk >= 0 && curr_blk < (int)MAX_DIR_ENTRIES && fat[curr_blk] != FAT_EOF) {
                curr_blk = fat[curr_blk];
                blocks += "," + std::to_string(curr_blk);
            }
            std::string type = dir_entries[i].type == TYPE_FILE ? "File" : "Dir";
            std::string access = getAccessString(dir_entries[i].access_rights);
            printf("%-*.*s %-*.*s   %-*.*s %*.d  %s\n", MAX_FILE_NAME_LENGTH, MAX_FILE_NAME_LENGTH, dir_entries[i].file_name, 5, 5, type.c_str(), 5, 5, access.c_str(), 10, dir_entries[i].size, blocks.c_str());
        }
    }

    return 0;
}

// cp <sourcefilepath> <destfilepath> makes an exact copy of the file
// <sourcefilepath> to a new file <destfilepath>
int
FS::cp(std::string sourcefilepath, std::string destfilepath)
{
    std::cout << "FS::cp(" << sourcefilepath << "," << destfilepath << ")\n";

    // read FAT from disk to memory
    int sts = readFAT(true);
    if (sts) return sts;

    // Find existing/source file, and find a free index for the destination file
    dir_info src;
    dir_info dest;
    sts = findFileEntry(sourcefilepath, EXISTING_FILE, src, READ);
    if (sts) return sts;
    sts = findFileEntry(destfilepath, NEW_FILE, dest, WRITE);
    if (sts) return sts;

    if (src.entries[src.index].type == TYPE_DIR) {
        std::cout << "Error: '" << sourcefilepath << "' is a directory\n";
        return 1;
    }

    char data[BLOCK_SIZE];
    memset(data, 0, BLOCK_SIZE);

    // Copy block by block from source file to destination file
    int prev_blk = -1;
    int first_blk = -1;
    int source_blk = src.entries[src.index].first_blk;
    while (source_blk != FAT_EOF) {

        // Find free FAT entry
        int curr_blk = findFreeBlock();
        if (curr_blk < 0) {
            std::cout << "Error: No more free blocks in FAT\n";
            return 1;
        }
        if (first_blk == -1)
            first_blk = curr_blk;
        if (prev_blk != -1)
            fat[prev_blk] = curr_blk;
        fat[curr_blk] = FAT_EOF;

        sts = disk.read(source_blk, (uint8_t*)data);
        if (sts) return sts;
        sts = disk.write(curr_blk, (uint8_t*)data);
        if (sts) return sts;

        prev_blk = curr_blk;
        source_blk = fat[source_blk];

    }

    /*
     * When copying a file a decision was made that the user must either give the destination name of the file,
     * or simply use the Linux standard �.� in order to use the existing name of the source file.
     * For instance - the following commands will give the same result:
     * $ cp file1 dir1/.
     * $ cp file1 dir1/file1
    */
    std::string dest_filename = getFileName(destfilepath);
    if (dest_filename == ".")
        dest_filename = getFileName(sourcefilepath);

    // Copy the file entry info from source to destination.
    memcpy(&dest.entries[dest.index], &src.entries[src.index], sizeof(dir_entry));
    // Set the new file name and the new first-block
    memcpy(dest.entries[dest.index].file_name, dest_filename.c_str(), dest_filename.length() + 1);
    dest.entries[dest.index].first_blk = first_blk;

    sts = disk.write(dest.block, (uint8_t*)dest.entries);
    if (sts)
        return sts;

    sts = writeFAT();
    if (sts)
        return sts;

    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";

    // Find existing/source file, and find a free index for the destination file (to verify that it does not already exists)
    dir_info src;
    dir_info dest;
    int sts = findFileEntry(sourcepath, EXISTING_FILE, src, READ);
    if (sts) return sts;
    sts = findFileEntry(destpath, NEW_FILE, dest, WRITE);
    if (sts) return sts;

    if (src.entries[src.index].type == TYPE_DIR) {
        std::cout << "Error: '" << sourcepath << "' is a directory. It is not allowed to move directories\n";
        return 1;
    }

    /*
     * When using mv to move or rename a file, a decision was made that the user must either give the destination name of the file,
     * or simply use the Linux standard �.� in order to use the existing name of the source file.
     * For instance - the following commands will give the same result:
     * $ mv file1 dir1/.
     * $ mv file1 dir1/file1
     */
    std::string dest_name = getFileName(destpath);
    if (dest_name == ".") {
        dest_name = getFileName(sourcepath);
        destpath = destpath.substr(0, destpath.length() - 1) + dest_name;
    }

    if (dest.block == src.block) {
        // source and destination resides in the same directory -> simply rename the filename.
        memcpy(src.entries[src.index].file_name, dest_name.c_str(), dest_name.length() + 1);

        sts = disk.write(src.block, (uint8_t*)src.entries);
        if (sts)
            return sts;
    }
    else {
        // we need to move the directory entry to the other directory!
        memcpy(&dest.entries[dest.index], &src.entries[src.index], sizeof(dir_entry));
        memcpy(dest.entries[dest.index].file_name, dest_name.c_str(), dest_name.length() + 1);
        sts = disk.write(dest.block, (uint8_t*)dest.entries);
        if (sts)
            return sts;

        memset(&src.entries[src.index], 0, sizeof(dir_entry));
        sts = disk.write(src.block, (uint8_t*)src.entries);
        if (sts)
            return sts;
    }


    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";

    // read FAT from disk to memory
    int sts = readFAT(true);
    if (sts) return sts;

    // Find the directory index for the passed file and read directory block into memory.
    dir_info src;
    sts = findFileEntry(filepath, EXISTING_FILE, src, WRITE);
    if (sts) return sts;

    // don't allow removal of a directory
    if (src.entries[src.index].type == TYPE_DIR) {
        std::cout << "Error: '" << filepath << "' is a directory\n";
        return 1;
    }

    int disk_blk = src.entries[src.index].first_blk;
    while (disk_blk != FAT_EOF) {
        // Mark the FAT entry as unused.
        int next_blk = fat[disk_blk];
        fat[disk_blk] = FAT_FREE;
        disk_blk = next_blk;
    }

    // Clear directory entry
    memset(&src.entries[src.index], 0, sizeof(dir_entry));
    sts = disk.write(src.block, (uint8_t*)src.entries);
    if (sts)
        return sts;

    sts = writeFAT();
    if (sts)
        return sts;

    return 0;
}

// append <sourcefilepath> <destfilepath> appends the contents of file <sourcefilepath> to
// the end of file <destfilepath>. The file <destfilepath> is unchanged.
int
FS::append(std::string sourcefilepath, std::string destfilepath)
{
    std::cout << "FS::append(" << sourcefilepath << "," << destfilepath << ")\n";

    // read FAT from disk to memory
    int sts = readFAT(true);
    if (sts) return sts;

    // Find existing source and destination files, and read respective directory block into memory.
    dir_info src;
    dir_info dest;
    sts = findFileEntry(sourcefilepath, EXISTING_FILE, src, READ);
    if (sts) return sts;
    sts = findFileEntry(destfilepath, EXISTING_FILE, dest, READ | WRITE);
    if (sts) return sts;

    if (src.entries[src.index].type == TYPE_DIR) {
        std::cout << "Error: '" << sourcefilepath << "' is a directory\n";
        return 1;
    }
    if (dest.entries[dest.index].type == TYPE_DIR) {
        std::cout << "Error: '" << destfilepath << "' is a directory\n";
        return 1;
    }

    // Find the block where the destination file ends, i.e. where we should start to append the source file.
    int src_file_size = src.entries[src.index].size;
    int dest_file_size = dest.entries[dest.index].size;
    int dest_idx = dest_file_size;
    int dest_blk = dest.entries[dest.index].first_blk;
    int prev_blk = -1;
    while (dest_idx >= BLOCK_SIZE) {
        dest_idx -= BLOCK_SIZE;
        prev_blk = dest_blk;
        dest_blk = fat[dest_blk];
    }

    int src_idx = 0;
    //char src_data[BLOCK_SIZE];
    //char dest_data[BLOCK_SIZE];
    char* src_data = (char*) malloc(BLOCK_SIZE);
    char* dest_data = (char*)malloc(BLOCK_SIZE);
    if (src_data == NULL) {
        std::cout << "Error: Could not allocate memory (malloc)\n";
        return 1;
    }
    if (dest_data == NULL) {
        free(src_data);
        std::cout << "Error: Could not allocate memory (malloc)\n";
        return 1;
    }

    int src_blk = src.entries[src.index].first_blk;

    sts = disk.read(src_blk, (uint8_t*)src_data);
    if (sts) {
        free(src_data);
        free(dest_data);
        return sts;
    }

    if (dest_idx > 0 || prev_blk == -1) {
        // Read the destination block from disk that we should continue to append data to (i.e. it's not completely filled)
        sts = disk.read(dest_blk, (uint8_t*)dest_data);
        if (sts) {
            free(src_data);
            free(dest_data);
            return sts;
        }
    }
    else {
        // Destination file ends on a block boundary. Need to allocate a fresh new block

        // Find free FAT entry
        dest_blk = findFreeBlock();
        if (dest_blk < 0) {
            std::cout << "Error: No more free blocks in FAT\n";
            free(src_data);
            free(dest_data);
            return 1;
        }
        fat[prev_blk] = dest_blk;
        fat[dest_blk] = FAT_EOF;

        memset(dest_data, 0, BLOCK_SIZE);
    }

    bool done = false;
    while (!done) {

        // Calculate the max nbr of bytes to be copied from source to destination file block. Need to check block boundaries carefully.
        int max_src_size = src_file_size < (BLOCK_SIZE - src_idx) ? src_file_size : (BLOCK_SIZE - src_idx);
        int max_dest_size = BLOCK_SIZE - dest_idx;

        int cpy_size = max_src_size > max_dest_size ? max_dest_size : max_src_size;

        // Append/copy data from source block to dest file block
        memcpy(&dest_data[dest_idx], &src_data[src_idx], cpy_size);

        src_idx += cpy_size;
        src_file_size -= cpy_size;
        dest_idx += cpy_size;
        dest_file_size += cpy_size;

        if (src_idx >= BLOCK_SIZE) {
            // Read next source file block
            src_idx = 0;
            if (src_blk < 0 || src_blk >= BLOCK_SIZE) {
                std::cout << "Error: Invalid block number: "<< src_blk <<"\n";
                free(src_data);
                free(dest_data);
                return 1;
            }
            src_blk = fat[src_blk];
            if (src_blk != FAT_EOF) {
                sts = disk.read(src_blk, (uint8_t*)src_data);
                if (sts) {
                    free(src_data);
                    free(dest_data);
                    return sts;
                }
            }
        }

        if (dest_idx >= BLOCK_SIZE || src_file_size == 0) {
            // Write current dest block to file and allocate a new disk block.
            dest_idx = 0;
            sts = disk.write(dest_blk, (uint8_t*)dest_data);
            if (sts) {
                free(src_data);
                free(dest_data);
                return sts;
            }

            if (src_file_size == 0) {
                done = true;
            }
            else {

                // Find free FAT entry
                prev_blk = dest_blk;
                dest_blk = findFreeBlock();
                if (dest_blk < 0) {
                    std::cout << "Error: No more free blocks in FAT\n";
                    free(src_data);
                    free(dest_data);
                    return 1;
                }
                fat[prev_blk] = dest_blk;
                fat[dest_blk] = FAT_EOF;

                memset(dest_data, 0, BLOCK_SIZE);
            }
        }

    }

    free(src_data);
    free(dest_data);


    dest.entries[dest.index].size = dest_file_size;
    sts = disk.write(dest.block, (uint8_t*)dest.entries);
    if (sts)
        return sts;

    sts = writeFAT();
    if (sts)
        return sts;

    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";

    std::string dirname = getFileName(dirpath);
    if (dirname == PARENT_DIR)
    {
        std::cout << "Error: '" << PARENT_DIR << "' is reserver for the parent directory name.\n";
        return 1;
    }

    // read FAT from disk to memory
    int sts = readFAT(true);
    if (sts) return sts;

    // Find free directory entry for the new directory. Plus make sure that only one directory with the same name can exist.
    dir_info dir;
    sts = findFileEntry(dirpath, NEW_FILE, dir, WRITE);
    if (sts) return sts;

    // Find free FAT entry
    int dir_blk = findFreeBlock();
    if (dir_blk < 0) {
        std::cout << "Error: No more free blocks in FAT\n";
        return 1;
    }
    fat[dir_blk] = FAT_EOF;

    // Add new directory name to current dir block
    memcpy(dir.entries[dir.index].file_name, dirname.c_str(), dirname.length() + 1);
    dir.entries[dir.index].first_blk = dir_blk;
    dir.entries[dir.index].size = 0;
    dir.entries[dir.index].type = TYPE_DIR;
    dir.entries[dir.index].access_rights = READ | WRITE | EXECUTE;

    sts = disk.write(dir.block, (uint8_t*)dir.entries);
    if (sts)
        return sts;

    // Add the mandatory parent directory ("..") to new directory block, i.e. to the destination dir block
    memset(dir.entries, 0, BLOCK_SIZE);
    dir.index = 0;
    memcpy(dir.entries[dir.index].file_name, PARENT_DIR.c_str(), PARENT_DIR.length() + 1);
    dir.entries[dir.index].first_blk = dir.block;
    dir.entries[dir.index].size = 0;
    dir.entries[dir.index].type = TYPE_DIR;
    dir.entries[dir.index].access_rights = READ | WRITE | EXECUTE;

    sts = disk.write(dir_blk, (uint8_t*)dir.entries);
    if (sts)
        return sts;

    sts = writeFAT();
    if (sts)
        return sts;

    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    if (dirpath == "/") {
        // Simply go to the root directory
        current_block = ROOT_BLOCK;
    }
    else {
        // Remove trailing "/" from the dirpath
        if (dirpath.back() == '/')
            dirpath.pop_back();

        // Find the directory index for the passed directory and read the directory block into memory.
        dir_info dir;
        int sts = findFileEntry(dirpath, EXISTING_FILE, dir, READ);
        if (sts) return sts;

        if (dir.entries[dir.index].type != TYPE_DIR) {
            std::cout << "Error: '" << dirpath << "' is not a directory\n";
            return 1;
        }

        current_block = dir.entries[dir.index].first_blk;
    }

    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n";
    std::string path = "";
    if (current_block == ROOT_BLOCK) {
        path = "/";

    }
    else {
        int curr_block = current_block;
        while (curr_block != ROOT_BLOCK) {

            int dir_index;
            dir_entry dir_entries[MAX_DIR_ENTRIES];
            int sts = findFileEntry(curr_block, PARENT_DIR, EXISTING_FILE, dir_index, dir_entries, 0);
            if (sts) return sts;

            int parent_block = dir_entries[dir_index].first_blk;

            // read parent directory
            sts = disk.read(parent_block, (uint8_t*)dir_entries);
            if (sts) return sts;

            // find directory entry which points to the current directory
            dir_index = -1;
            for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++)
            {
                if (dir_entries[i].file_name[0] != 0 && dir_entries[i].first_blk == curr_block)
                {
                    dir_index = i;
                    break;
                }
            }

            if (dir_index < 0) {
                std::cout << "Error: Sub directory with block-number " << curr_block << " not found in path " << path << "\n";
                return 1;
            }

            // get the directory name of the *current* directory
            std::string dir = dir_entries[dir_index].file_name;
            if (curr_block == current_block)
                path = dir;
            else
                path = dir + "/" + path;
            curr_block = parent_block;

        }
        path = "/" + path;
    }

    std::cout << path.c_str() << "\n";

    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";

    uint8_t access_rights = std::stoi(accessrights, nullptr, 0);

    std::string filename = getFileName(filepath);
    if (filename == PARENT_DIR)
    {
        std::cout << "Error: parent dir '" << PARENT_DIR << "' cannot be modified.\n";
        return 1;
    }

    // Find the directory index for the passed file and read directory block into memory.
    dir_info dir;
    int sts = findFileEntry(filepath, EXISTING_FILE, dir, 0);  // Note! If we don't allow full access for chmod we cannot change a read-only file
    if (sts) return sts;

    dir.entries[dir.index].access_rights = access_rights;
    sts = disk.write(dir.block, (uint8_t*)dir.entries);
    if (sts)
        return sts;

    return 0;
}
