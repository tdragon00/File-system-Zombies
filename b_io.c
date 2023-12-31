/**************************************************************
* Class:  CSC-415-02 Summer 2023
* Names:  Saripalli Hruthika, Nixxy Dewalt, Alekya Bairaboina, Banting Lin 
* Student IDs: 923066687, 922018328, 923041428, 922404012
* GitHub Name: Alekhya1311
* Group Name: Zombies
* Project: Basic File System
*
* File: b_io.c
*
* Description: Basic File System - Key File I/O Operations
*
**************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>			// for malloc
#include <string.h>			// for memcpy
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "b_io.h"
#include "fsFunc.h"
#include "mfs.h"
#include "fsLow.h"

#define MAXFCBS 20
#define B_CHUNK_SIZE 512

typedef struct b_fcb
	{
	/** TODO add al the information you need in the file control block **/
	char * buf;		//holds the open file buffer
	int index;		//holds the current position in the buffer
	int buflen;		//holds how many valid bytes are in the buffer
    DE * fileDE;    //Pointer to the Directory Entry
    int fileSize;
    int currentBufferOffset; //to keep track of the position within the buffer during read/write operations.
    int fileOffset; //current position within the file being read or written
    int currentBlock; //to keep track of the current block number within the file
	int fileAccessFlags; //flags on the file they
	// use bitwise operator & to check
	} b_fcb;
	
b_fcb fcbArray[MAXFCBS];

int startup = 0;	//Indicates that this has not been initialized

//Method to initialize our file system
void b_init ()
	{
	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].buf = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].buff == NULL)
			{
			return i;		//Not thread safe (But do not worry about it for this assignment)
			}
		}
	return (-1);  //all in use
	}
	
// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags for open
// O_RDONLY, O_WRONLY, or O_RDWR or O_CREAT
b_io_fd b_open (char * filename, int flags)
	{

	if (startup == 0) b_init();  //Initialize our system
	b_io_fd returnFd; //return value



	//call parse path to get test if we have valid file
	parsePath(filename);
	//see if it exists
	if(parseInfo.exist==0)
	{
		//see if it is a directory
		if(parseInfo.fileType == "d")
		{
			printf("This is a directory not a file");
			return -1;
		}

	}

	//  at this stage file is not a Directory entry or it does not exist


	//create file
	//need create a file function
	if(flags & O_CREAT)
	{
		//repopulating the parsepath struct since we will need it fof the fcb.
		parsePath(fileName);
	}

	returnFd = b_getFCB();				// get our own file descriptor
										// check for error - all used FCB's
	if(returnFd == -1)
	{
		printf("All FCBs used please close other files. \n");
		
	}

	if(returnFd != -1)
	{
		
	fcbArray[returnFd].buf = malloc(BLOCK_SIZE);		//holds the open file buffer
	fcbArray[returnFd].index = 0;		//holds the current position in the buffer
	fcbArray[returnFd].buflen = 0;		//holds how many valid bytes are in the buffer
    fcbArray[returnFd].fileDE = parsePathInfo.entry;			//Pointer to the Directory Entry needs to be in parsePathInfo
    fcbArray[returnFd].fileSize = parsePathInfo.fileSize;  		//needs Directory Entry
    fcbArray[returnFd].currentBufferOffset = 0; //to keep track of the position within the buffer during read/write operations.
    fcbArray[returnFd].fileOffset = 0; 	//current position within the file being read or written
    fcbArray[returnFd].currentBlock = 0; //to keep track of the current block number within the file
	fcbArray[returnFd].fileAccessFlags = flags; // need to watch out for create
	}

	return (returnFd);						// all set
	}


// Interface to seek function	
int b_seek (b_io_fd fd, off_t offset, int whence)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
		
	return (0); //Change this
	}



