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
#include "sysdeps.h"
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
  int EnableBinaryLog(char *szFileName);
  int Close();
  int Write(unsigned char *data, uint32_t length);
  int Read(unsigned char *data, uint32_t *length);
  int Flush();
  int SendSync(unsigned char *out_buf, int out_length, unsigned char *in_buf, int *in_length);
  int SetTimeout(int ms);
  int64_t OutputBufferCount();
  int64_t InputBufferCount();
private:
  int HDLCEncodePacket(unsigned char *in_buf, int in_length, unsigned char *out_buf, int *out_length);
  int HDLCDecodePacket(unsigned char *in_buf, int in_length, unsigned char *out_buf, int *out_length);
  
  int hPort;
  unsigned char *HDLCBuf;
  int to_ms;

};
