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
#ifdef _WIN32
#ifdef _WIN32
#include <Windows.h>
#endif
#endif

#define MAX_RETRY   50

typedef struct {
  unsigned char Version;
  char MemoryName[8];
  bool SkipWrite;
  bool SkipStorageInit;
  bool ZLPAwareHost;
  int ActivePartition;
  int MaxPayloadSizeToTargetInBytes;
} fh_configure_t;

class Firehose : public Protocol {
public:
  Firehose(SerialPort *port, HANDLE hLogFile = NULL);
  ~Firehose();

  int WriteData(unsigned char *writeBuffer, int64_t writeOffset, uint32_t writeBytes, uint32_t *bytesWritten, UINT8 partNum);
  int ReadData(unsigned char *readBuffer, int64_t readOffset, uint32_t readBytes, uint32_t *bytesRead, UINT8 partNum);

  int DeviceReset(void);
  int FastCopy(HANDLE hRead, int64_t sectorRead, HANDLE hWrite, int64_t sectorWrite, uint64 sectors, UINT8 partNum);
  int ProgramPatchEntry(PartitionEntry pe, TCHAR *key);
  int ProgramRawCommand(TCHAR *key);

  // Firehose specific operations
  int CreateGPP(uint32_t dwGPP1, uint32_t dwGPP2, uint32_t dwGPP3, uint32_t dwGPP4);
  int SetActivePartition(int prtn_num);
  int ConnectToFlashProg(fh_configure_t *cfg);

protected:

private:
  int ReadData(unsigned char *pOutBuf, uint32_t uiBufSize, bool bXML);
  int ReadStatus(void);

  SerialPort *sport;
  uint64 diskSectors;
  bool bSectorAddress;
  unsigned char *m_payload;
  unsigned char *m_buffer;
  unsigned char *m_buffer_ptr;
  uint32_t m_buffer_len;
  UINT32 dwMaxPacketSize;
  HANDLE hLog;
  char *program_pkt;
};
