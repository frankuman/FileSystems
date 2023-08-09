/**
 * @file fs.cpp
 * @author Oliver Bölin & Philippe Van Daele
 * @brief 
 * @version 1
 * @date 2023-08-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */


#include <iostream>
#include <cstring>
#include <cmath>
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
    //std::cout << "DEBUG: Trying to read... status: " << status << std::endl;
    return status;
    
}

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
int FS::format(){
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

    //if(!status){
    //    std::cout << "DEBUG: Disk formatted successfully!" << std::endl;
    //}

    return status;
}

int FS::FindingFileEntry(std::string filepath, uint8_t newOrExisting, dir_info &dir, uint8_t accessrights){
    
    int status = GetDirectoryBlock(filepath, dir.block);
    if(status) return status;


    return FileEntry(dir.block, filepath, dir.index, dir.entries, newOrExisting, accessrights);
}


int FS::GetDirectoryBlock(std::string filepath, int &dir_block){

    //std::cout << "DEBUG: Current filepath: " << filepath << std::endl;
    //std::cout << "DEBUG: Current dir_block: " << dir_block << std::endl; 

    auto pos = filepath.find_last_of('/');
    std::string parent_dir = (pos != std::string::npos) ? filepath.substr(0, pos) : "";
    std::string dirname;

    if(!parent_dir.empty()){
        //Incase mkdir dir1/dir2/subdir1
        //std::cout << "DEBUG 1.2: Parent_dir -> " << parent_dir << std::endl; // REMOVE LATER <------------
        if (parent_dir.front() == '/') {
            // absolute path - starts at the root dir
            dir_block = ROOT_BLOCK;
            parent_dir = parent_dir.substr(1);
        }
        else {
            // relative path - starts at the current dir
            dir_block = curr_blk;
        }
        
        while (!parent_dir.empty()) {
            
            std::size_t index = parent_dir.find('/');
            if(index == std::string::npos){
                dirname = parent_dir;
                parent_dir = "";
                
            }
            else{
                dirname = parent_dir.substr(0,index);
                parent_dir.substr(index + 1);
            }

            int dir_index;
            dir_entry dir_entries[MAX_DIR_ENTRIES];
            int status = FileEntry(dir_block, dirname, dir_index, dir_entries, OLD, READ);
            if (status){
                return status;

            }
            /////// Denna ska blockera skapningen av fil och dirs i dirs med bara read. Nånstans.
            bool access = (dir_entries[dir_index].access_rights & WRITE) == WRITE;
            if (!access) {
                std::cout << "ERROR: The file access of '" << dir_entries[index].file_name << "' is not permitted '\n";
            }
            /////////////
            if (dir_entries[dir_index].type != TYPE_DIR) {
                std::cout << "Error: Filepath is not a directory: " << dirname << "\n";
                return 1;
            }
            dir_block = dir_entries[dir_index].first_blk;
        }

    }
    else{

        if (filepath.front() == '/') {
            // absolute path - starts at the root dir
            dir_block = ROOT_BLOCK;
        }
        else {
            dir_block = curr_blk;
        }
    }
    //std::cout << "DEBUG: Current parent dir: " << parent_dir << std::endl; 

    return 0;
}

