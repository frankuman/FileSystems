#include <iostream>
#include <cstring>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
}

FS::~FS()
{

}

//Userdefined functions
int FS::ReadFromFAT(){
    int status = disk.read(FAT_BLOCK, (uint8_t*)fat);
    std::cout << "DEBUG: Trying to read... status: " << status << std::endl;
    return status;
    
}

/*
std::string FS::getFileName(std::string filepath){

}
*/
int FS::findFreeBlock() {
    int fatIndex;
    for (fatIndex = 2; fatIndex < (BLOCK_SIZE / 2); fatIndex++){
        if (fat[fatIndex] == FAT_FREE){
            break;
        }
    }
    if (fatIndex == (BLOCK_SIZE / 2)) {
        fatIndex = -1;
    }

    return fatIndex;
}

// formats the disk, i.e., creates an empty file system
int
FS::format(){
    std::cout << "FS::format()\n";

    // initializes FAT
    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK] = FAT_EOF;


    for (int i = 2; i < (BLOCK_SIZE / 2); i++)
    {
        fat[i] = FAT_FREE;
    }

    //initialize blocks to all zeroes
    uint8_t root_block[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        root_block[i] = 0;
    }

    int status = disk.write(ROOT_BLOCK, root_block);
    if(!status){
        std::cout << "DEBUG: Disk formatted successfully!" << std::endl;
    }
    return status;
}
int FS::FileEntry(int dir_block, std::string filepath, int& index, dir_entry* dir_entries){

    int status = disk.read(dir_block, (uint8_t*)dir_entries);
    if (status){
        return status;
    }
    
    index = -1;
    for(int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
        if (dir_entries[i].file_name[0] == 0){ //We can place file here
            if(index < 0){
                index = i;
            }
        }
    }
    std::cout << "DEBUG: Found Index for filename at " << index << std::endl;
    // 1. If (Check if a file name with that name already exists)
    // 2. Check access rights (IGNORE FOR NOW)
    return status;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int FS::create(std::string filepath){

    std::cout << "FS::create(" << filepath << ")\n";
    int status = ReadFromFAT();
    if(status){
        return status;
    }

    std::string nameOfFile = filepath;
    dir_info dir;
    
    status = FileEntry(dir.block,filepath,dir.index,dir.entries);
    int current_block = findFreeBlock();



    if(current_block == -1){
        std::cout << "ERROR: No free blocks\n" << std::endl;
        return 1;
    }

    fat[current_block] = FAT_EOF;
    int first_block = current_block;
    char data[BLOCK_SIZE] = {int(0)}; //Data container  memset(data, 0, BLOCK_SIZE);


    std::string line = ".";
    uint32_t size = 0;      // Current block size counter
    uint32_t tot_size = 0;  // File size counter
    //To find the next block
    while(!line.empty()){
        std::getline(std::cin, line);
        if((line.length() + 1 + size) > BLOCK_SIZE){
            int length = BLOCK_SIZE - size;
            tot_size += length;
            memcpy(&data[size], line.c_str(), length);
            std::cout << "DEBUG: Placing " << line.substr(length) << " at block " << current_block << std::endl;

            status = disk.write(current_block, (uint8_t*)data);
            
            if(status){
                return 1;
            }
            memset(data,0,BLOCK_SIZE);
            int new_block = findFreeBlock();
                if (new_block == -1) {
                    std::cout << "ERROR: No free blocks\n";
                    return 1;
                }
            std::cout << "DEBUG: Old block: " << current_block << " New block: " << new_block << std::endl;

            fat[new_block] = FAT_EOF;
            fat[current_block] = new_block;
            current_block = new_block;
            size = 0;
            line = line.substr(length);
            // 0000000/0
        }
        else{
            std::cout << "DEBUG: Placing " << line << " at block " << current_block << std::endl;
            memcpy(&data[size], line.c_str(), line.length() + 1);
            size += line.length() + 1;
            tot_size += line.length();
            line.clear();
        }
    }
    tot_size++;
    status = disk.write(current_block, (uint8_t*)data);
    if(status){
        return status;
    }
    memcpy(dir.entries[dir.index].file_name, nameOfFile.c_str(), nameOfFile.length() + 1);
    dir.entries[dir.index].first_blk = first_block;
    dir.entries[dir.index].size = tot_size;
    dir.entries[dir.index].type = TYPE_FILE;
    dir.entries[dir.index].access_rights = READ | WRITE;
    
    status = disk.write(dir.block, (uint8_t*)dir.entries);
    if (status)
        return status;
    status = writeToFAT();
    if (status)
        return status;
    
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls(){
std::cout << "FS::ls()\n";

    // read FAT from disk to memory
    int status = ReadFromFAT();
    if (status){
        return status;
    }

    // Get the directory entries for the current dir
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = disk.read(curr_blk, (uint8_t*)dir_entries);
    if (status)
        return status;


    // std::cout << "Name \t size" << std::endl; 
    // for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
    //     if (dir_entries[i].file_name[0] != 0){
    //         std::string blocks = std::to_string(curr_blk);
            
    //         while (curr_blk >= 0 && curr_blk < (int)MAX_DIR_ENTRIES && fat[curr_blk] != FAT_EOF) {
    //             curr_blk = fat[curr_blk];
    //             blocks += "," + std::to_string(curr_blk);
    //         }
    //         int curr_blk = dir_entries[i].first_blk;
    //         std::cout << dir_entries[i].file_name << "\t " << dir_entries[i].size << std::endl;
    //     }
    // }

    // Use printf to make the output prettier and more readable
    // Design decision: Show also the used FAT blocks - to make it easier to test the program
    char underline[50];
    memset(underline, '-', 50 - 1);
    underline[50 - 1] = '\0';
    printf("%-*.*s %-*.*s  %-*.*s  %*.*s  %-*.*s\n", 50, 50, "filename", 5, 5, "type", 6, 6, "access", 9, 9, "size", 15, 15, "file-blocks");
    printf("%-*.*s %-*.*s  %-*.*s  %*.*s  %-*.*s\n", 50, 50, underline, 5, 5, underline, 6, 6, underline, 9, 9, underline, 15, 15, underline);
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
            //dir_entries[i].access_rights
            std::string type = dir_entries[i].type == TYPE_FILE ? "File" : "Dir";
            std::string access = "";
            access += (dir_entries[i].access_rights & READ) ? "r" : "-";
            access += (dir_entries[i].access_rights & WRITE) ? "w" : "-";
            access += (dir_entries[i].access_rights & EXECUTE) ? "x" : "-";            
            printf("%-*.*s %-*.*s   %-*.*s %*.d  %s\n", 28, 28, dir_entries[i].file_name, 5, 5, type.c_str(), 5, 5, access.c_str(), 10, dir_entries[i].size, blocks.c_str());
        }
    }

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}


//----------------- OWN FUNCTIONS -----------------



int FS::writeToFAT(){
    int status = disk.write(FAT_BLOCK, (uint8_t*)fat);
    return status;
}
