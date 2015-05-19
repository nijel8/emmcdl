/*****************************************************************************
 * diskwriter.h
 *
 * This file defines the interface for the DiskWriter class
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/diskwriter.h#7 $
$DateTime: 2015/04/29 17:06:00 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
10/10/11   pgw     Fix up spacing and add hVolume
03/21/11   pgw     Initial version.
=============================================================================*/
#pragma once

#include "partition.h"
#include "protocol.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>

#define WRITE_TIMEOUT_MS    3000
#define MAX_VOLUMES         26
#define MAX_DISKS         26


typedef struct {
  __int32	     serialnum;
  __int32      drivetype;
  uint64      volsize;
  int          disknum;
  TCHAR        fstype[MAX_PATH+1];
  TCHAR        mount[MAX_PATH+1];
  TCHAR        rootpath[MAX_PATH+1];
  TCHAR        volume[MAX_PATH+1];
} vol_entry_t;

typedef struct {
  uint64      disksize;
  int          blocksize;
  int          disknum;
  int          volnum[MAX_VOLUMES+1];
  TCHAR        diskname[MAX_PATH+1];
} disk_entry_t;

class DiskWriter : public Protocol {
public:
  int diskcount;
  bool bPatchDisk;

  DiskWriter();
  ~DiskWriter();

  int WriteData(BYTE *writeBuffer, __int64 writeOffset, DWORD writeBytes, DWORD *bytesWritten, UINT8 partNum);
  int ReadData(BYTE *readBuffer, __int64 readOffset, DWORD readBytes, DWORD *bytesRead, UINT8 partNum);

  int FastCopy(HANDLE hRead, __int64 sectorRead, HANDLE hWrite, __int64 sectorWrite, uint64 sectors, UINT8 partNum=0);
  int OpenDevice(int dn);
  int OpenDiskFile(TCHAR *oFile, uint64 sectors);
  void CloseDevice();
  int InitDiskList(bool verbose = false);
  int DeviceReset(void);
  int ProgramPatchEntry(PartitionEntry pe, TCHAR *key);
  int ProgramRawCommand(TCHAR *key);

  // Functions for testing purposes
  int CorruptionTest(uint64 offset);
  int DiskTest(uint64 offset);
  int WipeLayout();

protected:

private:
  HANDLE hVolume;
  int disk_num;
  HANDLE hFS;
  OVERLAPPED ovl;
  disk_entry_t *disks;
  vol_entry_t *volumes;

  int GetVolumeInfo(vol_entry_t *vol);
  int GetDiskInfo(disk_entry_t *de);
  TCHAR *TCharToString(TCHAR *p);
  int UnmountVolume(vol_entry_t vol);
  int LockDevice();
  int UnlockDevice();
  bool IsDeviceWriteable();
  int GetRawDiskSize(uint64 *ds);
  int RawReadTest(uint64 offset);
};