int FS::FileEntry(int dir_block, std::string filepath, int &index, dir_entry *dir_entries, uint8_t NewOrOld, uint8_t accessrights)
{

    int status = disk.read(dir_block, (uint8_t*)dir_entries);
    if (status){
        return status;
    }
    std::string filename;



    index = -1;
    if(NewOrOld == 1){
        for(int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
        
            if (dir_entries[i].file_name[0] == 0){ //file here
                if(index < 0){
                    index = i;
                }
            }
            else if(strcmp(dir_entries[i].file_name, filepath.c_str()) == 0){
                index = -2;
                break;
            }
            

        }
    }
    else{
        for(int i = 0; i < (int)MAX_DIR_ENTRIES; i++){

            if (strcmp(dir_entries[i].file_name, filepath.c_str()) == 0){ //Detta behöver skrivas om angående fulla filnamn
                index = i;
            }
    

        }
    }

    if (index == -2) {
        std::cout << "ERROR: File already exists: " << filepath << "\n";
        return 1;
    }
    
    if (accessrights > 0) {
    // Check file access
        if (NewOrOld == OLD) {
            // Existing file: check the requested access to the file
            bool access = (dir_entries[index].access_rights & accessrights) == accessrights;
            if (!access) {
                std::cout << "ERROR: The file access of '" << dir_entries[index].file_name << "' is not permitted '\n";
                return 1;
            }
        }
        else{
            
        }
}
    //std::cout << "DEBUG: Found Index for filename at " << index << std::endl;
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
    
    //status = FileEntry(dir.block,filepath,dir.index,dir.entries, NEW);
    status = FindingFileEntry(filepath,NEW,dir,WRITE);
    if(status){
        return status;
    }

    int curr_blk = findFreeBlock();

    if(curr_blk == -1){
        std::cout << "ERROR: No free blocks\n" << std::endl;
        return 1;
    }
    //std::cout << "CHECK: " << working_directory << dir.entries[dir.index].access_rights << std::endl;

    fat[curr_blk] = FAT_EOF;
    int first_block = curr_blk;
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
            //std::cout << "DEBUG: Placing " << line.substr(length) << " at block " << curr_blk << std::endl;

            status = disk.write(curr_blk, (uint8_t*)data);
            
            if(status){
                return 1;
            }
            memset(data,0,BLOCK_SIZE);
            int new_block = findFreeBlock();
                if (new_block == -1) {
                    std::cout << "ERROR: No free blocks\n";
                    return 1;
                }
            //std::cout << "DEBUG: Old block: " << curr_blk << " New block: " << new_block << std::endl;

            fat[new_block] = FAT_EOF;
            fat[curr_blk] = new_block;
            curr_blk = new_block;
            size = 0;
            line = line.substr(length);
            // 0000000/0
        }
        else{
            //std::cout << "DEBUG: Placing " << line << " at block " << curr_blk << std::endl;
            memcpy(&data[size], line.c_str(), line.length() + 1);
            size += line.length() + 1;
            tot_size += line.length();
            line.clear();
        }
    }
    tot_size++;
    status = disk.write(curr_blk, (uint8_t*)data);
    if(status){
        return status;
    }
    memcpy(dir.entries[dir.index].file_name, nameOfFile.c_str(), nameOfFile.length() + 1);
    dir.entries[dir.index].first_blk = first_block;
    dir.entries[dir.index].size = tot_size;
    dir.entries[dir.index].type = TYPE_FILE;
    dir.entries[dir.index].access_rights = READ | WRITE;
    
    status = disk.write(dir.block, (uint8_t*)dir.entries);
    if (status){
        return status;
    }

    status = writeToFAT();
    if (status){
        return status;
    }
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";
    int status = ReadFromFAT();
    if(status){
        return status;
    }

    dir_info dir;
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = FileEntry(dir.block,filepath,dir.index,dir.entries,OLD, READ);
    if(status){
        return status;
    }

    if (!(dir.entries[dir.index].access_rights & READ)) {
        std::cout << "You do not have access rights! :(\n";
        return 0;
    }
    int file_block = dir.entries[dir.index].first_blk;
    

    uint32_t size = 0;
    uint32_t tot_size = 0;

    char data[BLOCK_SIZE];

    status = disk.read(file_block, (uint8_t*)data);
    if(status){
        return status;
    }
    int length = 0;

    
    while(tot_size < dir.entries[dir.index].size){
        for (int i = size; i < BLOCK_SIZE; i++, length++){
            if (data[i] == '\0'){
                break;
            } 
        }
        std::cout << &data[size] << "\n";
        size += length;
        tot_size += length;
        size++;
        tot_size++;
        std::cout << "\n";
    }
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls(){

    std::cout << "FS::ls()\n";

    // read FAT from disk to memory
    int status = ReadFromFAT();
    if (status){
        return status;
    }
    
    
    // Get the directory entries for the current dir
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = disk.read(curr_blk, (uint8_t*)dir_entries);
    if (status){
        return status;
    }
    std::string cwd = working_directory;


    std::cout << "Name \t Size \t Access rights \t Type " << std::endl;
    std::cout << "---- \t ---- \t ------------- \t ---- " << std::endl;
    //std::cout << "CHECK: " << working_directory << dir_entries[curr_blk].access_rights << std::endl;
    std::cout << "CHECK: " << curr_blk << std::endl;


    

    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
        if (dir_entries[i].file_name[0] != 0){
            std::string blocks = std::to_string(curr_blk);
            std::string filetype;
            std::string access_right = "";
        
            if (dir_entries[i].access_rights & READ) {
                access_right += "r";
            } else {
                access_right += "-";
            }

            if (dir_entries[i].access_rights & WRITE) {
                access_right += "w";
            } else {
                access_right += "-";
            }

            if (dir_entries[i].access_rights & EXECUTE) {
                access_right += "x";
            } else {
                access_right += "-";
            }

            int curr_blk = dir_entries[i].first_blk;
            if (curr_blk != ROOT_BLOCK){
                if(dir_entries[i].type == 0){
                    std::cout << dir_entries[i].file_name << "\t "<< dir_entries[i].size << "\t " << access_right << "\t" << "\t" << " file" << std::endl;
                }
                else{
                    std::cout << dir_entries[i].file_name << "\t " << " - " << "\t " << access_right << "\t" << "\t" << " dir" << std::endl;
                }
            }

        }
    }

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath){
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";

    int status = ReadFromFAT();
    if (status){
        return status;
    }

    //check if the desitnation file already exists
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = disk.read(curr_blk, (uint8_t*)dir_entries);
    if (status){
        return status;
    }

    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
    if (dir_entries[i].file_name[0] != 0){
        std::string blocks = std::to_string(curr_blk);
        
            if(dir_entries[i].file_name == destpath){
                std::cout << "Error: The destination file already exists!" << std::endl;
                return 0;
            }
        }
    }

    dir_info source;
    dir_info destination;

    status = FindingFileEntry(sourcepath, OLD, source, READ);
    if(status) return status;
    status = FindingFileEntry(destpath, NEW, destination, WRITE);
    if(status) return status;

    char data[BLOCK_SIZE];
    memset(data, 0, BLOCK_SIZE);

    int first_block = -1;
    int previous_block = -1;
    int source_block = source.entries[source.index].first_blk;
    while(source_block != FAT_EOF){

        //finding FAT free entry
        int curr_blk = findFreeBlock();
        if(curr_blk < 0){
            std::cout << "Error: There isn't anymore free blocks in FAT" << std::endl;
            return 1;
        }

        if(first_block == -1){
            first_block = curr_blk;
        }
        if(previous_block != -1){
            fat[previous_block] = curr_blk;
        }
        fat[curr_blk] = FAT_EOF;

        status = disk.read(source_block, (uint8_t*)data);
        if(status) return status;
        status = disk.write(curr_blk, (uint8_t*)data);
        if(status) return status;

        previous_block = curr_blk;
        source_block = fat[source_block];
    }

    std::cout << "Destination index: " << destination.index << std::endl;

    //copying the file entry info from source to destination
    memcpy(&destination.entries[destination.index], &source.entries[source.index], sizeof(dir_entry));
    
    //set the new filename and the new first-block
    memcpy(destination.entries[destination.index].file_name, destpath.c_str(), destpath.length() + 1);
    destination.entries[destination.index].first_blk = first_block;

    status = disk.write(destination.block, (uint8_t*)destination.entries);
    if(status) return status;

    status = writeToFAT();
    if(status) return status;

    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";

    dir_info source;
    dir_info destination;
    
    int status = FindingFileEntry(sourcepath, OLD, source, READ);
    if(status) return status;
    status = FindingFileEntry(destpath, NEW, destination, WRITE);
    if(status) return status;

    //check if the desitnation file already exists
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = disk.read(curr_blk, (uint8_t*)dir_entries);
    if (status){
        return status;
    }

    for (int i = 0; i < (int)MAX_DIR_ENTRIES; i++){
        if (dir_entries[i].file_name[0] != 0){
            std::string blocks = std::to_string(curr_blk);

            if(dir_entries[i].file_name == destpath){
                std::cout << "Error: The destination file already exists!" << std::endl;
                return 0;
            }
        }
    }

    if(destination.block == source.block){ // The source file exists on the same level as the destination file
        memcpy(source.entries[source.index].file_name, destpath.c_str(), destpath.length() + 1);

        status = disk.write(source.block, (uint8_t*) source.entries);
        if(status){
            return status;
        }
    }

    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath){
    std::cout << "FS::rm(" << filepath << ")\n";
    
    int status = ReadFromFAT();
    if(status) return status;
    
    dir_info source;
    status = FindingFileEntry(filepath, OLD, source, WRITE);
    if(status) return status;
    std::string pathname;
    get_dir_name(working_directory,pathname);
    if(filepath == pathname){
        std::cout << "ERROR: You can't remove the current directory your are in " << pathname << std::endl;
        return 1;
    }
    int disk_block = source.entries[source.index].first_blk;
    while(disk_block != FAT_EOF){
        int next_block = fat[disk_block];
        fat[disk_block] = FAT_FREE;
        disk_block = next_block;
    }

    //writing the empty block to disk
    memset(&source.entries[source.index], 0, sizeof(dir_entry));
    status = disk.write(source.block, (uint8_t*)source.entries);
    if (status) return status;
    
    status = writeToFAT();
    if (status) return status;
    
    return 0;
}

