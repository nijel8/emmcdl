/*****************************************************************************
* protocol.h
*
* This file implements the streaming download protocol
*
* Copyright (c) 2007-2015
* Qualcomm Technologies Incorporated.
* All Rights Reserved.
* Qualcomm Confidential and Proprietary
*
*****************************************************************************/
/*=============================================================================
Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/protocol.h#4 $
$DateTime: 2015/05/07 21:41:17 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
12/09/15   pgw     Initial version.
=============================================================================*/
#pragma once

#include "partition.h"
#include <windows.h>

#define MAX_XML_LEN         2048
#define MAX_TRANSFER_SIZE   0x100000

class Protocol {
public:
  //int ConnectToFlashProg(fh_configure_t *cfg);
  //int CreateGPP(DWORD dwGPP1, DWORD dwGPP2, DWORD dwGPP3, DWORD dwGPP4);
  //int SetActivePartition(int prtn_num);

  Protocol();
  ~Protocol();

  int DumpDiskContents(uint64 start_sector, uint64 num_sectors, TCHAR *szOutFile, UINT8 partNum, TCHAR *szPartName);
  int WipeDiskContents(uint64 start_sector, uint64 num_sectors, TCHAR *szPartName);

  int ReadGPT(bool debug);
  int WriteGPT(TCHAR *szPartName, TCHAR *szBinFile);
  void EnableVerbose(void);
  int GetDiskSectorSize(void);
  void SetDiskSectorSize(int size);
  uint64 GetNumDiskSectors(void);
  HANDLE GetDiskHandle(void);

  virtual int DeviceReset(void) = ERROR_SUCCESS;
  virtual int WriteData(BYTE *writeBuffer, __int64 writeOffset, DWORD writeBytes, DWORD *bytesWritten, UINT8 partNum) = ERROR_SUCCESS;
  virtual int ReadData(BYTE *readBuffer, __int64 readOffset, DWORD readBytes, DWORD *bytesRead, UINT8 partNum) = ERROR_SUCCESS;
  virtual int FastCopy(HANDLE hRead, __int64 sectorRead, HANDLE hWrite, __int64 sectorWrite, uint64 sectors, UINT8 partNum) = ERROR_SUCCESS;
  virtual int ProgramRawCommand(TCHAR *key) = ERROR_SUCCESS;
  virtual int ProgramPatchEntry(PartitionEntry pe, TCHAR *key) = ERROR_SUCCESS;

protected:

  int LoadPartitionInfo(TCHAR *szPartName, PartitionEntry *pEntry);
  void Log(char *str, ...);
  void Log(TCHAR *str, ...);

  gpt_header_t gpt_hdr;
  gpt_entry_t *gpt_entries;
  uint64 disk_size;
  HANDLE hDisk;
  BYTE *buffer1;
  BYTE *buffer2;
  BYTE *bufAlloc1;
  BYTE *bufAlloc2;
  int DISK_SECTOR_SIZE;

private:

  bool bVerbose;


};
