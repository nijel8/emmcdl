/*****************************************************************************
 * firehose.h
 *
 * This file implements the streaming download protocol
 *
 * Copyright (c) 2007-2013
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/firehose.h#11 $
$DateTime: 2015/04/01 17:01:45 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
02/06/13   pgw     Initial version.
=============================================================================*/
#pragma once

#include "serialport.h"
#include "protocol.h"
#include "partition.h"
#include <stdio.h>
#include <stdint.h>
#include "sysdeps.h"

#define MAX_RETRY   50

typedef struct {
  unsigned char Version;
  char MemoryName[8];
  bool SkipWrite;
  bool SkipStorageInit;
  bool ZLPAwareHost;
  int ActivePartition;
  int MaxPayloadSizeToTargetInBytes;
  int AckRawDataEveryNumPackets;
} fh_configure_t;

class Firehose : public Protocol {
public:
  Firehose(SerialPort *port, uint32_t maxPacketSize = (1024*1024), int hLogFile = -1);
  virtual ~Firehose();
  int WriteData(unsigned char *writeBuffer, int64_t writeOffset, uint32_t writeBytes, uint32_t *bytesWritten, uint8_t partNum);
  int ReadData(unsigned char *readBuffer, int64_t readOffset, uint32_t readBytes, uint32_t *bytesRead, uint8_t partNum);

  int DeviceReset(void);
  int FastCopy(int hRead, int64_t sectorRead, int hWrite, int64_t sectorWrite, __uint64_t sectors, uint8_t partNum);
  int ProgramPatchEntry(PartitionEntry pe, char *key);
  int ProgramRawCommand(char *key);
  int PeekLogBuf(int64_t start, int64_t size);


  // Firehose specific operations
  int CreateGPP(uint32_t dwGPP1, uint32_t dwGPP2, uint32_t dwGPP3, uint32_t dwGPP4);
  int SetActivePartition(int prtn_num);
  int ConnectToFlashProg(fh_configure_t *cfg);

protected:

private:
  int ReadData(unsigned char *pOutBuf, uint32_t uiBufSize, bool bXML);
  int ReadStatus(void);

  SerialPort *sport;
  __uint64_t diskSectors;
  bool m_read_back_verify;
  bool m_rawmode;
  unsigned char *m_payload;
  unsigned char *m_buffer;
  unsigned char *m_buffer_ptr;
  uint32_t m_buffer_len;
  uint32_t dwMaxPacketSize;
  int hLog;
  char *program_pkt;
};
