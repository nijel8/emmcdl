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
#ifdef HAVE_TERMIO_H
#include <termios.h>
#endif


SerialPort::SerialPort()
{
  hPort = -1;
  to_ms = 1000;  // 1 second default timeout for packets to send/rcv
  HDLCBuf = (unsigned char *)malloc(MAX_PACKET_SIZE);

}

SerialPort::~SerialPort()
{
  if( hPort != -1 ) {
    emmcdl_close(hPort);
  }
  if( HDLCBuf ) free(HDLCBuf);
}

int SerialPort::Open(int port)
{
  char tPath[32];
  
  sprintf(tPath,"/dev/ttyUSB%d",port );
  // Open handle to serial port and set proper port settings
  hPort = emmcdl_open( tPath,O_RDWR);

  if( hPort != -1 ) {
    SetTimeout(to_ms);
  
    return 0;
  }
  return errno;
}

int SerialPort::Close()
{
  if( hPort != -1 ) {
    emmcdl_close(hPort);
  }
  hPort = -1;
  return 0;
}

int SerialPort::Write(unsigned char *data, uint32_t length)
{
  int status = 0;

  // Write data to serial port
  if(emmcdl_write(hPort,data,length)< 0) {
    // Write not complete
    status = errno;
  }
  return status;
}

int SerialPort::Read(unsigned char *data, uint32_t *length)
{
  int status = 0;

  // Read data in from serial port
  if( emmcdl_read(hPort,data,*length) < 0) {
    status = errno;
  }
  return status;
}

int SerialPort::Flush()
{
  unsigned char tmpBuf[1024];
  uint32_t len = sizeof(tmpBuf);
  
  // Set timeout to 1ms to just flush any pending data then change back to default
  SetTimeout(1);
  Read(tmpBuf, &len);
  SetTimeout(1000);

#ifdef _WIN32
  PurgeComm(hPort, PURGE_RXABORT|PURGE_TXABORT|PURGE_RXCLEAR|PURGE_TXCLEAR);
#endif
  return 0;
}

int SerialPort::SendSync(unsigned char *out_buf, int out_length, unsigned char *in_buf, int *in_length)
{
  uint32_t status = 0;
  uint32_t bytesOut = 0;
  uint32_t bytesIn = 0;
  
  // As long as hPort is valid write the data to the serial port and wait for response for timeout
  if( hPort == -1 ) {
    return EBADF;
  }

  // Do HDLC encoding then send out packet
  bytesOut = MAX_PACKET_SIZE;
  status = HDLCEncodePacket(out_buf,out_length,HDLCBuf,(int *)&bytesOut);

  // We know we have a good handle now so write out data and wait for response
  status = Write(HDLCBuf,bytesOut) ;
  if( status != 0) return status;

  // Keep looping through until we have received 0x7e with size > 1
  for(int i=0;i<MAX_PACKET_SIZE;i++) {
    // Now read response and and wait for up to timeout
    bytesIn = 1;
    status = Read(&HDLCBuf[i],&bytesIn);
    if( status != 0 ) return status;
    if( bytesIn == 0 ) break;
    if( HDLCBuf[i] == 0x7E ) {
      if( i > 1 ) {
        bytesIn = i+1;
        break;
      } else {
        // If packet starts with multiple 0x7E ignore
        i = 0;
      }
    }

  }

  // Remove HDLC encoding and return the packet
  status = HDLCDecodePacket(HDLCBuf,bytesIn,in_buf,in_length);

  return status;
}

int SerialPort::SetTimeout(int ms)
{
#ifdef HAVE_TERMIO_H
    struct termios tio;

    if(tcgetattr(fd, &tio)) return;
    if(tcgetattr(fd, &tio_save)) return;

    tio.c_lflag = 0; /* disable CANON, ECHO*, etc */

        /* no timeout but request at least one character per read */
    tio.c_cc[VTIME] = ms;
    //tio.c_cc[VMIN] = 1;

    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIFLUSH);
#endif
  to_ms = ms;
  return 0;
}

int SerialPort::HDLCEncodePacket(unsigned char *in_buf, int in_length, unsigned char *out_buf, int *out_length)
{
  unsigned char *outPtr = out_buf;
  unsigned short crc = (unsigned short)CalcCRC16(in_buf,in_length);

  // Encoded packets start and end with 0x7E
  *outPtr++ = ASYNC_HDLC_FLAG;
  for(int i=0; i < in_length+2; i++ ) {
    // Read last two bytes of data from crc value
    if( i == in_length ) {
      in_buf = (unsigned char *)&crc;
    }
    
    if( *in_buf == ASYNC_HDLC_FLAG ||
        *in_buf == ASYNC_HDLC_ESC ) {
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

int SerialPort::HDLCDecodePacket(unsigned char *in_buf, int in_length, unsigned char *out_buf, int *out_length)
{
  unsigned char *outPtr = out_buf;
  // make sure our output buffer is large enough
  if( *out_length < in_length ) {
    return ENOMEM;
  }

  for(int i=0; i < in_length; i++) {
    switch(*in_buf) {
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