int FS::get_file_string(std::string filepath,std::string &text){
    //This function works the same as cat but modifies the reference text to be the text of the filepath. 
    int status = ReadFromFAT();
    if(status){
        return status;
    }

    dir_info dir;
    dir_entry dir_entries[MAX_DIR_ENTRIES];
    status = FileEntry(dir.block,filepath,dir.index,dir.entries,OLD,READ);
    if(status){
        return status;
    }

    int file_block = dir.entries[dir.index].first_blk;
    

    uint32_t size = 0;
    uint32_t tot_size = 0;

    char data[BLOCK_SIZE];

    status = disk.read(file_block, (uint8_t*)data);
    if(status){
        return status;
    }
    int length = 0;

    
    while(tot_size < dir.entries[dir.index].size){
        for (int i = size; i < BLOCK_SIZE; i++, length++){
            if (data[i] == '\0'){
                break;
            } 
        }
        text += &data[size];
        size += length;
        tot_size += length;
        size++;
        tot_size++;
        
    }
    return 0;

}


int FS::get_free_blocks(int* free_blocks,int amount_blocks,int start_block){
    //get new
    int status = disk.read(FAT_BLOCK, (uint8_t*) &fat);
    if(status){
        return status;
    }

    int free_blocks_counter = 0;
    //calculates free blocks
    for(int i = 2; i < BLOCK_SIZE/2 && free_blocks_counter < amount_blocks; i++){
        if(fat[i] == FAT_EOF){
            free_blocks[free_blocks_counter+start_block] = i;
            free_blocks_counter++;
        }
    }
    //std::cerr << "DEBUG: Free blocks: " << *free_blocks << std::endl;

    return 0;
}
int FS::update_FAT(int* free_block, int block_amount){
    // WARNING THIS IS COPYPASTED
    if (disk.read(FAT_BLOCK, (uint8_t*) &fat) < 0 ) {
        std::cerr << "Error: FAT encountered disk error" << std::endl;
        return -1;
    }

	// link FAT
	for (int i = 0; i < block_amount-1; i++){
        // if no free blocks in FAT was reached (incase other function than get_free_blocks was used)
        if (i >= BLOCK_SIZE/2) {
            std::cerr << "Error: Disk is full" << std::endl;
            return -1;
        }

		fat[free_block[i]] = free_block[i+1];
	}
	fat[free_block[block_amount-1]] = FAT_EOF;

	// FAT update
	if (disk.write(FAT_BLOCK, (uint8_t*) &fat) != 0){
		std::cerr << "Error: FAT encountered disk error" << std::endl;
		return -1;
	}
    //std::cerr << "DEBUG: No Error - Disk updated" << std::endl;
	return 0;
}
int FS::write_block(std::string text, int block_amount, int* free_blocks){
    int status = update_FAT(free_blocks,block_amount);
    if(status) return status;
    char c_write[BLOCK_SIZE];
    
    for(int i = 0; i < block_amount -1; i++){
        text.copy(c_write, BLOCK_SIZE, i*BLOCK_SIZE);
        //std::cout << "DEBUG- text: " << text << std::endl;

        if(disk.write(free_blocks[i], (uint8_t*) &c_write) != 0){
            std::cerr << "Error: Couldnt write" << std::endl;

        }
        
    }
    //Write the last block
    text.copy(c_write,text.length()%BLOCK_SIZE,(block_amount-1)*BLOCK_SIZE);
    c_write[text.length()%BLOCK_SIZE] = '\0';
    disk.write(free_blocks[block_amount-1],(uint8_t*) &c_write);
    //std::cout << "DEBUG- c_write: " << c_write << std::endl;
    return 0;
}
// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string sourcepath, std::string destinationpath){
    int status = ReadFromFAT();

    std::cout << "FS::append(" << sourcepath << "," << destinationpath << ")\n";
    std::string temp_append_text;
    get_file_string(sourcepath,temp_append_text);
    std::string append_text = "\n" + temp_append_text;
    
    //Find first block of paths
    dir_info source;
    dir_info destination;
    status = FindingFileEntry(sourcepath, OLD, source, READ);
    if(status) return status;
    status = FindingFileEntry(destinationpath, OLD, destination, READ | WRITE);
    if(status) return status;

    if (!(source.entries[source.index].access_rights & READ)) {
        std::cout << "You do not have access rights to read from source! :(\n";
        return 0;
    }
    if (!(destination.entries[destination.index].access_rights & WRITE)) {
        std::cout << "You do not have access rights to write to destination! :(\n";
        return 0;
    }
    int source_block = source.entries[source.index].first_blk;
    int destination_block = destination.entries[destination.index].first_blk;
    int source_size = source.entries[source.index].size;
    int destination_size = destination.entries[destination.index].size;
    int tot_size = destination_size + source_size;

    //We need the latest fat from disk
    if(disk.read(FAT_BLOCK,(uint8_t*) &fat)< 0){
        return -1;
    }
    while(fat[destination_block] != FAT_EOF){
        destination_block = fat[destination_block];
    }

    char text_buffer[BLOCK_SIZE];

    if(disk.read(destination_block,(uint8_t*) &text_buffer)<0){
        return -1;
    }
    append_text = text_buffer + append_text;
    //Calculation of how many blocks file will be
    int block_amount = ceil((append_text.length()+1) / (float) BLOCK_SIZE);
    if(block_amount < 0){
        return -1;
    }
    destination.entries[destination.index].size = tot_size;


    int free_blocks[block_amount+1];

    free_blocks[0] = destination_block;
    
    status = (get_free_blocks(free_blocks,block_amount-1,1));    
    if(status){
        return status;
    }
    status = write_block(append_text,block_amount,free_blocks);
    //std::cout << "DEBUG: ( SOURCEBLOCK: " << source_block << ", DESTBLOCK: " << destination_block << ")\n";
    //std::cout << "DEBUG: ( append_text: " << append_text << ")\n";
    //std::cout << "DEBUG: ( BLOCK AMOUNT: " << block_amount << ")\n";
    //std::cout << "DEBUG: ( FREE AMOUNT: " << *free_blocks << ")\n";
    //std::cout << "DEBUG: ( SOURCESIZE: " << source_size << ", DESTSIZE: " << destination_size << ")\n";
    //std::cout << "DEBUG: ( NEW SIZE: " << destination.entries[destination.index].size << ")\n";

    if(status){
        return status;
    }
    //Path things need to happen here (or does it?)
    //memcpy(destination.entries[destination.index].size, tot_size, tot_size. + 1);

    status = disk.write(destination.block, (uint8_t*)destination.entries);
    status = writeToFAT();
    if(status){
        return status;
    }
    return 0;

}
int FS::get_dir_name(std::string path,std::string &name){
    //We should extract the path name here
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos){
        return -1;
    }
    name = path.substr(pos + 1);
    return 0;
}

