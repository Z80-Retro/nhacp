//****************************************************************************
//
//    Copyright (C) 2023 John Winans
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Lesser General Public
//    License as published by the Free Software Foundation; either
//    version 2.1 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
//    USA
//
//****************************************************************************

//****************************************************************************
//
//	Just enough of an NHACP server to provide simple file access to the 
//	Z80 retro to access disk image files from a server.
//
//****************************************************************************


#include "serial.h"
#include "hexdump.h"
#include "dbg.h"

#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>


//****************************************************************************
//
// Observation: The client initiates ALL messages.
// All messages from the client must has a session-header.  
// In the simple case, this can simply be 0x8f 0x00.
//
//	The protocol must start with a client-issued HELLO:
//		0x8f 	
//		0x00 [SYSTEM] 	
//		0x08 0x00 				- length
//		0x00 [HELLO] 			- message type
//		"ACP" 					- magic
//		0x01 0x00 				- protocol version number = 1
//		0x00 0x00				- options = none
//
//	Response to the hello request must be:
//
//		0x15 0x00				- length
//		0x80 [SESSION-STARTED]	- message type
//		0x00					- session-ID = 0
//		0x01 0x00				- protocol version number = 1
//		0x10					- string length
//		"NABU-ADAPTOR-1.1"		- string value (sans terminator)
//
//	Open a file:
//
//		0x8f
//		0x00 [SYSTEM]
//		0x.. 0x00				- length
//		0x01 [STORAGE-OPEN]		- message type
//		0x00 					- want to use file descriptior zero
//		0x00 0x00 				- flags = read only
//		0x.. 					- string length
//		"filename.URL.here"
//
//	Response to the file-open request:
//
//		0x06 0x00				- length
//		0x83 [STORAGE-LOADED]	- message type
//		0x00					- file descriptior
//		0x00 0x04 0x00 0x00		- file length (in bytes)
// 
//	Read a block from the file:
//
//		0x8f
//		0x00 [SYSTEM]
//		0x.. 0x..				- length
//		0x07 [STORAGE-GET-BLOCK]
//		0x..					- file descriptior
//		0x.. 0x..0x..0x..		- 0-based block index number
//		0x.. 0x..				- block length
//
//	Read response:
//
//		0x.. 0x..				- length
//		0x84 [DATA-BUFFER]
//		0x..0x..				- Length of the following data buffer
//		0x.......				- data bytes
//
//****************************************************************************



// The "login" message from the client
#pragma pack(1)
struct req_hello {
	uint8_t		req;		// 0x8f
	uint8_t		session;	// 0x00
	uint16_t	mlen;		// 0x0008
	uint8_t		type;		// 0x00
	uint8_t		magic[3];	// "ACP"
	uint16_t	version;	// 0x0001
	uint16_t	options;	// 0x0000
};
#pragma pack()

#pragma pack(1)
struct res_session_started {
	uint16_t	mlen;		// 0x0015
	uint8_t		type;		// 0x80
	uint8_t		session_id;	// 0x00
	uint16_t	version;	// 0x0001
	uint8_t		slen;		// 0x10
	uint8_t		str[];		// "NABU-ADAPTOR-1.1"
};
#pragma pack()


#pragma pack(1)
struct req_storage_open {
	uint8_t		req;		// 0x8f
	uint8_t		session;	// 0x00
	uint16_t	mlen;		// 0x....
	uint8_t		type;		// 0x01
	uint8_t		fd;			// 0x..
	uint16_t	flags;		// 0x0000
	uint8_t		slen;		// ..
	uint8_t		url[];		// slen bytes
};
#pragma pack()

#pragma pack(1)
struct res_storage_loaded {
	uint16_t	mlen;		// 0x0006
	uint8_t		type;		// 0x83
	uint8_t		fd;			// 0x..
	uint32_t	flen;		// file size/length
};
#pragma pack()


#pragma pack(1)
struct req_get_block {
	uint8_t		req;		// 0x8f
	uint8_t		session;	// 0x00
	uint16_t	mlen;		// 0x....
	uint8_t		type;		// 0x07
	uint8_t		fd;			// 0x..
	uint32_t	ix;			// block number
	uint16_t	blen;		// block size
};
#pragma pack()

#pragma pack(1)
struct res_data_buffer {
    uint16_t    mlen;       // 0x0006
    uint8_t     type;       // 0x84
	uint16_t	dlen;		// size of data
	uint8_t		data[];		// data block
};
#pragma pack()