// Interface to write function	
int b_write (b_io_fd fd, char * buffer, int count)
	{
    int fileAccessFlags = fcbArray[fd].flags;
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
    // Check if the file has the appropriate write access
	if(!((fileAccessFlags & O_RDWR) == 0) || ((fileAccessFlags & O_WRONLY) == 0)) 
		{
			return (-1); // File does not have access to write
		}
	
	int numRemainingBytes = count;
    int numBlocksWritten = 0;
	int totalBytes = 0;
    // If the file size is greater than or equal to the number of blocks spanned by the 
    // file multiplied by the block size (BLOCK_SIZE). This means file needs more space 
    // to accommodate the new data being written.
	if(fcbArray[fd].fileDE->fileSize >= fcbArray[fd].fileDE->dirBlocks * BLOCK_SIZE) {
		//allocates new space for the file, copies the old data to the new location, 
        // and updates the file descriptor accordingly.
		char *newBuffer = malloc(fcbArray[fd].fileDE->dirBlocks * BLOCK_SIZE);

		LBAread(newBuffer, fcbArray[fd].fileDE->dirBlocks, fcbArray[fd].fileDE->location);
        // Allocate new blocks for the file
        int newLocation = allocateFreeSpace(fcbArray[fd].fileDE->dirBlocks * 2);
        // Write the old data to the new blocks
		LBAwrite(newBuffer, fcbArray[fd].fileDE->dirBlocks, newLocation);

		// free the old blocks that were used
		freeTheBlocks(fcbArray[fd].fileDE->location,fcbArray[fd].fileDE->dirBlocks);

		// Update the file location and the number of blocks for the file
		fcbArray[fd].fileDE->location = newLocation;
		fcbArray[fd].fileDE->dirBlocks = fcbArray[fd].fileDE->dirBlocks * 2;

		reloadCurrentDir(dir[0]);

		free(newBuffer);
		newBuffer = NULL;
		

    }
    // if the number of remaining bytes to be written is greater than or equal to the chunk size 
    // to determine if there are enough bytes to write a whole chunk of data.

    if(numRemainingBytes >= B_CHUNK_SIZE) {

		int numBlocksToWrite = numRemainingBytes/B_CHUNK_SIZE;		
		// temporary buffer to hold the data for a whole number of chunks
		char *tempBuffer = malloc(numBlocksToWrite * B_CHUNK_SIZE);
        // Copy data from the original buffer to the temporary buffer
		memcpy(tempBuffer, buffer, numBlocksToWrite * B_CHUNK_SIZE);
        // Write the data to disk
        numBlocksWritten += LBAwrite(tempBuffer, numBlocksToWrite, 
							fcbArray[fd].currentBufferOffset + fcbArray[fd].fileDE->location);
		
        numRemainingBytes -= numBlocksToWrite * B_CHUNK_SIZE;

        int newFileSize = numBlocksToWrite * B_CHUNK_SIZE;
		int sizeOffset = fcbArray[fd].fileSize - fcbArray[fd].fileOffset;	
        // Update file offset and buffer offset
		fcbArray[fd].fileOffset += numBlocksToWrite * B_CHUNK_SIZE;
		fcbArray[fd].currentBufferOffset = numBlocksToWrite * B_CHUNK_SIZE;
        // Update current block
        fcbArray[fd].currentBlock += numBlocksToWrite;

		free(tempBuffer);
		tempBuffer = NULL;
    }

	// as long as there are bytes remaining to be written and the remaining bytes are less than the chunk size
	
    while(numRemainingBytes < B_CHUNK_SIZE && numRemainingBytes > 0) {

		// location within the buffer where the data will be written.
		int bufferLocation;

		// temporary buffer for our buffer
		char *tempBuffer = malloc(B_CHUNK_SIZE);
        // Get the offset of the block to write
		int blockOffset = blocksNeeded(fcbArray[fd].fileOffset) - 1;
		// check if the file offset is at the beginning of a new block and the offset is greater than or equal 
        // to B_CHUNK_SIZE. If so, increment blockOffset by 1 to move to the next block.

		if(fcbArray[fd].fileOffset % B_CHUNK_SIZE == 0 && fcbArray[fd].fileOffset >= B_CHUNK_SIZE)
			{
				blockOffset += 1;
			}
		// Copy the initial content to temporary buffer
		LBAread(tempBuffer, 1, blockOffset + fcbArray[fd].fileDE->location);
        // Determine the location within the buffer to write the data
		if(fcbArray[fd].fileOffset < B_CHUNK_SIZE)
			{
				bufferLocation = fcbArray[fd].fileOffset;
			}
		else if(fcbArray[fd].fileOffset >= B_CHUNK_SIZE)
			{
				bufferLocation = fcbArray[fd].fileOffset % B_CHUNK_SIZE;
			}

		int bytesToWrite;
        // Determine the number of bytes to write based on the remaining bytes and the 
        // space left in the current block
		if(numRemainingBytes > B_CHUNK_SIZE - bufferLocation)
			{
				bytesToWrite = B_CHUNK_SIZE - bufferLocation;
			}
		else
			{
				bytesToWrite = numRemainingBytes;
			}
        // Copy the data from the user buffer to the temporary buffer
		memcpy(tempBuffer + bufferLocation, buffer, bytesToWrite);
		fcbArray[fd].currentBufferOffset = bytesToWrite;
        // Move the user buffer to the next location to write
		buffer += fcbArray[fd].currentBufferOffset;
        int numBlocksWritten = LBAwrite(tempBuffer, 1, 
						blockOffset + fcbArray[fd].fileDE->location);
		fcbArray[fd].fileOffset += bytesToWrite;
        // Update the total number of bytes written
		totalBytes += bytesToWrite;
        numRemainingBytes -= bytesToWrite;
        // Free the temporary buffer used to store the data for this block
		free(tempBuffer);

		
    }
    // If the file buffer is NULL, allocate memory for it
	if(fcbArray[fd].buf == NULL) {
        fcbArray[fd].buf = malloc(B_CHUNK_SIZE);
    }
	// Reset the current buffer offset to 0 for the next write operation
	fcbArray[fd].currentBufferOffset = 0; 
	fcbArray[fd].fileDE->fileSize += count;
	

	return (totalBytes);
	}



// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill 
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
int b_read (b_io_fd fd, char * buffer, int count)
	{

	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
	return (0);	//Change this
	}
	
// Interface to Close the file	
int b_close (b_io_fd fd)
	{

	}
