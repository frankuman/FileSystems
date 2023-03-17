#include <iostream>
#include <string>
#include <cstdint>
#include <cstring>
#include "disk.h"

#ifndef __FS_H__
#define __FS_H__

#define ROOT_BLOCK 0
#define FAT_BLOCK 1
#define FAT_FREE 0
#define FAT_EOF -1

#define TYPE_FILE 0
#define TYPE_DIR 1
#define READ 0x04
#define WRITE 0x02
#define EXECUTE 0x01

#define MAX_FILE_NAME_LENGTH 55

#define EXISTING_FILE 0
#define NEW_FILE 1

// Avoid padding of int8 and int16 members, otherwise this struct may require 68 bytes on disk instead of 64, and we will in that case loose 4 file entries per directory.
#pragma pack(push, 1)
struct dir_entry {
    char file_name[MAX_FILE_NAME_LENGTH + 1]; // name of the file / sub-directory
    uint16_t first_blk; // index in the FAT for the first block of the file
    uint32_t size; // size of the file in bytes
    uint8_t type; // directory (1) or file (0)
    uint8_t access_rights; // read (0x04), write (0x02), execute (0x01)
};
#pragma pack(pop)

const unsigned MAX_DIR_ENTRIES = (BLOCK_SIZE / sizeof(dir_entry));
const std::string PARENT_DIR = "..";

// Helper struct to keep info about a file in a directory
struct dir_info {
    int block;  // disk block number where the dir 'entries' are stored
    int index;  // index to actual dir_entry in the 'entries' array
    dir_entry entries[MAX_DIR_ENTRIES]; // all directory entries in a block
};

class FS {
private:
    Disk disk;
    // size of a FAT entry is 2 bytes
    int16_t fat[BLOCK_SIZE / 2];

    // keep track of current block - it will be changed by the command cd(dirpath);
    uint16_t current_block = ROOT_BLOCK;

    // read FAT into memory
    int readFAT(bool assumeFormatted);
    // write FAT from memory to disk
    int writeFAT();
    // find next free block in FAT
    int findFreeBlock();

    // get the filename from a full filepath, e.g. extract "test.txt" from path /dir1/dir2/test.txt
    std::string getFileName(std::string filepath);
    // get the directory path from a full filepath, e.g. extract "/dir1/dir2" from path /dir1/dir2/test.txt
    std::string getDirPath(std::string filepath);

    // return the access rights as a string with the format "rwx" for read/write/execute
    std::string getAccessString(uint8_t access_rights);

    // find the directory block where the given file resides (note: filepath may contain a relative or absolute path)
    int getDirBlock(std::string filepath, int& dir_block);

    // find a file entry in directory. Either find an exsting file, or a free entry for a new file (note: check for duplicate filenames).
    int findFileEntry(std::string filepath, uint8_t newOrExisting, dir_info& dir, uint8_t access_rights);

    // find a file entry in a directory. The directory block is known. 
    // either find an exsting file, or find a free entry for a new file (note: check for duplicate filenames).
    int findFileEntry(int block_no, std::string filepath, uint8_t newOrExisting, int& dir_index, dir_entry* dir_entries, uint8_t access_rights);

    

public:
    FS();
    ~FS();

    // formats the disk, i.e., creates an empty file system
    int format();
    // create <filepath> creates a new file on the disk, the data content is
    // written on the following rows (ended with an empty row)
    int create(std::string filepath);
    // cat <filepath> reads the content of a file and prints it on the screen
    int cat(std::string filepath);
    // ls lists the content in the currect directory (files and sub-directories)
    int ls();

    // cp <sourcefilepath> <destfilepath> makes an exact copy of the file
    // <sourcefilepath> to a new file <destfilepath>
    int cp(std::string sourcefilepath, std::string destfilepath);
    // mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
    // or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
    int mv(std::string sourcepath, std::string destpath);
    // rm <filepath> removes / deletes the file <filepath>
    int rm(std::string filepath);
    // append <filepath1> <filepath2> appends the contents of file <filepath1> to
    // the end of file <filepath2>. The file <filepath1> is unchanged.
    int append(std::string filepath1, std::string filepath2);

    // mkdir <dirpath> creates a new sub-directory with the name <dirpath>
    // in the current directory
    int mkdir(std::string dirpath);
    // cd <dirpath> changes the current (working) directory to the directory named <dirpath>
    int cd(std::string dirpath);
    // pwd prints the full path, i.e., from the root directory, to the current
    // directory, including the currect directory name
    int pwd();

    // chmod <accessrights> <filepath> changes the access rights for the
    // file <filepath> to <accessrights>.
    int chmod(std::string accessrights, std::string filepath);
};

#endif // __FS_H__
