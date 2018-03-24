#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>


#define TRUE 0
#define FALSE 1
#define ROOTINO 1
#define BSIZE 512
#define NDIRECT 12
#define IPB (BSIZE / sizeof(struct dinode))
#define IBLOCK(i)     ((i) / IPB + 2)
#define BPB           (BSIZE*8)
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)
#define DIRSIZ 14

// File system super block
struct superblock {
	uint size;			//fs size (blocks)
	uint nblocks;  		//number of blocks
	uint ninodes;  		//number of inodes
};

// On-disk inode structure
struct dinode {
	short type; 			//file type
	short major; 			//major device number
	short minor; 			//minor device number
	short nlink; 			//number of links to inode
	uint size; 				//size of file(bytes)
	uint addrs[NDIRECT + 1];	//data block addresses
};

// Directory entry
struct dirent {
	ushort inum;		//inode number
	char name[DIRSIZ];	//directory name
};

// unused | superblock | inode table | bitmap (data) | data blocks
// some gaps in here

int main(int argc, char *argv[]) {
	int repair = 0;
	char * filename;
	if (strcmp(argv[1], "-r") == 0) {
		repair = 1;
		filename = argv[2];
	} else filename = argv[1];

	int fd;
	if (repair) {
		fd = open(filename, O_RDWR);
	} else {
		fd = open(filename, O_RDONLY);
	}
	// Check 0: bad file input
	if (fd == -1) {
		fprintf(stderr, "image not found.\n");
		return 1;
	}

	int rc;
	struct stat sbuf;

	rc = fstat(fd, &sbuf);
	assert (rc == 0);
	void *img_ptr;
	if (repair) {
		img_ptr = mmap(NULL, sbuf.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	} else {
		img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	}
	assert (img_ptr != MAP_FAILED);

	struct superblock *sb;
	sb = (struct superblock *)(img_ptr + BSIZE);

	//inodes
	int i, j;
	int blockAddr, type;
	// First inode block in list of inodes
	struct dinode *dip = (struct dinode *)(img_ptr + 2 * BSIZE);
	// Pointer to bitmap location
	void *bitmap = (void *)(img_ptr + (sb->ninodes / IPB + 3) * BSIZE);
	// List of used blocks
	int usedBlocks[sb->size];
	// Number of links to inode in file system
	int numLinks[sb->ninodes];
	// List of used inodes
	int inodeUsed[sb->ninodes];
	// Directory list for used inodes
	int usedinodeDirectory[sb->ninodes];
	// Parent inode numbers
	int parentInode[sb->ninodes];

	// Set all used blocks to unused
	for (j = 0; j < sb->size; j++ ) {
		usedBlocks[j] = 0;
	}
	// Set used blocks to used in list
	for (j = 0; j < (sb->ninodes / IPB + 3 + (sb->size / (BSIZE * 8) + 1)); j++) {
		usedBlocks[j] = 1;
	}

	for (i = 0; i < sb->ninodes; i++) {
		struct dirent *currDirectory;
		type = dip->type;
		int k;

		// Check 3: valid root inode
		if (i == 1 && type != 1) {
			fprintf(stderr, "ERROR: root directory does not exist.\n");
			return 1;
		}
		// Check for valid file type
		if (type >= 0 && type <= 3) {
			// Make sure it is valid
			if (type != 0) {
				// If directory, check to make sure it is a valid directory
				if (type == 1) {
					void *blkAddr = img_ptr + dip->addrs[0] * BSIZE;
					currDirectory = (struct dirent*)(blkAddr);
					// Check 4: valid directory format
					if (strcmp(currDirectory->name, ".") != 0) {
						fprintf(stderr, "ERROR: directory not properly formatted.\n");
						return 1;
					}
					currDirectory++;
					if (strcmp(currDirectory->name, "..") != 0) {
						fprintf(stderr, "ERROR: directory not properly formatted.\n");
						return 1;
					}
					parentInode[i] = currDirectory->inum;
					// Extra Credits: valid parent directory
					if (i != 1 && currDirectory->inum == i) {
						fprintf(stderr, "ERROR: parent directory mismatch.\n");
						return 1;
					}
					struct dinode *parentDir = (struct dinode *)(img_ptr + 2 * BSIZE + currDirectory->inum * sizeof(struct dinode));
					if (parentDir->type != 1) {
						fprintf(stderr, "ERROR: parent directory mismatch.\n");
						return 1;
					}
					int validParentDir = FALSE;
					int x, y, z;
					for (x = 0; x < NDIRECT; x++) {
						struct dirent *currDirectory;
						if (parentDir->addrs[x] != 0) {
							currDirectory = (struct dirent *)(img_ptr + parentDir->addrs[x] * BSIZE);
							// Check for valid parent directory
							for (y = 0; y < BSIZE / sizeof(struct dirent); y++) {
								if (currDirectory->inum != i) {
									validParentDir = TRUE;
								}
								currDirectory++;
							}
						}
						if (dip->addrs[x] != 0) {
							currDirectory = (struct dirent *)(img_ptr + dip->addrs[x] * BSIZE);
							// Find used inodes and mark them in list
							for (z = 0; z < BSIZE / sizeof(struct dirent); z++) {
								if (currDirectory->inum == 0) {
                                    currDirectory++;
                                    continue;
								}
								// For each used inode found also increment reference count
								if (strcmp(currDirectory->name, ".") != 0 && strcmp(currDirectory->name, "..") != 0) {
									usedinodeDirectory[currDirectory->inum] = 1;
									numLinks[currDirectory->inum]++;
								}
								currDirectory++;
							}
						}
					}
					if (dip->addrs[NDIRECT] != 0) {
						uint* inptr = (uint*)(img_ptr + dip->addrs[NDIRECT] * BSIZE);
						for (x = 0; x < BSIZE / sizeof(uint); ++x) {
							if (inptr[x] == 0) break;
							currDirectory = (struct dirent *)(img_ptr + inptr[x] * BSIZE);
							// Find used inodes and mark them in list
							for (z = 0; z < BSIZE / sizeof(struct dirent); z++) {
								if (currDirectory->inum == 0) {
                                    currDirectory++;
                                    continue;
								}
								// For each used inode found also increment reference count
								if (strcmp(currDirectory->name, ".") != 0 && strcmp(currDirectory->name, "..") != 0) {
									usedinodeDirectory[currDirectory->inum] = 1;
									numLinks[currDirectory->inum]++;
								}
								currDirectory++;
							}

						}
					}
					// Check 5: valid parent directory
					if (validParentDir == FALSE) {
						fprintf(stderr, "ERROR: parent directory mismatch.\n");
						return 1;
					}
				}

				for (k = 0; k < NDIRECT + 1; k++) {
					int x;
					blockAddr = dip->addrs[k];
					// Make sure address is valid
					if (blockAddr != 0) {
						if (blockAddr != 0) {
							// Check 2: bad address in inode
							if ((blockAddr) < ((int)BBLOCK(sb->nblocks, sb->ninodes)) + 1 || blockAddr > (sb->size * BSIZE)) {
								if (k == NDIRECT)
									fprintf(stderr, "ERROR: bad indirect address in inode.\n");
								else
									fprintf(stderr, "ERROR: bad direct address in inode.\n");
								return 1;
							}
							// Check 7: check used blocks are only used once
							// Problem?
							if (usedBlocks[blockAddr] == 1) {
								if (k == NDIRECT)
									fprintf(stderr, "ERROR: indirect address used more than once.\n");
								else
									fprintf(stderr, "ERROR: direct address used more than once.\n");
								return 1;
							}
						}
						usedBlocks[blockAddr] = 1;

						// Check 5: check used blocks in bitmap
						int bitmapLocation = (*((char*)bitmap + (blockAddr >> 3)) >> (blockAddr & 7)) & 1;
						if (bitmapLocation == 0) {
							fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
							return 1;
						}
					}

					if (type == 1 && blockAddr > 1 && k == 0) {
						int checkDirFormat = 0;
						int isRoot = 0;
						int checkRootDir = 0;
						if (i == 1) {
							isRoot++;
						}
						for (x = 0; x < NDIRECT + 1; x++) {
							currDirectory = (struct dirent *)(img_ptr + (blockAddr * BSIZE) + x * (sizeof(struct dirent)));
							if (currDirectory->inum != 0) {
								if (strcmp(currDirectory->name, ".") == 0 || strcmp(currDirectory->name, "..") == 0) {
									checkDirFormat++;
									if (isRoot == 1 && currDirectory->inum == 1)
										checkRootDir++;
								}
							}
						}
						// Check 4: valid directory format
						if (checkDirFormat != 2) {
							fprintf(stderr, "ERROR: directory not properly formatted.\n");
							return 1;
						}
						// Check 3: valid root inode
						if (isRoot == 1) {
							if (checkRootDir != 2) {
								fprintf(stderr, "ERROR: root directory does not exist.\n");
								return 1;
							}
						}
					}
				}

				if (dip->size > BSIZE * NDIRECT) {
					int *indirect = (int *)(img_ptr + (blockAddr * BSIZE));
					for (k = 0; k < BSIZE / 4; k++) {
						int block = *(indirect + k);
						// Check if address is valid
						if (block != 0) {
							// Check 2: bad address in inode
							if (block < ((int)BBLOCK(sb->nblocks, sb->ninodes)) + 1) {
								if (k < NDIRECT)
									fprintf(stderr, "ERROR: bad direct address in inode.\n");
								else
									fprintf(stderr, "ERROR: bad indirect address in inode.\n");
								return 1;
							}
							// Check 8: check used blocks are only used once
							// Problem?
							if (usedBlocks[block] == 1) {
								fprintf(stderr, "ERROR: address used more than once.\n");
								return 1;
							}
							usedBlocks[block] = 1;
							// Check 6: check used blocks in bitmap
							int bitmapLocation = (*((char*)bitmap + (block >> 3)) >> (block & 7)) & 1;
							if (bitmapLocation == 0) {
								fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
								return 1;
							}
						}
					}
				}

			}
			dip++;
		} else {
			// Check 1: valid type
			fprintf(stderr, "ERROR: bad inode.\n");
			return 1;
		}
	}

	numLinks[1]++;

	// Check 6: check used blocks are marked bitmap and are used
	int block;
	for (block = 0; block < sb->size; block++) {
		int bitmapLocation = (*((char*)bitmap + (block >> 3)) >> (block & 7)) & 1;
		if (bitmapLocation == 1) {
			if (usedBlocks[block] == 0) {
				fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
				return 1;
			}
		}
	}

	int lost_and_found = 0;
	if (repair) {
		dip = (struct dinode *)(img_ptr + 2 * BSIZE);
		dip++;
		int x, z;
		for (x = 0; x < NDIRECT; x++) {
			struct dirent *currDirectory;
			if (dip->addrs[x] != 0) {
				currDirectory = (struct dirent *)(img_ptr + dip->addrs[x] * BSIZE);
				for (z = 0; z < BSIZE / sizeof(struct dirent); z++) {
					if (strcmp(currDirectory->name, "lost_and_found") == 0) {
						lost_and_found = currDirectory->inum;
						break;
					}
					currDirectory++;
				}
			}
		}
	}

	dip = (struct dinode *)(img_ptr + 2 * BSIZE);
	for (i = 0; i < sb->ninodes; i++) {
		type = dip->type;
		if (type != 0) {
			inodeUsed[i] = 1;
		}

		// Check 9: check used inodes reference a directory
		if (i > 1 && inodeUsed[i] == 1 && usedinodeDirectory[i] == 0) {
			fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
			if (repair == 0) return 1;
			fprintf(stderr, "fix %d\n", i);
			int x, z;
			int fixed = 0;
			struct dinode * lost = (struct dinode*)(img_ptr + 2 * BSIZE);
			lost += lost_and_found;
			for (x = 0; x < NDIRECT && fixed == 0; x++) {
				struct dirent *currDirectory;
				if (lost->addrs[x] != 0) {
					currDirectory = (struct dirent *)(img_ptr + dip->addrs[x] * BSIZE);
					for (z = 0; z < BSIZE / sizeof(struct dirent); z++) {
						if (currDirectory->inum == 0) {
							currDirectory->inum = i;
							char name[128];
							snprintf(name, 128, "lost_and_found_%d", i);
							strcpy(currDirectory->name, name);
							fixed = 1;
							break;
						}
						currDirectory++;
					}
				}
			}
			numLinks[i] = 1;
		}

		// Check 10: check used inodes in directory are in inode table
		if (usedinodeDirectory[i] == 1 && inodeUsed[i] == 0) {
			fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
			return 1;
		}

		// Check 12: No extra links allowed for directories
		if (type == 1 && numLinks[i] != 1) {
			fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
			return 1;
		}

		// Check 11: hard links to file match files reference count
		if (numLinks[i] != dip->nlink && i != 1) {
			fprintf(stderr, "ERROR: bad reference count for file.\n");
			return 1;
		}

		if (type == 1 && i != 1) {
			int len = 0;
			int visited[sb->ninodes];
			int cur = i;
			while (cur != 1) {
				int j;
				for (j = 0; j < len; ++j)
					if (visited[j] == cur) break;
				if (j < len) {
					fprintf(stderr, "ERROR: inaccessible directory exists.\n");
					return 1;
				}
				visited[len++] = cur;
				cur = parentInode[cur];
			}
		}


		dip++;
	}
	if (msync(img_ptr, sbuf.st_size, MS_SYNC) < 0) {
		fprintf(stderr, "msync failed\n");
	}
	if (munmap(img_ptr, sbuf.st_size) < 0) {
		fprintf(stderr, "munmap failed\n");
	}
	close(fd);
	return 0;
}

