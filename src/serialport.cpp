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

#include "tchar.h"
#include "serialport.h"
#include "stdlib.h"

SerialPort::SerialPort()
{
  hPort = INVALID_HANDLE_VALUE;
  to_ms = 1000;  // 1 second default timeout for packets to send/rcv
  HDLCBuf = (BYTE *)malloc(MAX_PACKET_SIZE);

}

SerialPort::~SerialPort()
{
  if( hPort != INVALID_HANDLE_VALUE ) {
    CloseHandle(hPort);
  }
  if( HDLCBuf ) free(HDLCBuf);
}

int SerialPort::Open(int port)
{
  TCHAR tPath[32];
  
  swprintf_s(tPath,32,L"\\\\.\\COM%i",port );
  // Open handle to serial port and set proper port settings
  hPort = CreateFile( tPath,
                      (GENERIC_READ | GENERIC_WRITE),
                      FILE_SHARE_READ,
                      NULL,
                      OPEN_EXISTING,
                      0,
                      NULL);
  if( hPort != INVALID_HANDLE_VALUE ) {
    SetTimeout(to_ms);
  
    return ERROR_SUCCESS;
  }
  return GetLastError();
}

int SerialPort::Close()
{
  if( hPort != INVALID_HANDLE_VALUE ) {
    CloseHandle(hPort);
  }
  hPort = INVALID_HANDLE_VALUE;
  return ERROR_SUCCESS;
}

int SerialPort::Write(BYTE *data, DWORD length)
{
  int status = ERROR_SUCCESS;

  // Write data to serial port
  if( !WriteFile(hPort,data,length,&length,NULL) ) {
    // Write not complete
    status = GetLastError();
  }
  return status;
}

int SerialPort::Read(BYTE *data, DWORD *length)
{
  int status = ERROR_SUCCESS;

  // Read data in from serial port
  if( !ReadFile(hPort,data,*length,length,NULL) ) {
    status = GetLastError();
  }
  return status;
}

int SerialPort::Flush()
{
  BYTE tmpBuf[1024];
  DWORD len = sizeof(tmpBuf);
  
  // Set timeout to 1ms to just flush any pending data then change back to default
  SetTimeout(1);
  Read(tmpBuf, &len);
  SetTimeout(1000);

  PurgeComm(hPort, PURGE_RXABORT|PURGE_TXABORT|PURGE_RXCLEAR|PURGE_TXCLEAR);
  return ERROR_SUCCESS;
}

int SerialPort::SendSync(BYTE *out_buf, int out_length, BYTE *in_buf, int *in_length)
{
  DWORD status = ERROR_SUCCESS;
  DWORD bytesOut = 0;
  DWORD bytesIn = 0;
  
  // As long as hPort is valid write the data to the serial port and wait for response for timeout
  if( hPort == INVALID_HANDLE_VALUE ) {
    return ERROR_INVALID_HANDLE;
  }

  // Do HDLC encoding then send out packet
  bytesOut = MAX_PACKET_SIZE;
  status = HDLCEncodePacket(out_buf,out_length,HDLCBuf,(int *)&bytesOut);

  // We know we have a good handle now so write out data and wait for response
  status = Write(HDLCBuf,bytesOut) ;
  if( status != ERROR_SUCCESS) return status;

  // Keep looping through until we have received 0x7e with size > 1
  for(int i=0;i<MAX_PACKET_SIZE;i++) {
    // Now read response and and wait for up to timeout
    bytesIn = 1;
    status = Read(&HDLCBuf[i],&bytesIn);
    if( status != ERROR_SUCCESS ) return status;
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
  COMMTIMEOUTS commTimeout;
  commTimeout.ReadTotalTimeoutConstant = ms;
  commTimeout.ReadIntervalTimeout = MAXDWORD;
  commTimeout.ReadTotalTimeoutMultiplier = MAXDWORD;
  commTimeout.WriteTotalTimeoutConstant = ms;
  commTimeout.WriteTotalTimeoutMultiplier = MAXDWORD;
  SetCommTimeouts(hPort,&commTimeout);
  
  to_ms = ms;
  return ERROR_SUCCESS;
}

int SerialPort::HDLCEncodePacket(BYTE *in_buf, int in_length, BYTE *out_buf, int *out_length)
{
  BYTE *outPtr = out_buf;
  unsigned short crc = (unsigned short)CalcCRC16(in_buf,in_length);

  // Encoded packets start and end with 0x7E
  *outPtr++ = ASYNC_HDLC_FLAG;
  for(int i=0; i < in_length+2; i++ ) {
    // Read last two bytes of data from crc value
    if( i == in_length ) {
      in_buf = (BYTE *)&crc;
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
  return ERROR_SUCCESS;
}

int SerialPort::HDLCDecodePacket(BYTE *in_buf, int in_length, BYTE *out_buf, int *out_length)
{
  BYTE *outPtr = out_buf;
  // make sure our output buffer is large enough
  if( *out_length < in_length ) {
    return ERROR_NOT_ENOUGH_MEMORY;
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

  return ERROR_SUCCESS;
}