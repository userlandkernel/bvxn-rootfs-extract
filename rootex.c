#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <lzfse.h>

#define BVXN_MAGIC "bvxn"

void usage(void) {
	printf("Usage: rootex [/path/to/bvxn_rootfs.dmg] [/path/to/raw_output_rootfs.bin]\n");
}


bool file_exists(const char* path) {
	return access(path, F_OK) != -1;
}

/**
 * @function open_input_file
 * @param path path to the file to open
 * @brief Opens a file for reading
 * @return file descriptor
*/
int open_input_file(const char *path) {
	int fd = open(path, O_RDONLY);
	if(fd < 0) {
		fprintf(stderr, "Failed to open '%s' for reading.\n", path);
	}
	return fd;
}


int open_output_file(const char *path) {

	if(file_exists(path)) {
		remove(path);
	}

	int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0x0777);
	if(fd < 0) {
		fprintf(stderr, "Failed to open '%s' for writing.\n", path);
        }
        return fd;
}


size_t get_filesize(int file) {
	struct stat st = {};

	// Validate file descriptor
	if(file < 0) {
		return 0;
	}

	// Retrieve file stats
	if(fstat(file, &st) < 0) {
		fprintf(stderr, "Failed to get statistics for fd '%d'.\n", file);
		return 0;
	}
	return (size_t)st.st_size;
}

char *map_input_file(int file) {
	size_t size = get_filesize(file);
	char *page = NULL;
	if(!size) {
		fprintf(stderr, "File is too small to map.\n");
		return NULL;
	}
	if((page = mmap(0, size, PROT_READ, MAP_SHARED, file, 0)) == (uintptr_t)-1) {
		fprintf(stderr, "Failed to map file.\n");
		return NULL;
	}
	return page;
}

char *map_output_file(int file, size_t size) {
	char *page = NULL;
	if(!size) {
		fprintf(stderr, "Size is too small to map.\n");
		return NULL;
	}

	// Go to last byte
	if(lseek(file, size - 1, SEEK_SET) == -1) {
		fprintf(stderr, "Failed to seek.\n");
		return NULL;
	}

	// Write a null-byte
	if(write(file, "", 1) != 1) {
		fprintf(stderr, "Failed to write null-byte\n");
		return NULL;
	}

	// Now map the file
	if((page = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0)) == (uintptr_t)-1) {
		fprintf(stderr, "Failed to map file.\n");
		return NULL;
	}

	return page;
}

int main(int argc, char *argv[]){

	const char *inputFilePath = NULL;
	const char *outputFilePath = NULL;
	int inputFile = -1;
	int outputFile = -1;
	char *inputMap = NULL;
	char *outputMap = NULL;


	if(argc <= 2) {
		usage();
		return 0;
	}

	inputFilePath = argv[1];
	outputFilePath = argv[2];

	// Check wether the input file exists
	if(!file_exists(inputFilePath)) {
		fprintf(stderr, "The input file '%s' does not seem to exist.\n", inputFilePath);
		return 1;
	}

	// Open the input file
	inputFile = open_input_file(inputFilePath);

	// Map the input file into the memory
	inputMap = map_input_file(inputFile);

	// Get the size of the input file
	size_t size = get_filesize(inputFile);

	// Open the output file
	outputFile = open_output_file(outputFilePath);

	// Map the output file into the memory
	outputMap = map_output_file(outputFile, size);

	// Decode the file
  
	int count = 0;
  
	char* outputDst = outputMap;
	char* inputDst = inputMap;
  
	while(inputDst != inputMap+size) {
  
    int bytesRemain = (inputMap + size) - inputDst;
    
    // Never read OOB
		if(bytesRemain > 4) {
    
			if(*(uint32_t*)BVXN_MAGIC == *(uint32_t*)inputDst) { // Check if bvxn magic is found
      
				printf("OFF: %#lx\n", (inputDst-inputMap)); // Print the offset of the magic
        
				count = lzfse_decode_buffer(outputDst, size, inputDst, size, NULL); // Decompress the data
        printf("bytes decoded: %d\n", count);
        outputDst += count;
			}
      
		}
		inputDst++;
	}

	// Close the input file
	close(inputFile);

	// Close the output file
	close(outputFile);

	return 0;
}