#pragma pack(1)
struct req_put_block {
	uint8_t		req;		// 0x8f
	uint8_t		session;	// 0x00
	uint16_t	mlen;		// 0x....
	uint8_t		type;		// 0x08
	uint8_t		fd;			// 0x..
	uint32_t	ix;			// block number
	uint16_t	blen;		// block size
	uint8_t		data[];		// the data
};
#pragma pack()

#pragma pack(1)
struct res_ok {
    uint16_t    mlen;       // 0x0006
    uint8_t     type;       // 0x81
};
#pragma pack()

#pragma pack(1)
struct res_error {
    uint16_t    mlen;       // 0x0006
    uint8_t     type;       // 0x82
	uint16_t	code;		// the type of error
	uint8_t		strlen;
	uint8_t		str[];
};
#pragma pack()



#pragma pack(1)
struct msg {
	struct {
		uint8_t		session;
		uint8_t		req;
		uint16_t	mlen;
		uint8_t		type;
	} hdr;
	struct {
		char		buf[8192];
	} body;
};
#pragma pack()



enum MSG_TYPE { 
	MSG_TYPE_HELLO = 0x00,
	MSG_TYPE_STORAGE_OPEN = 0x01,
	MSG_TYPE_STORAGE_GET = 0x02,
	MSG_TYPE_STORAGE_PUT = 0x03,
	MSG_TYPE_GET_DATE_TIME = 0x04,
	MSG_TYPE_FILE_CLOSE = 0x05,
	MSG_TYPE_GET_ERROR_DETAILS = 0x06,
	MSG_TYPE_STORAGE_GET_BLOCK = 0x07,
	MSG_TYPE_STORAGE_PUT_BLOCK = 0x08,
	MSG_TYPE_FILE_READ = 0x09,
	MSG_TYPE_FILE_WRITE = 0x0a,
	MSG_TYPE_FILE_SEEK = 0x0b,
	MSG_TYPE_FILE_GET_INFO = 0x0c,
	MSG_TYPE_FILE_SET_SIZE = 0x0d,
	MSG_TYPE_LIST_DIR = 0x0e,
	MSG_TYPE_GET_DIR_ENTRY = 0x0f,
	MSG_TYPE_REMOVE = 0x10,
	MSG_TYPE_RENAME = 0x11,
	MSG_TYPE_MKDIR = 0x12,

	MSG_TYPE_SESSION_STARTED = 0x80,
	MSG_TYPE_OK = 0x81,
	MSG_TYPE_ERROR = 0x82,
	MSG_TYPE_STORAGE_LOADED = 0x83,
	MSG_TYPE_DATA_BUFFER = 0x84,
	MSG_TYPE_DATE_TIME = 0x85,
	MSG_TYPE_FILE_INFO = 0x86,
	MSG_TYPE_UINT8_VALUE = 0x87,
	MSG_TYPE_UINT16_VALUE = 0x88,
	MSG_TYPE_UINT32_VALUE = 0x89,
	MSG_TYPE_FILE_ATTRS = 0x8a,

	MSG_TYPE_GOODBYE = 0xef
};

enum error {
	ENOTSUP = 123		// XXX
};


/**
* Read up to len bytes with a timeout.
*
* @return 0 EOF
* @return -1 An I/O error has been encountered. 
* @return >0 The number of bytes have been read into buf.
*****************************************************************************/
static int read_timeout(int fd, void *buf, size_t len)
{
	int numread = 0;	///< how many bytes we read in so far

	// XXX Meh... Good enough.. Hardcode the timeout to 1 second.
	struct timeval	tv = { .tv_sec = 1, .tv_usec = 0 };
	fd_set  		fds;
	char*			p = (char*)buf;

	while (numread < len) {

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		int rc = select(fd+1, &fds, NULL, NULL, &tv);

		if (rc == -1) {
			perror("select()");
			return -1;
		} else if (rc) {
			rc = read(fd, &p[numread], len-numread);
			if (rc == -1) {
				perror("read");
				return -1;
			} else if (rc == 0) {
				return -1;		// end of file
			} else {
				numread += rc;
			}
		} else {
			return 0;
		}

		// XXX if not on linux, need to reset the timeout value here
	}

#if 1
	DBG("RX:\n");
	hexdump((char*)&buf, sizeof(numread));
#endif

	return numread;
}