int FS::mkdir(std::string dirpath)
{

    std::cout << "FS::mkdir(" << dirpath << ")\n";
    //<- check if dir(path) doesn't already exist
    std::string name;
    dir_info dir;
    int status = get_dir_name(dirpath,name);
    if(status){
        name = dirpath;
    }
    std::cout << "DEBUG: dirname: " << name << "\n";

    //<- check if parent dir exists or if valid path
    //<- check access rights
    /***/
    
    if(name == PARENT_DIR){
        std::cerr << "Error:" << PARENT_DIR << " is for the parent dir" << std::endl;
    }
    status = ReadFromFAT();
    if(status){
        return status;
    }
    
    status = FindingFileEntry(dirpath,NEW,dir,WRITE);
    if(status){
        return status;
    }
    int free_block = findFreeBlock();
    fat[free_block] = FAT_EOF;
    memcpy(dir.entries[dir.index].file_name, name.c_str(), name.length() + 1);
    dir.entries[dir.index].first_blk = free_block;
    dir.entries[dir.index].size = 0;
    dir.entries[dir.index].type = TYPE_DIR;
    dir.entries[dir.index].access_rights = READ | WRITE | EXECUTE;
    status = disk.write(dir.block, (uint8_t*)dir.entries);
    if(status){
        return status;
    }
    memset(dir.entries, 0, BLOCK_SIZE);
    dir.index = 0;
    memcpy(dir.entries[dir.index].file_name, PARENT_DIR.c_str(), PARENT_DIR.length() + 1);
    dir.entries[dir.index].first_blk = dir.block;
    dir.entries[dir.index].size = 0;
    dir.entries[dir.index].type = TYPE_DIR;
    dir.entries[dir.index].access_rights = READ | WRITE | EXECUTE;

    status = disk.write(free_block, (uint8_t*)dir.entries);
    if(status){
        return status;
    }
    status = writeToFAT();
    if(status){
        return status;
    }
    return 0;
}

