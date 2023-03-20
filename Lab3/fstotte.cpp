/**
 * @file fs.cpp
 * @author Totte Hansen (toha20@student.bth.se)
 * @author Gustav Eriksson (guny20@student.bth.se)
 *
 * @brief Filessystem operations and disk handeling.
 * @version 0.8
 * @date 2022-01-05
 *
 * @copyright None
 *
 */

#include <iostream>
#include <fstream>
#include <string.h>
#include <math.h>
#include <sstream> //! only used once

#include "fs.h"

FS::FS() {
    std::cout << "Creating file system\n";
    current_path = ROOT_BLOCK;
}

FS::~FS() { }

//* -- Public functions -- *//

// formats the disk, i.e., creates an empty file system
int FS::format() {
    std::cout << "Formatting..." << std::endl;

    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK] = FAT_EOF;
    // block 0 and 1 kept intact for root and FAT respectevly
    for (int i = 2; i < BLOCK_SIZE/2; i++){
        // clear FAT
        fat[i] = FAT_FREE;
    }
    if (disk.write(FAT_BLOCK, (uint8_t*) &fat) < 0){
        std::cout << "Error: Formatting error has occured" << std::endl;
        return -1;
    }

    struct dir_entry dir_buffer[entry_per_blk]; // dir buffer with empty names
    // minimum size of a dir (aka root size after format)
    int min_size = sizeof(dir_entry)*2;
    // set ``.`` (current dir)
    set_entry(dir_buffer, (std::string) ".", min_size,
        ROOT_BLOCK, TYPE_DIR, DOT_INDX);
    // set ``..`` (previous dir)
    set_entry(dir_buffer, (std::string) "..", min_size,
        ROOT_BLOCK, TYPE_DIR, DOTDOT_INDX);

    // error handle
    if (disk.write(ROOT_BLOCK, (uint8_t*) &dir_buffer) < 0) {
        std::cout << "Error: Formatting error has occured" << std::endl;
        return -1;
    }

    current_path = ROOT_BLOCK;

    std::cout << "Format successful." << std::endl;

    return 0;
}


// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
//! NOTE, many terminals do not allow a line longer than 4096 chars,
int FS::create(std::string filepath) {
    // get file contents from user
	std::string str_in;
    std::getline(std::cin, str_in);

    if (write_file(filepath, str_in) < 0) { return -1; }

    return 0;
}


// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath) {
    // string to for read_file to pass contents to
    std::string read_str;
	if (read_file(filepath, read_str) < 0) { return -1; }

    // print file contents
    std::cout << read_str << std::endl;

	return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls(){
	struct dir_entry dir_buffer[entry_per_blk]; // Dir buffer.

	// Read current directory.
	disk.read(current_path, (uint8_t*) &dir_buffer);

    // check read permissions
    if (!(dir_buffer[DOT_INDX].access_rights & READ)) {
        std::cerr << "Erorr: Read permission denied." << std::endl;
        return -1;
    }

    // headers
    std::cout << "Name \t| Size \t| Type \t| Access \n" 
        << "---------------------------------" << std::endl;

	for (int i = 0; i < entry_per_blk; i++){
		// Empty name, empty entry.
		if (dir_buffer[i].file_name[0] != '\0'){
			// Add name and size to write out.
			std::cout << dir_buffer[i].file_name <<  "\t| "
			<< dir_buffer[i].size;

			// Add type to out buffer.
			if (dir_buffer[i].type == TYPE_DIR) { std::cout << "\t| Dir "; }
			else 								{ std::cout << "\t| File"; }

			std::cout << "\t| ";

			// Add access rights to out buffer.
			if (dir_buffer[i].access_rights & READ) 	{ std::cout << "R"; }
            else                                        { std::cout << "-"; }
			if (dir_buffer[i].access_rights & WRITE) 	{ std::cout << "W"; }
            else                                        { std::cout << "-"; }
			if (dir_buffer[i].access_rights & EXECUTE) 	{ std::cout << "X"; }
            else                                        { std::cout << "-"; }

			std::cout << std::endl;
		}

	}

	return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath) {
    // destpaths validity is checked in write_file()
	if (check_valid_filepath_length(sourcepath) != 0)  { return -1; }

    //* -- check if destpath is dir -- *//
    // get first_blk
    int dest_blk = find_path(destpath, TYPE_DIR, true);
    // -1, path is not found, error
    if (dest_blk == -1) {
        std::cerr << "Error: Destination path does not exist" << std::endl;
    }
    // != -2, path is not dir (wrong type error in find_path)
    else if (dest_blk != -2) {
        // name should be same as source: 
        // get name
        char name[56];
        get_name(sourcepath, name, false);

        // append name to dest
        destpath += "/";
        destpath += name;
    }



    //* -- copy -- */
	std::string source_data;
    // read contents of source
	if (read_file(sourcepath, source_data) < 0) { return -1; }

    // write source content to destination (takes up different blocks)
	if (write_file(destpath, source_data) != 0) { return -1; }

	return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath) {
    // holds name of destination entry
    char dest_name[56];
    // holds name of source entry
    char source_name[56];

    //* -- correctness checking -- *//
    // check if source path exist
    struct dir_entry source_entry;
    if (cpy_entry(sourcepath, source_entry) < 0) {
        std::cerr << "Error: Source path does not exist" << std::endl;
        return -1;
    }
    // check read/write permissions to remove and copy entry
    if ((source_entry.access_rights & (READ+WRITE)) != READ+WRITE) {
        std::cerr << "Error: Read/write permission denied." << std::endl;
        return -1;
    }

    // get name for entry to be copied from, and removed
    if (get_name(sourcepath, source_name, true) < 0) { return -1; }
    // declare temporary block storage, and define block to read entry from
    int source_blk = find_path(sourcepath, TYPE_DIR);
    if (source_blk < 0) { return -1; }

    // holds a copy of destination entry (if it exists)
    struct dir_entry dest_entry;
    // check if entry aldready exist
    if (find_path(destpath) > -1) {
        // get existing destination entry
        if (cpy_entry(destpath, dest_entry) < 0) { return -1; }

        // file of same name exists
        if (dest_entry.type == TYPE_FILE) {
            std::cerr << "Error: Destination path already exist." << std::endl;
            return -1;
        }

        // check destinations entry permissions
        if (!(dest_entry.access_rights & WRITE)) {
            std::cerr << "Error: Write access denied." << std::endl;
            return -1;
        }

        // dest name will remain same as source name if copied to dir
        strcpy(dest_name, source_name);
    }
    // destination is not occupied and file is being moved
    else {
        // get new file name and dir destpath
        if (get_name(destpath, dest_name, true) < 0) { return -1; }

        // copy dest dir parent to check existance and permissions
        if (cpy_entry(destpath, dest_entry) < 0)     { return -1; }
        if (!(dest_entry.access_rights & WRITE)) {
            std::cerr << "Error: Write permission denied." << std::endl;
            return -1;
        }
    }


    // get dir to write to
    int dest_blk = dest_entry.first_blk;

    //* -- sourcepath update -- *//
    struct dir_entry source_buffer[entry_per_blk]; // dir buffer
    // get dir for entry to be moved
    if (disk.read(source_blk, (uint8_t*) &source_buffer) < 0) { return -1; }

    // temporarily stores entry to be moved to new dir
    if (cpy_entry(source_buffer, source_entry, source_name) < 0) { return -1; }
    // remove entry from dir
    if (remove_entry(source_buffer, source_name) < 0) { return -1; }
    // decrement sourcepath sizes by one entry
    if (update_dir_size(source_buffer, -(int)sizeof(dir_entry)) != 0) {
        return -1;
    }


    //* -- destpath update -- *//
    // set entry to new name
    strcpy(source_entry.file_name, dest_name);

    // if source and dest is in same dir, reuse source_buffer
    if (dest_blk == source_blk) {
        // increment destpath dir by one enrty
        if (update_dir_size(source_buffer, sizeof(dir_entry)) < 0) { return -1; }

        // set entry to dest_path directory
        if (set_entry(source_buffer, source_entry)) { return -1; }
    }
    // if source and dest are in different load into another buffer
    else {
        // buffer for dir to store moved entry
        struct dir_entry dest_buffer[entry_per_blk]; // dir buffer
        if (disk.read(dest_blk, (uint8_t*) &dest_buffer) < 0) { return -1; }

        // increment destpath dir by one enrty
        if (update_dir_size(dest_buffer, sizeof(dir_entry)) < 0) { return -1; }

        // set entry to dest_path directory
        if (set_entry(dest_buffer, source_entry)) { return -1; }

        // update dest dir to disk
    if (disk.write(dest_blk, (uint8_t*) &dest_buffer) < 0) { return -1; }
    }

    //* -- disk update -- *//
    // updates after main code. if code fails, no updates will be made
    // update source dir to disk
    if (disk.write(source_blk, (uint8_t*) &source_buffer) < 0) { return -1; }

    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath) {
	struct dir_entry dir_buffer[entry_per_blk]; // dir buffer
	// load fat
	disk.read(FAT_BLOCK, (uint8_t*) &fat);

	// stores a copy of path entry for checking attributes
	struct dir_entry entry;
	if (cpy_entry(filepath, entry) < 0) { return -1; }

	//* -- rm dir -- *//
	if (entry.type == TYPE_DIR) {
		disk.read(entry.first_blk, (uint8_t*) &dir_buffer);

		// error handling for parent write permissions
		if (!(dir_buffer[1].access_rights & WRITE)) {
			std::cout  << "Error: Read access denied." << std::endl;
			return -1;
		}

		// check if dir ONLY contains "." and ".." at index 0-1
		for (int i = 2; i < entry_per_blk; i++){
			// Empty name, empty entry
			if (dir_buffer[i].file_name[0] != '\0'){
				// entry exists, dir cannot be removed
				std::cerr << "Error: Can not remove non-empty directory."
				<< std::endl;
				return -1;
			}
		}

		// cannot remove ROOT dir
		if (entry.first_blk == ROOT_BLOCK) {
			std::cerr << "Error: Can not remove root directory."
				<< std::endl;
			return -1;
		}

		// cannot remove current path
		if (entry.first_blk == current_path) {
			std::cerr << "Error: Can not remove current standing path."
				<< std::endl;
			return -1;
		}

		// remove from FAT
		fat[entry.first_blk] = FAT_FREE;

	}
	//* -- rm file -- *//
	else {
		// error handling for parent write permissions
		char rm_name[56];
		std::string parent_path = filepath;
		get_name(parent_path, rm_name, true);

		int parent_blk = find_path(parent_path);
		disk.read(parent_blk, (uint8_t*) &dir_buffer);

		if (!(dir_buffer[0].access_rights & WRITE)) {
			std::cout  << "Error: Read access denied." << std::endl;
			return -1;
		}
		// error handling finished.

		int file_blk = entry.first_blk;

		// sets all fAT blocks of file to FAT_FREE
		int nxt_blk; //buffer to find EOF
		while (file_blk != FAT_EOF) {
			nxt_blk = fat[file_blk];
			fat[file_blk] = FAT_FREE;
			file_blk = nxt_blk;
		}
	}

	// get dir block
	char name_buffer[56];
	get_name(filepath, name_buffer, true);      // split name from path


	int dir_blk = find_path(filepath);          // get block nr of parent
	disk.read(dir_blk, (uint8_t*) &dir_buffer); // load dir

	if (remove_entry(dir_buffer, name_buffer) < 0) {
		return -1;
	}

	// decrement sizes by one enrty
	if (update_dir_size(dir_buffer, -(int) sizeof(dir_entry)) < 0) { return -1;}

	// write dir to disk
	if (disk.write(dir_blk, (uint8_t*) &dir_buffer) < 0) { return -1; }
	// FAT update
	if (disk.write(FAT_BLOCK, (uint8_t*) &fat) < 0)         { return -1; }

	return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string source_path, std::string dest_path) {
    //* -- read source -- *//
    // get contents to be appended from source path
    // (correctness of source_path handled in read_file)
    std::string append_buffer; // stores contents of sourcefil
    if (read_file(source_path, append_buffer) < 0) { return -1; }
    // add newline to append_buffer becuase specs say so
    append_buffer = "\n" + append_buffer;

    //* -- read dest -- *//
    // get first block of destinations path
    int source_blk = find_path(source_path, TYPE_FILE);
	int dest_blk = find_path(dest_path, TYPE_FILE);
	// A lot of error handling for a nicer ourput.
	if (source_blk < 0 || dest_blk < 0){
		if (source_blk == -1 || dest_blk == -1){
			std::cerr << "Error: Path error" << std::endl;
			return -1;
		}
		else if (source_blk < -1 && dest_blk < -1){
			std::cerr << "append: " << source_path << " & " <<
			dest_path << ": Are both directories" << std::endl;
			return -1;
		}
		else if (source_blk < -1 || dest_blk < -1){
			std::cerr << "append: ";
			if (source_blk < -1) {std::cerr << source_path;}
			else						{std::cerr << dest_path;}
			std::cerr << ": Is a directory. " << std::endl;
			return -1;
		}
	}

    if (dest_blk < 0) {
        std::cerr
            << "Error: Destination path '" << dest_path
            << "' does not exist, or is a directory." << std::endl;
        return -1;
    }

	// error handling for write permissions
    struct dir_entry entry;
    if (cpy_entry(dest_path, entry)) { return -1; }
    if (!(entry.access_rights & WRITE)) {
        std::cout  << "Error: Write access denied." << std::endl;
        return -1;
    }

    // get latest fat from disk
    if (disk.read(FAT_BLOCK, (uint8_t*) &fat) < 0) { return -1; }
    // update dest_path do path to block to append to (aka last file block)
    while (fat[dest_blk] != FAT_EOF) { dest_blk = fat[dest_blk]; }

    // get contents of last (non-full) block and prepend to append_buffer
    char char_buffer[BLOCK_SIZE];
    if (disk.read(dest_blk, (uint8_t*) &char_buffer) < 0) { return -1; };
    append_buffer = char_buffer + append_buffer;


    //* --  write dest -- *//
    // amount of blocks file has to occupy
    int blk_amount = ceil((append_buffer.length()+1) / (float) BLOCK_SIZE);
    if (blk_amount < 0) { return -1; }

    // buffer for free blocks + EOF block of dest_path
    int free_blks[blk_amount+1];
    // set EOF to first available block
    free_blks[0] = dest_blk;
    // get blocks to write to (excluding dest_blk, which is already known)
    if (get_free_blocks(free_blks, blk_amount-1, 1) < 0) { return -1; }
    // update fat and write to disk
    if (write_to_block(append_buffer, blk_amount, free_blks) < 0) { return -1; }


    //* -- update dest size -- *//
    // split source name from path
    char name[56];
    get_name(source_path, name, true);

    // buffer to hold a dir
    struct dir_entry dir_buffer[entry_per_blk];
    // load dir of source
    int path_blk = find_path(source_path);
    disk.read(path_blk, (uint8_t*) &dir_buffer);
    // get size (excluding EOL char)
    int source_size = dir_buffer[get_entry_indx(dir_buffer, name)].size - 1;

    // split dest name from path (reuse name buffer)
    get_name(dest_path, name, true);

    // if source and dest is not in same dir, load dest dir
    if (source_path != dest_path) {
        // read dest entry from dir
        path_blk = find_path(dest_path);
        disk.read(path_blk, (uint8_t*) &dir_buffer);
    }

    // dest size = destsize + source_size
    dir_buffer[get_entry_indx(dir_buffer, name)].size += source_size;

    // update dir with new entry size
    disk.write(path_blk,(uint8_t*) &dir_buffer);

    return 0;

}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath) {
    // check if dir already exist
    if (find_path(dirpath, TYPE_DIR, true) != -1) {
        std::cerr << "Error: Path already exist." << std::endl;
        return -1;
    }

    // split new dir name from path
    char name[56];
    if (get_name(dirpath, name, true) < 0) { return -1; }

    // get block to read parent dir
    int parent_blk = find_path(dirpath, TYPE_DIR);
    // check path existance of parent
    if (parent_blk < 0) {
        std::cerr << "Error: Path does not exist." << std::endl;
        return -1;
    }

    struct dir_entry dir_buffer[entry_per_blk]; // dir buffer
    // get dir parent of new sub-dir
    disk.read(parent_blk, (uint8_t*) &dir_buffer);

    // check write permissions of parent
    if (!(dir_buffer[DOT_INDX].access_rights & WRITE)) {
        std::cerr << "Error: Write permission denied." << std::endl;
        return -1;
    }


    // get free block for sub-dir (is above other code to abort in case of error)
    int sub_blk = get_free_block();
    if (sub_blk < 0) { return -1; }

    //* -- update parent dir -- *//
    // get parent dir to be updated
    // check space availability
    if (dir_buffer[DOT_INDX].size >= BLOCK_SIZE) {
        std::cerr << "Error: Directory full. Can not append." << std::endl;
        return -1;
    }

    // mininum size of a dir
    int min_size = 2*sizeof(dir_entry);
    // update parent size, and the parents parents by one entry
    update_dir_size(dir_buffer, sizeof(dir_entry));

    // add new entry to parent
    if (set_entry(dir_buffer, name, min_size, sub_blk, TYPE_DIR) < 0) {
        return -1;
    }

    // update parent to disk
    if (disk.write(parent_blk, (uint8_t*) &dir_buffer) < 0) { return -1; }


    //* -- update new sub-dir -- *//
    // udate FAT
    disk.read(FAT_BLOCK, (uint8_t*) &fat);
    fat[sub_blk] = FAT_EOF;
    disk.write(FAT_BLOCK, (uint8_t*) &fat);

    // get parent size for sub-dir ".."
    int parent_size = dir_buffer[DOT_INDX].size;
    int parent_prms = dir_buffer[DOT_INDX].access_rights;
    // clear buffer names to reuse for sub-dir
    for (int i = 0; i < entry_per_blk; i++) {
        dir_buffer[i].file_name[0] = '\0';
    }

    // set "."
    if (set_entry(dir_buffer, ".", 2*sizeof(dir_entry), sub_blk, TYPE_DIR, DOT_INDX) < 0) {
        return -1;
    }
    // set ".."
    if (set_entry(dir_buffer, "..", parent_size, parent_blk, TYPE_DIR, DOTDOT_INDX, parent_prms)  < 0) {
        return -1;
    }

    // update sub-dir to disk
    disk.write(sub_blk, (uint8_t*) &dir_buffer);

    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath) {
    // store path to check validity
    int path_blk = find_path(dirpath, TYPE_DIR);
    // error if path was not found or of wrong type (-1, -2 respectivly)
    if ( path_blk < 0) {
        std::cerr << "Error: Path not found, or could not be opened." << std::endl;
        return -1;
    }
    // if block exists, set current_path
    else {
        current_path = path_blk;
    }

    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int FS::pwd(){
    std::string working_dir = "";
    int temp_path_blk;
    int previouse_dir_blk;

    // if current standing dir is root, traversing is not necessary
    if (current_path == ROOT_BLOCK) {
        working_dir = "/";
    }

    struct dir_entry dir_buffer[entry_per_blk];
    disk.read(current_path, (uint8_t*) &dir_buffer);

    while (dir_buffer[0].first_blk != ROOT_BLOCK){
        // Saves the blk value of the dir.
        temp_path_blk = dir_buffer[0].first_blk;
        // Reads the parent dir into the buffer.
        disk.read(dir_buffer[1].first_blk, (uint8_t*) &dir_buffer);

        // Gets the entry indx for the dir that was prevoiuse in the buffer.
        previouse_dir_blk = get_entry_indx(dir_buffer, temp_path_blk);
        // Inserts the dirname at the beginning
        // of the string and adds a /.
        working_dir.insert(0, dir_buffer[previouse_dir_blk].file_name);
        working_dir.insert(0, 1, '/');
    }

    std::cout << working_dir << std::endl;

    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int FS::chmod(std::string accessrights, std::string filepath) {
    struct dir_entry dir_buffer[entry_per_blk]; // dir buffer
    // stores accessrights as int
    int prms;
    // try if accessrights are an integer
    try { prms = std::stoi(accessrights); }
    catch(const std::exception& e) {
        std::cerr << "Error: Access rights has to be integer." << std::endl;
        return -1;
    }

    struct dir_entry entry;
    if (cpy_entry(filepath, entry) < 0) {
        std::cerr << "Error: Path does not exist." << std::endl;
        return -1;
    }

    //* -- dir update*//
    // if entry is dir
    if (entry.type == TYPE_DIR) {
        // read entries dr block
        disk.read(entry.first_blk, (uint8_t*) &dir_buffer);
        // update access
        if (update_dir_access(dir_buffer, prms) < 0);
    }

    //* -- file update -- *//
    // type is file
    else {
        char name[56];
        // split name and path
        get_name(filepath, name, true);

        // get parent dir (reuse of entry.first_blk for write)
        entry.first_blk = find_path(filepath);
        if (entry.first_blk < 0) { return -1; }
        if (disk.read(entry.first_blk, (uint8_t*) &dir_buffer) < 0) { return -1; }

        // get index and update permissions
        dir_buffer[get_entry_indx(dir_buffer, name)].access_rights = prms;
    }

    // write updated access dir to disk
    disk.write(entry.first_blk, (uint8_t*) &dir_buffer);

    return 0;
}


//* -- Private functions -- *//

int FS::check_valid_filepath_length(std::string& filepath){
	if (filepath.length() > 56){
		std::cerr << "Error: Filepath too long." << std::endl;
		return -1;
	}
	if (filepath.length() < 1){
		std::cerr << "Error: Filepath cant be empty." << std::endl;
		return -1;
	}

	return 0;
}

int FS::get_free_blocks(int* free_blocks, int block_amount, int start_indx) {
    // get newest fat
    disk.read(FAT_BLOCK, (uint8_t*) &fat);

	int free_count = 0; // number of free blocks found
    // get free blocks
	for (int i = 2; i < BLOCK_SIZE/2 && free_count < block_amount; i++){
		if (fat[i] == FAT_FREE){
            // add to free_blocks offset by start_indx
			free_blocks[free_count+start_indx] = i;
			free_count++;
		}
	}

	// error handler too few free blocks on disk
	if (free_count < block_amount) {
		std::cerr << "Error: Too little disk memory" << std::endl;
		return -1;
	}

	return 0;
}

int FS::get_free_block() {
    int block[1];
    if (get_free_blocks(block, 1) < 0) { return -1; }

    return block[0];
}

int FS::link_and_update_fat(int* free_block, int block_amount){
    // get latest fat from disk
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

	return 0;
}

int FS::write_to_block(std::string strIn, int block_amount, int* free_blocks) {
    if (link_and_update_fat(free_blocks, block_amount) < 0) {
		return -1;
    }

	char write_out[BLOCK_SIZE];
	// write all full blocks to disk (last + EOF not included)
	for (int i = 0; i < block_amount - 1; i++){
        // fill write_out with chars from strIn (excluding EOL)
		strIn.copy(write_out, BLOCK_SIZE, i*BLOCK_SIZE);

        // write block to disk
		if (disk.write(free_blocks[i], (uint8_t*) &write_out) != 0){
			std::cerr << "Error: Could not write to blocks." << std::endl;
			return -1;
		}
	}

	// write last (non-full) block
	strIn.copy(write_out, strIn.length() % BLOCK_SIZE, (block_amount-1) * BLOCK_SIZE);
	// insert end-of-line
	write_out[strIn.length() % BLOCK_SIZE] = '\0';

	if (disk.write(free_blocks[block_amount-1], (uint8_t*) &write_out) != 0){
		std::cerr << "Error: Could not write to blocks." << std::endl;
		return -1;
	}

	return 0;
}

int FS::set_entry(struct dir_entry* dir, std::string name,
                   int size, int block, int type,
                   int entry_indx, int prms) {

    if (entry_indx == -1) {
        // returns the first free index in dir_buffer and check path existance
	    entry_indx = get_free_entry(dir, name);

        // if a free index could not be found or path exist
        if (entry_indx < 0) {
            std::cerr << "Error: Directory is full or could not be opened" << std::endl;
            return -1;
        }
    }

	strcpy(dir[entry_indx].file_name, name.c_str());
	dir[entry_indx].size          = size;
	dir[entry_indx].first_blk     = block;
	dir[entry_indx].type          = type;
	dir[entry_indx].access_rights = prms;

    return 0;
}

int FS::set_entry(struct dir_entry* dir, struct dir_entry entry, int entry_indx) {
    set_entry(dir, entry.file_name, entry.size, entry.first_blk, entry.type,
              entry_indx, entry.access_rights);

    return 0;
}

int FS::remove_entry(struct dir_entry* dir, std::string name) {
    for (int i = 0; i < entry_per_blk; i++) {
        // compare name to dir[i]
        if (dir[i].file_name == name) {
            dir[i].file_name[0] = '\0';
            return 0;
        }
    }

    std::cerr << "Error: Entry not found." << std::endl;
    return -1;
}

int FS::cpy_entry(std::string path, struct dir_entry &entry) {
    // check path exists
    if (find_path(path) < 0) { return -1; }

    char name[56];
    // get name of file, and remove name from source path
    get_name(path, name, true);

    int from_blk = find_path(path, TYPE_DIR); // get dir holding file entry
    struct dir_entry dir_buffer[entry_per_blk]; // dir buffer
    disk.read(from_blk, (uint8_t*) &dir_buffer);

    // get entry index to be copied from
    int entry_indx = get_entry_indx(dir_buffer, name);
    if(entry_indx < 0) {
        std::cerr << "Error: Entry does not exist." << std::endl;
        return -1;
    }
    entry = dir_buffer[entry_indx];

    return 0;
}

int FS::cpy_entry(struct dir_entry* dir, struct dir_entry &entry, char* name) {
    // get entry index to be copied from
    int entry_indx = get_entry_indx(dir, name);
    if(entry_indx < 0) {
        std::cerr << "Error: Entry does not exist." << std::endl;
        return -1;
    }
    // copy from dir[i] to entry
    entry = dir[entry_indx];

    return 0;
}

int FS::get_entry_indx(struct dir_entry* dir, std::string name) {
    // read dir until entry index (``i``) is found
	for (int i = 0; i < entry_per_blk; i++) {
        // if path exists
		if (dir[i].file_name == name) {
			return i;
		}
    }

    // if no entry is found
    return -1;
}

int FS::get_entry_indx(struct dir_entry* dir, int blk) {
    // read entries until entry index (``i``) is found
	for (int i = 0; i < entry_per_blk; i++) {
        // if path exists
		if (dir[i].first_blk == blk) {
			return i;
		}
    }

    // if no entry is found
    return -1;
}

int FS::get_free_entry(struct dir_entry* dir, std::string name) {
	int free_indx;
    if (dir[DOT_INDX].size > BLOCK_SIZE) {
        std::cerr << "Error: Directory full" << std::endl;
        return -1;
    }

    // check every entry in block
	for (int i = 0; i < entry_per_blk; i++) {
		// empty name, empty entry
		if (dir[i].file_name[0] == '\0') {
			free_indx = i;
		}

		// if path exists
		if (dir[i].file_name == name) {
			std::cerr << "Path already exist." << std::endl;
			return -1;
		}
	}

	return free_indx;
}

int FS::find_path (std::string path, int type_out, bool silence) {
    int currentBlk;
    int type; // store current block type
    struct dir_entry read_buffer[entry_per_blk];

    // if only root, return root block
    if (path == "/" && type_out == TYPE_DIR) {
        disk.read(ROOT_BLOCK, (uint8_t*) &read_buffer);
        // if root is only block to return, allow access according to read permission
        return read_buffer[0].first_blk;
    }

    // start at root, remove root path
    if (path[0] == '/') {
        currentBlk = ROOT_BLOCK;
        path.erase(0,1);
        // root is always dir
        type = TYPE_DIR;
    }
    // start in current dir
    else {
        currentBlk = current_path;
    }

    disk.read(currentBlk, (uint8_t*) &read_buffer);

    // stream to easily split path into names
    std::istringstream ssPath(path);
    std::string name;
    // stores amount of access denies during traverse
    int denies = 0;
    // traverse down filepath one dir at a time
    while (std::getline(ssPath, name, '/')) {
        // stores found entry's position in block (keeps -1 if path was not found)
        int nxt_entry = -1;
        // iterate until entry is found, or end of block
        for (int i = 0; i < entry_per_blk && nxt_entry < 0; i++) {
            if (read_buffer[i].file_name == name) {
                nxt_entry = i;
            }
        }

        // path does not exist
        if (nxt_entry < 0) { return -1; }

        // get type for later error handling
        type = read_buffer[nxt_entry].type;
        currentBlk = read_buffer[nxt_entry].first_blk;

        disk.read(currentBlk, (uint8_t*) &read_buffer);
    }

    // return conditions
    // no exclusion
    if (type_out == -1) {
        return currentBlk;
    }
    // if path retrieves wrong type
    else if (type_out != type) {
        if (silence == false) {
            std::cerr << "Error: Path is of incorrect type" << std::endl;
        }
        return -2;
    }
    // return specified type
    else {
        return currentBlk;
    }
}

int FS::write_file(std::string path, std::string write_out) {
    struct dir_entry dir_buffer[entry_per_blk]; // dir buffer

    // load fat
    disk.read(FAT_BLOCK, (uint8_t*) &fat);
    if (find_path(path, -1, true) != -1) {
        std::cerr << "Error: Path already exist." << std::endl;
        return -1;
    }

    // split name and path
    char name[56]; // name buffer
    get_name(path, name, true);

    int dir_blk = find_path(path, -1);
    if (dir_blk < 0) { return -1; };
    // load dir for file entry
    disk.read(dir_blk, (uint8_t*) &dir_buffer);

    if (dir_buffer[DOT_INDX].type != TYPE_DIR) {
        std::cerr << "Error: Path is not a directory." << std::endl;
        return -1;
    }

    if ((dir_buffer[0].access_rights & WRITE) == false) {
        std::cerr << "Error: Write access denied." << std::endl;
        return -1;
    }

    // amount of blocks file has to occupy
    int block_amount = ceil((write_out.length()+1) / (float) BLOCK_SIZE);

    // store indecies for empty blocks
    int free_blks[block_amount]; // index of free blocks
	if (get_free_blocks(free_blks, block_amount) < 0) {
		return -1;
    }

    // set entry at first available entry
    if (set_entry(dir_buffer, name, write_out.length()+1, free_blks[0], TYPE_FILE) < 0) {
        return -1;
    }

    // handles disk write of file contents
    if (write_to_block(write_out, block_amount, free_blks) < 0) {
        return -1;
    }

    // updates dir sizes
    if (update_dir_size(dir_buffer, sizeof(dir_entry)) < 0 ) { return -1; }

    // updates dir to disk by one entry
    if (disk.write(dir_blk, (uint8_t*) &dir_buffer) != 0) { return -1; }

    return 0;
}

int FS::read_file(std::string path, std::string& read_to) {
    // load fat
    disk.read(FAT_BLOCK, (uint8_t*) &fat);

    // copy entry of path for access rights and type compare
    struct dir_entry entry;
    // copy entry, and return -1 if it does not exist
    if (cpy_entry(path, entry) < 0) {
        return -1;
        std::cout  << "Error: File does not exist."
            << std::endl;
    }

    // error handling if trying to read a dir
    if (entry.type == TYPE_DIR) {
        std::cout  << "Error: Cannot read a directory."
            << std::endl;
        return -1;
    }

    // error handling for read permissions
    if ((entry.access_rights & READ) == false) {
        std::cout  << "Error: Read access denied."
            << std::endl;
        return -1;
    }

    int file_blk = entry.first_blk;

    // buffer for block content
    // (BLOCK_SIZE+1 to append EOL for use with string)
	char readBlk[BLOCK_SIZE+1];
	// read all "full" blocks
	while (fat[file_blk] != FAT_EOF) {
		disk.read(file_blk, (uint8_t*) &readBlk);
        readBlk[BLOCK_SIZE] = '\0';
		read_to.append(readBlk);
		file_blk = fat[file_blk];
	}

	//read "non-full" block
	disk.read(file_blk, (uint8_t*) &readBlk);
	read_to.append(readBlk);

	return 0;
}

int FS::update_dir_size(struct dir_entry* dir, int size,
                        bool update_parent, bool update_subs) {

    //! ``dir`` writes to disk OUTSIDE function! This function does NOT own it!

    // update current dir
    dir[DOT_INDX].size += size;

    if (update_parent == true) {
        // if ".." is same as "."
        if (dir[DOT_INDX].first_blk == ROOT_BLOCK) {
            dir[DOTDOT_INDX].size = dir[DOT_INDX].size;
        }
        // if dir is a sub-dir
        else {
            // get current dir block to compare in ".."/parent
            int sub_blk = dir[DOT_INDX].first_blk;

            // buffer to store ".." directory
            struct dir_entry buffer[entry_per_blk];
            if (disk.read(dir[DOTDOT_INDX].first_blk, (uint8_t*) &buffer)) {
                return -1;
            }

            // update size in ".." where it stores sub-dir
            buffer[get_entry_indx(buffer, sub_blk)].size += size;

            // update ".." to disk
            if (disk.write(buffer[DOT_INDX].first_blk, (uint8_t*) &buffer) < 0) {
                return -1;
            }
        }
    }

    // updates ".." of sub-dirs
    if (update_subs == true) {
        // buffer to store sub dirs
        struct dir_entry buffer[entry_per_blk];
        // get subs (0-1 are dots-, not sub dirs)
        for (int i = 2; i < entry_per_blk; i++) {
            // a child MUST have a name (type is not overwritten at ``rm``, file_name is)
            if (((std::string) dir[i].file_name != "\0") && (dir[i].type == TYPE_DIR)) {
                // load sub dir
                disk.read(dir[i].first_blk, (uint8_t*) &buffer);
                //update parent size
                buffer[DOTDOT_INDX].size += size;
                // update to disk
                disk.write(dir[i].first_blk, (uint8_t*) &buffer);
            }
        }
    }

    return 0;
}

int FS::update_dir_access(struct dir_entry* dir, int prms) {
    //! ``dir`` writes to disk OUTSIDE function! This function does NOT own it!

    // update current dir
    dir[DOT_INDX].access_rights = prms;

    // if ".." is same as "."
    if (dir[DOT_INDX].first_blk == ROOT_BLOCK) {
        dir[DOTDOT_INDX].access_rights = prms;
    }
    // if dir is a sub-dir
    else {
        // get current dir block to compare in ".."/parent
        int sub_blk = dir[DOT_INDX].first_blk;

        // buffer to store ".." directory
        struct dir_entry buffer[entry_per_blk];
        if (disk.read(dir[DOTDOT_INDX].first_blk, (uint8_t*) &buffer)) {
            return -1;
        }

        // update permissions in ".." where it stores sub-dir
        buffer[get_entry_indx(buffer, sub_blk)].access_rights = prms;

        // update ".." to disk
        if (disk.write(buffer[DOT_INDX].first_blk, (uint8_t*) &buffer) < 0) {
            return -1;
        }
    }

    // buffer to store sub dirs
    struct dir_entry buffer[entry_per_blk];
    // get subs (0-1 are dots-, not sub dirs)
    for (int i = 2; i < entry_per_blk; i++) {
        // a child MUST have a name (type is not overwritten at ``rm``, file_name is)
        if (((std::string) dir[i].file_name != "\0") && (dir[i].type == TYPE_DIR)) {
            // load sub dir
            disk.read(dir[i].first_blk, (uint8_t*) &buffer);
            // update parent permissions
            buffer[DOTDOT_INDX].access_rights = prms;
            // update to disk
            disk.write(dir[i].first_blk, (uint8_t*) &buffer);
        }
    }

    return 0;
}

int FS::get_name(std::string& path, char* name, bool rm_name) {
    int name_size;
    // get name length
    for(name_size = path.length(); path[name_size] != '/' && name_size > 0; name_size--);
    if (name_size < 1 && path[0] == '/') { name_size = path.length(); }
    else if (name_size < 1) { name_size = path.length() + 1;          }
    else                    { name_size = path.length() - name_size;  }

    // file name error handler
    if (check_valid_filepath_length(path) != 0) { return -1; }

    // copy name form end of str path to char* name
    strcpy(name, path.substr(path.length()-name_size+1, name_size).c_str());

    // remove name from path
    if (rm_name == true) {
        // delete name from path
        if (path.length() > name_size) {
            path.erase(path.length()-name_size, name_size);
        }
        // path starts at root with not sub-dirs
        else if (path.length() == name_size) {
            path = "/";
        }
        // path is current dir
        else { path = "."; }
    }

    return 0;
}
