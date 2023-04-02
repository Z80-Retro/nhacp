/**********************************************************************
 *
 * Filename:    main.c
 * 
 * Description: A simple test program for the CRC implementations.
 *
 * Notes:       To test a different CRC standard, modify crc.h.
 *
 * 
 * Copyright (c) 2000 by Michael Barr.  This software is placed into
 * the public domain and may be used for any purpose.  However, this
 * notice must not be changed or removed and no warranty is either
 * expressed or implied by its publication or distribution.
 **********************************************************************/

#include <stdio.h>
#include <string.h>

#include "crc.h"

uint8_t* tv[] = {
	"The quick brown fox jumps over the lazy dog.",
	"NABU HCCA application communication protocol"
};

int main(void)
{
	for (int i=0; i<sizeof(tv)/sizeof(tv[0]); ++i) {
		printf("The check value for the %s standard is 0x%X\n", CRC_NAME, CHECK_VALUE);
		printf("The crcSlow() of \"%s\" is 0x%X\n", tv[i], crcSlow(tv[i], strlen(tv[i])));
		crcInit();
		printf("The crcFast() of \"%s\" is 0x%X\n", tv[i], crcFast(tv[i], strlen(tv[i])));
	}
	return 0;
}
