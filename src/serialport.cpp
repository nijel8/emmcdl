/*****************************************************************************
 * serialport.cpp
 *
 * This file implements the serialport and HDLC protocols and interface
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
 Edit History

 $Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/serialport.cpp#3 $
 $DateTime: 2014/12/29 17:28:07 $ $Author: pweber $

 when       who     what, where, why
 -------------------------------------------------------------------------------
 11/08/11   pgw     Initial version.
 =============================================================================*/

#include "serialport.h"
#include "stdlib.h"
#include <termios.h>
#include <sys/ioctl.h>

SerialPort::SerialPort() {
	hPort = -1;
	to_ms = 1000;  // 1 second default timeout for packets to send/rcv
	HDLCBuf = (unsigned char *) malloc(MAX_PACKET_SIZE);

}

SerialPort::~SerialPort() {
	if (hPort >= 0) {
		emmcdl_close(hPort);
	}
	if (HDLCBuf)
		free(HDLCBuf);
}

int SerialPort::Open(int port) {
	char tPath[32];
	struct termios tio;

	sprintf(tPath, "/dev/ttyUSB%d", port);
	// Open handle to serial port and set proper port settings
	hPort = emmcdl_open(tPath, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (hPort < 0) {
		return -1;
	}

	bzero(&tio, sizeof(tio));
	tio.c_lflag = 0; /*disable CANON, ECHO*, etc */
	tio.c_cflag = /*B115200 |*/CS8 | CLOCAL | CSTOPB | CREAD;
	tio.c_iflag = IGNPAR;
	tio.c_oflag = 0;

	/*to_ms*10 timeout*/
	tio.c_cc[VTIME] = to_ms * 10;
	tio.c_cc[VMIN] = 0;
	//tio.c_cc[VSTART] = 0x11;
	//tio.c_cc[VSTOP] = 0x13;

	tcflush(hPort, TCIFLUSH);

	if (tcsetattr(hPort, TCSANOW, &tio)) {
		printf("config serial port fail %d:%s\n", errno, strerror(errno));
		return -1;
	}

	return 0;
}

int SerialPort::Close() {
	if (hPort != -1) {
		emmcdl_close(hPort);
	}

	hPort = -1;
	return 0;
}

int SerialPort::Write(unsigned char *data, uint32_t length) {
	int N = 0;
	ssize_t wsize = 0;
	ssize_t csize = 0;
	ssize_t len = length;

	fd_set wfds;
	struct timeval tv;
	int retval;
	time_t tv_sec = ((length*5)/(1024*16));

	if (tv_sec < 5) tv_sec = 5;

	do {
tryagain:
		/* Watch Serial Port to see when it has output. */
		FD_ZERO(&wfds);

		FD_SET(hPort, &wfds);

		/* Wait up to five seconds. */
		tv.tv_sec = tv_sec;
		tv.tv_usec = 0;
		/* Don't rely on the value of tv now! */
		retval = TEMP_FAILURE_RETRY (select(hPort + 1, NULL, &wfds, NULL, &tv));
		if (retval == -1)
			perror("select()");
		else if (retval) {
			//printf("Serial Port Output is ready now.\n");
			/* FD_ISSET(hPort, &rfds) will be true. */
			// Write data to serial port
			csize = emmcdl_write(hPort, data + wsize, len);
			if (csize < 0 && errno == EAGAIN) {
				// Write not complete
				printf("%s EAGAIN\n", __func__);
			} else if (csize > 0) {
				wsize += csize;
				len -= csize;
				N = 0;
			} else {
				emmcdl_sleep_ms(10);
			}
		} else {
			printf("Serial Port Output not ready within %ld seconds. try read again:%d\n", tv_sec, ++N);
			goto tryagain;
		}
	} while (len);
	return len ? -1 : 0;
}

int SerialPort::Read(unsigned char *data, uint32_t *length) {
	ssize_t rsize = 0;
	ssize_t csize = 0;
	fd_set rfds;
	struct timeval tv;
	int retval;


	do {
		/* Watch Serial Port to see when it has input. */
		FD_ZERO(&rfds);

		FD_SET(hPort, &rfds);

		/* Wait up to five seconds. */
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		/* Don't rely on the value of tv now! */
		retval = TEMP_FAILURE_RETRY (select(hPort + 1, &rfds, NULL, NULL, &tv));

		if (retval == -1)
			perror("select()");
		else if (retval) {
//tryagain:
			//printf("Serial port Data is available now.\n");
			/* FD_ISSET(hPort, &rfds) will be true. */
			// Read data in from serial port
			csize = emmcdl_read(hPort, data + rsize, (*length) - rsize);
			if (csize < 0 && errno == EAGAIN) {
				// Read not complete
				printf("%s EAGAIN\n", __func__);
				continue;
			} else if (csize > 0) {
				rsize += csize;
			}
		} else {
			printf("Serial Port No data within five seconds.\n");
			//goto tryagain;
			return -1;
		}

	} while (csize < 0 /*&& (rsize < *length)*/);
	*length = rsize;

	return 0;
}

int64_t SerialPort::InputBufferCount(){
    int64_t count = 0;
    ioctl(hPort, TIOCINQ, &count);
    return count;
}

int64_t SerialPort::OutputBufferCount(){
    int64_t count = 0;
    ioctl(hPort, TIOCOUTQ, &count);
    return count;
}

int SerialPort::Flush() {
	unsigned char tmpBuf[1024];
	uint32_t len = sizeof(tmpBuf);

	// Set timeout to 1ms to just flush any pending data then change back to default
	SetTimeout(1);
	Read(tmpBuf, &len);
	SetTimeout(1000);

	tcflush(hPort, TCIOFLUSH);
	return 0;
}

int SerialPort::SendSync(unsigned char *out_buf, int out_length,
		unsigned char *in_buf, int *in_length) {
	uint32_t status = 0;
	uint32_t bytesOut = 0;
	uint32_t bytesIn = 0;

	// As long as hPort is valid write the data to the serial port and wait for response for timeout
	if (hPort == -1) {
		return EBADF;
	}

	// Do HDLC encoding then send out packet
	bytesOut = MAX_PACKET_SIZE;
	status = HDLCEncodePacket(out_buf, out_length, HDLCBuf, (int *) &bytesOut);

	// We know we have a good handle now so write out data and wait for response
	status = Write(HDLCBuf, bytesOut);
	if (status != 0)
		return status;

	// Keep looping through until we have received 0x7e with size > 1
	for (int i = 0; i < MAX_PACKET_SIZE; i++) {
		// Now read response and and wait for up to timeout
		bytesIn = 1;
		status = Read(&HDLCBuf[i], &bytesIn);
		if (status != 0)
			return status;
		if (bytesIn == 0)
			break;
		if (HDLCBuf[i] == 0x7E) {
			if (i > 1) {
				bytesIn = i + 1;
				break;
			} else {
				// If packet starts with multiple 0x7E ignore
				i = 0;
			}
		}

	}

	// Remove HDLC encoding and return the packet
	status = HDLCDecodePacket(HDLCBuf, bytesIn, in_buf, in_length);

	return status;
}

int SerialPort::SetTimeout(int ms) {
#if 0
	struct termios tio;

	if (tcgetattr(hPort, &tio)) {
		printf("get serial port config fail %d:%s\n", errno, strerror(errno));
		return -1;
	}

	tio.c_cc[VTIME] = ms * 10;

	//tcflush(hPort, TCIFLUSH);

	if (tcsetattr(hPort, TCSANOW, &tio)) {
		printf("config serial port fail %d:%s\n", errno, strerror(errno));
		return -1;
	}
#endif
	to_ms = ms;
	return 0;
}

int SerialPort::HDLCEncodePacket(unsigned char *in_buf, int in_length,
		unsigned char *out_buf, int *out_length) {
	unsigned char *outPtr = out_buf;
	unsigned short crc = (unsigned short) CalcCRC16(in_buf, in_length);

	// Encoded packets start and end with 0x7E
	*outPtr++ = ASYNC_HDLC_FLAG;
	for (int i = 0; i < in_length + 2; i++) {
		// Read last two bytes of data from crc value
		if (i == in_length) {
			in_buf = (unsigned char *) &crc;
		}

		if (*in_buf == ASYNC_HDLC_FLAG || *in_buf == ASYNC_HDLC_ESC) {
			*outPtr++ = ASYNC_HDLC_ESC;
			*outPtr++ = *in_buf++ ^ ASYNC_HDLC_ESC_MASK;
		} else {
			*outPtr++ = *in_buf++;
		}
	}
	*outPtr++ = ASYNC_HDLC_FLAG;

	// Update length of packet
	*out_length = outPtr - out_buf;
	return 0;
}

int SerialPort::HDLCDecodePacket(unsigned char *in_buf, int in_length,
		unsigned char *out_buf, int *out_length) {
	unsigned char *outPtr = out_buf;
	// make sure our output buffer is large enough
	if (*out_length < in_length) {
		return ENOMEM;
	}

	for (int i = 0; i < in_length; i++) {
		switch (*in_buf) {
		case ASYNC_HDLC_FLAG:
			// Do nothing it is an escape flag
			in_buf++;
			break;
		case ASYNC_HDLC_ESC:
			in_buf++;
			*outPtr++ = *in_buf++ ^ ASYNC_HDLC_ESC_MASK;
			break;
		default:
			*outPtr++ = *in_buf++;
		}
	}

	*out_length = outPtr - out_buf;
	// Should do CRC check here but for now we are using USB so assume good

	return 0;
}
