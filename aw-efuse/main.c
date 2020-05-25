#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

enum efuseConst{
	efuseBase = 0x01c14000,

	regCTL = 0x10,
	regWD  = 0x14,
	regRD  = 0x18,

	cmdKey   = 0xac00,
	cmdRead  = 0x2,
	cmdWrite = 0x1,
};

volatile void* mmapMemory(off_t address, size_t length){
	if (length == 0){
		return NULL;
	}

	int fd = open("/dev/mem", O_RDWR|O_SYNC);
	if (fd < 0){
		return NULL;
	}

	size_t pagesize = sysconf(_SC_PAGE_SIZE);
	length = ((length-1)/pagesize + 1) * pagesize;
	off_t remainder = address % pagesize;

	volatile void *mapping = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, address - remainder);
	if (mapping == MAP_FAILED){
		mapping = NULL;
		goto done;
	}

	mapping += remainder;

done:
	close(fd);
	return mapping;
}

uint32_t efuseRead(volatile uint32_t* efuse, uint8_t offset){
	efuse[regCTL] = (((uint32_t)offset) << 18) | cmdKey | cmdRead;
	while (efuse[regCTL] & cmdRead);
	return efuse[regRD];
}

void efusePrintContents(volatile uint32_t* efuse){
	printf("eFUSE dump:\n");
	printf("     ");
	for(int j=0; j<8; j++){
		printf("       %u ", j);
	}
	printf("\n");
	for(int i=0; i<64; i+=8){
		printf("%02X | ",i);
		for(int j=0; j<8; j++){
			printf("%08X ", efuseRead(efuse, i+j));
		}
		printf("\n");
	}
}

int efuseWrite(volatile uint32_t* efuse, uint8_t offset, uint32_t value){
	uint32_t current = efuseRead(efuse, offset);
	if (current == value){
		return 1;
	}

	/* Zeroes can be turned into ones but not the other way around */
	if((current | value) == value){
		for(uint8_t i=0; i<10; i++){
			efuse[regWD] = value;
			efuse[regCTL] = (((uint32_t)offset) << 18) | cmdKey | cmdWrite;
			while (efuse[regCTL] & cmdWrite);

			if (efuseRead(efuse, offset) == value){
				return 1;
			}
		}
	}

	return 0;
}

int efuseWriteArray(volatile uint32_t* efuse, uint8_t offset, uint32_t* value, uint8_t len){
	for (uint8_t i=0; i<len; i++){
		if (!efuseWrite(efuse, offset+i, value[i])){
			return 0;
		}
	}

	return 1;
}

void usage(){
	printf("***** WARNING: THIS PROGRAM CAN BRICK YOUR BOARD WHEN USED INCORRECTLY *****\n");
	printf("./efuse dump                         : Print eFUSE contents\n");
	printf("./efuse write offset filename.bin    : Write filename.bin contents to offset\n");
}

int main(int argc, char** argv){
	int retVal = 0;

	if (argc == 1){
		usage();
		return -1;
	}

	argc--;
	argv++;

	volatile uint32_t* efuse = (volatile uint32_t*)mmapMemory(efuseBase, 128);
	if (!efuse){
		printf("Failed to access efuse controller\n");
		return -1;
	}

	while(argc){
		printf("****************************************************************************\n");
		if (!strcmp(argv[0], "dump")){
			efusePrintContents(efuse);

			argc--;
			argv++;
		} else if (argc >= 3 && !strcmp(argv[0], "write")){
			FILE* f = fopen(argv[2], "rb");
			if (f){
				fseek(f, 0 , SEEK_END);
				unsigned int bytes = ftell(f);
				fseek(f, 0 , SEEK_SET);

				if (bytes%4){
					printf("Write length is not a multiple of 4\n");
					return -1;
				}

				uint32_t data[bytes/4];
				if (fread(data, 4, bytes/4, f) != bytes/4){
					printf("Read error: %s\n", strerror(errno));
					return -1;
				}

				int offset = strtol(argv[1], NULL, 0);
				if (offset >= 0x40){
					printf("Write out of range (%02x)\n", offset);
					return -1;
				}

				printf("Writing %u words at 0x%02x from '%s': ", bytes/4, offset, argv[2]);
				if (!efuseWriteArray(efuse, offset, data, bytes/4)){
					printf("Write failed\n");
					retVal = 1;
				}else{
					printf("Write successful\n");
				}
				fclose(f);
			}else{
				printf("Failed to open '%s': %s\n", argv[3], strerror(errno));
				return -1;
			}

			argc-=3;
			argv+=3;
		}else{
			printf("Command '%s' not understood\n", argv[0]);
			return -1;
		}
	}

	return retVal;
}