int FS::cd(std::string dirpath) {
    working_directory += "/" + dirpath;
    if (dirpath == "/" || dirpath == PARENT_DIR) {
        goHome();
        return 0;
    }

    removeTrailingSlash(dirpath);

    dir_info dir;
    int status = FindingFileEntry(dirpath, OLD, dir, READ);
    if (status != 0) {
        std::cout << ("1'" + dirpath + "' is not a directory") << std::endl;
        return status;
    }

    if (dir.entries[dir.index].type != TYPE_DIR) {
        std::cout << ("2'" + dirpath + "' is not a directory") << std::endl;
        return -1;
    }

    curr_blk = dir.entries[dir.index].first_blk;
    //std::cout << ("DEBUG: Directory changed to '" + dirpath + "'") << std::endl;
    return 0;
}

void FS::goHome() {
    curr_blk = ROOT_BLOCK;
    working_directory = "..";
    //std::cout << ("DEBUG: Directory changed to root sucessfully") << std::endl;
}

void FS::removeTrailingSlash(std::string& str) {
    if (!str.empty() && str.back() == '/') {
        str.pop_back();
    }
}

// hmh
//  pwd prints the full path, i.e., from the root directory, to the current
//  directory, including the currect directory name
int FS::pwd(){
    std::cout << "FS::pwd()\n";
    std::cout << working_directory << std::endl;

    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
        
    dir_info dir;
    int status = ReadFromFAT();
    if (status){
        return status;
    }
    status = FindingFileEntry(filepath, OLD, dir, 0);
    if(status){
        std::cout << "ERROR: That doesn't exist" << std::endl;

    }
    std::string pathname;
    get_dir_name(filepath,pathname);
    if(pathname == PARENT_DIR){ 
        std::cout << "ERROR: You can't modify the parent directory of " << PARENT_DIR << std::endl;
        return 1;
    }
    uint8_t accessInt = std::stoi(accessrights);
    dir.entries[dir.index].access_rights = accessInt;
    status = disk.write(dir.block, (uint8_t*)dir.entries);
    if (status){
        return status;
    }
    
    // WRITE = 4
    // READ = 2
    // X = 1
    // 4 >= 4, Ja, WRITE
 
    // if (status != 0) {
    //     std::cout << (filepath + "' is not a directory") << std::endl;
    //     return status;
    // }
    // if (dir.entries[dir.index].type != TYPE_DIR) {
    //     std::cout << (filepath + "' is not a directory") << std::endl;
    //     return -1;
    // }


    return 0;
    // // chmod rw- dir/file1
    // std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";

    // uint8_t access_rights = std::stoi(accessrights, nullptr, 0);
    // std::string filename;
    // get_dir_name(filepath,filename);
    
    // if (filename == PARENT_DIR){
    //     std::cout << "Error: parent dir '" << PARENT_DIR << "' cannot be modified.\n";
    //     return 1;
    // }

    // // Find the directory index for the passed file and read directory block into memory.
    // dir_info dir;
    // int sts = FindingFileEntry(filepath, OLD, dir, 0);  // Note! If we don't allow full access for chmod we cannot change a read-only file
    // if (sts) return sts;

    // dir.entries[dir.index].access_rights = access_rights;
    // sts = disk.write(dir.block, (uint8_t*)dir.entries);
    // if (sts)
    //     return sts;

    // return 0;
}
//----------------- OWN FUNCTIONS -----------------

int FS::writeToFAT(){
    //writes to FAT
    int status = disk.write(FAT_BLOCK, (uint8_t*)fat);
    return status;
}



