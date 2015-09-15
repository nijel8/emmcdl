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
#include "usb.h"
//#include <sys/ioctl.h>

static const char *serial = 0;
static unsigned short vendor_id = 0;
static int long_listing = 0;
static char ERROR[128];

SerialPort::SerialPort() {
	hPort = NULL;
	to_ms = 1000;  // 1 second default timeout for packets to send/rcv
	HDLCBuf = (unsigned char *) malloc(MAX_PACKET_SIZE);

}

SerialPort::~SerialPort() {
	if (hPort) {
		//TODO
	}
	if (HDLCBuf)
		free(HDLCBuf);
}

int match_fastboot_with_serial(usb_ifc_info *info, const char *local_serial)
{
    if(!(vendor_id && (info->dev_vendor == vendor_id)) &&
       (info->dev_vendor != 0x05c6))// Qualcomm
            return -1;
    if (info->dev_product  != 0x9008) return -1;

    if(info->ifc_class != 0xff) return -1;
    if(info->ifc_subclass != 0xff) return -1;
    if(info->ifc_protocol != 0xff) return -1;
    if(info->ifc_protocol != 0xff) return -1;

    // require matching serial number or device path if requested
    // at the command line with the -s option.
    if (local_serial && (strcmp(local_serial, info->serial_number) != 0 &&
                   strcmp(local_serial, info->device_path) != 0)) return -1;
    return 0;
}

int match_fastboot(usb_ifc_info *info)
{
    return match_fastboot_with_serial(info, serial);
}

int list_devices_callback(usb_ifc_info *info)
{
    if (match_fastboot_with_serial(info, NULL) == 0) {
        const char* serial = info->serial_number;
        if (!info->writable) {
            serial = "no permissions"; // like "adb devices"
        }
        if (!serial[0]) {
            serial = "????????????";
        }
        // output compatible with "adb devices"
        if (!long_listing) {
            printf("%s\tfastboot\n", serial);
        } else if (strcmp("", info->device_path) == 0) {
            printf("%-22s fastboot\n", serial);
        } else {
            printf("%-22s fastboot %s\n", serial, info->device_path);
        }
    }

    return -1;
}

usb_handle *open_device(void)
{
    static usb_handle *usb = 0;
    int announce = 1;

    if(usb) return usb;

    for(;;) {
        usb = usb_open(match_fastboot);
        if(usb) return usb;
        if(announce) {
            announce = 0;
            fprintf(stderr,"< waiting for device >\n");
        }
        usleep(1000);
    }
}

void list_devices(void) {
    // We don't actually open a USB device here,
    // just getting our callback called so we can
    // list all the connected devices.
    usb_open(list_devices_callback);
}

int SerialPort::Open(int port) {
  usb_handle *usb = open_device();
  hPort = usb;
  return 0;
}

int SerialPort::Close() {
	if (hPort) {
           //TODO
	}

	hPort = NULL;
	return 0;
}

int SerialPort::Write(unsigned char *data, uint32_t length) {
    int r;

    r = usb_write(hPort, data, length);
    if(r < 0) {
        sprintf(ERROR, "data transfer failure (%s)", strerror(errno));
        usb_close(hPort);
        return -1;
    }
    if(r != ((int) length)) {
        sprintf(ERROR, "data transfer failure (short transfer)");
        usb_close(hPort);
        return -1;
    }

    return r;
}

int SerialPort::Read(unsigned char *data, uint32_t *length) {
    int r;
        r = usb_read(hPort, data, *length);
        if(r < 0) {
            sprintf(ERROR, "status read failed (%s)", strerror(errno));
            usb_close(hPort);
            return -1;
        }

	*length = r;
	return r ? 0 : -1;
}

int64_t SerialPort::InputBufferCount(){
    int64_t count = 0;
    //ioctl(hPort, TIOCINQ, &count);
    return count;
}

int64_t SerialPort::OutputBufferCount(){
    int64_t count = 0;
    //ioctl(hPort, TIOCOUTQ, &count);
    return count;
}

int SerialPort::Flush() {
	unsigned char tmpBuf[1024];
	uint32_t len = sizeof(tmpBuf);

	// Set timeout to 1ms to just flush any pending data then change back to default
	SetTimeout(1);
	Read(tmpBuf, &len);
	SetTimeout(1000);

	//tcflush(hPort, TCIOFLUSH);
	return 0;
}

int SerialPort::SendSync(unsigned char *out_buf, int out_length,
		unsigned char *in_buf, int *in_length) {
	uint32_t status = 0;
	uint32_t bytesOut = 0;
	uint32_t bytesIn = 0;

	// As long as hPort is valid write the data to the serial port and wait for response for timeout
	if (hPort == NULL) {
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
