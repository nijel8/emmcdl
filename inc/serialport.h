/*****************************************************************************
 * serialport.h
 *
 * This file implements HDLC encoding and serial port interface
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/serialport.h#3 $
$DateTime: 2014/08/05 11:44:01 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
11/08/11   pgw     Initial version.
=============================================================================*/
#pragma once

#include <stdio.h>
#include <Windows.h>
#include "crc.h"

#define  ASYNC_HDLC_FLAG      0x7e
#define  ASYNC_HDLC_ESC       0x7d
#define  ASYNC_HDLC_ESC_MASK  0x20
#define  MAX_PACKET_SIZE      0x20000

class SerialPort {
public:
  SerialPort();
  ~SerialPort();
  int Open(int port);
  int EnableBinaryLog(TCHAR *szFileName);
  int Close();
  int Write(BYTE *data, DWORD length);
  int Read(BYTE *data, DWORD *length);
  int Flush();
  int SendSync(BYTE *out_buf, int out_length, BYTE *in_buf, int *in_length);
  int SetTimeout(int ms);
private:
  int HDLCEncodePacket(BYTE *in_buf, int in_length, BYTE *out_buf, int *out_length);
  int HDLCDecodePacket(BYTE *in_buf, int in_length, BYTE *out_buf, int *out_length);
  
  HANDLE hPort;
  BYTE *HDLCBuf;
  int to_ms;

};