/**
* Read a request message.  If the data stops arriving before the timeout 
* period  has passed then return a timeout error.
*
*****************************************************************************/
static int read_req(int fd, struct msg *msg)
{
	int rc;
	int len = 0;

	msg->hdr.req = 0xff;

	// read and discard bytes until we receive a 0x8f
	while(msg->hdr.req != 0x8f) {
		if (read_timeout(fd, &msg->hdr.req, 1) == 1) {
			printf(".");
			fflush(stdout);
		}
	}

	// read the rest of the message header
	if ((rc = read_timeout(fd, &msg->hdr.session, sizeof(msg->hdr)-1)) != 0) {
		return rc;
	}
	len += rc;

	if ( msg->hdr.mlen > sizeof(struct msg) ) {
		return -1;		// illegal too big message
	}

	// read the message body
	if ((rc = read_timeout(fd, &msg->body, msg->hdr.mlen)) != 0) {
		return rc;
	}
	len += rc;

#if 1
	printf("read_req():\n");
	hexdump((char*)&msg, len);
#endif

	return len;
}


// state = wait_hello, conneted,...

/**
*****************************************************************************/
static int send_error(int fd, int err)
{
	DBG("entered: err=%d\n", err);

	// format the response message
	// write it
	return 0;
}


/**
*****************************************************************************/
static int process_hello(int fd, struct req_hello* msg)
{
	DBG("entered\n");

	// close any open files
	// res_session_started(fd, 0);
	return 0;
}

/**
*****************************************************************************/
static int process_storage_open(int fd, struct req_storage_open* msg)
{
	DBG("entered\n");

	// find a slot for the requested file descriptor
	// res_storage_loaded(fd, desc)
	return 0;
}

/**
*****************************************************************************/
static int process_storage_get_block(int fd, struct req_get_block* msg)
{
	DBG("entered\n");

	// verify the fd
	// seek to the requested index
	// read the file data
	// res_data_buffer(fd, buf, len)
	return 0;
}

/**
*****************************************************************************/
static int process_storage_put_block(int fd, struct req_put_block* msg)
{
	DBG("entered\n");

	send_error(fd, ENOTSUP);
	return 0;
}

/**
* Read and reply to NHACP messages.
*****************************************************************************/
static void nhacp(int port)
{
	int rc;

	union {
		struct msg					msg;
		struct req_hello			req_hello;
		struct req_storage_open		req_storage_open;
		struct req_get_block		req_get_block;
		struct req_put_block		req_put_block;
	} buf;


	while (1) {
		while ((rc = read_req(port, &buf.msg)) <= 0) {
			if (!rc)
				return;
			printf("timeout\n");
		}

//#warning "validate the req message length matches the type in here"

		switch(buf.msg.hdr.type) 
		{
		case MSG_TYPE_HELLO:
			process_hello(port, &buf.req_hello);
			break;

		case MSG_TYPE_STORAGE_OPEN:
			process_storage_open(port, &buf.req_storage_open);
			break;

		case MSG_TYPE_STORAGE_GET_BLOCK:
			process_storage_get_block(port, &buf.req_get_block);
			break;

		case MSG_TYPE_STORAGE_PUT_BLOCK:
			process_storage_put_block(port, &buf.req_put_block);
			break;

		default:
			send_error(port, ENOTSUP);
		}
	}
}



/**
*****************************************************************************/
static void usage()
{
	fprintf(stderr, "Options:\n"
		"    [-t tty]    Specify the TTY to use.\n");
	exit(1);
}

/**
*****************************************************************************/
int main(int argc, char **argv)
{
    int port;
    //const char *tty = "/dev/ttyUSB0";		// default tty name
    const char *tty = "/dev/ttyUSB1";		// default tty name
	int ch;
	int rts = 0;
	int terminal = 0;

	speed_t speed = B115200;

    extern char *optarg;

	while((ch = getopt(argc, argv, "4t:x")) != -1)
	{
		switch (ch)
		{
		case 't':		// tty port
			tty = optarg;
			break;

		case '4':	// RS485 set RTS to turn off transmitter
			rts = 1;
			break;
			
		case 'x':	// open, and start a dumb terminal
			terminal = 1;
			break;

		default:
			usage();
		}
	}

	printf("opening %s\n", tty);

    /* Open the tty */
    if ((port = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK, 0)) < 0)
    {
        printf("Can not open '%s'\n", tty);
        exit(-1);
    }
    initPort(port, speed);

	if (rts)
		setControlLines(port, 1, 0);

	if (terminal)
		doStream(port);		// uber-dumb terminal
	else
		nhacp(port);

	close(port);
	return(0);
}
