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
#include "sysdeps.h"
#include <string.h>
#include <stdlib.h>

#define WRITE_TIMEOUT_MS    3000
#define MAX_VOLUMES         26
#define MAX_DISKS         26


typedef struct {
  int32_t	     serialnum;
  int32_t      drivetype;
  __uint64_t volsize;
  int          disknum;
  char        fstype[MAX_PATH+1];
  char        mount[MAX_PATH+1];
  char        rootpath[MAX_PATH+1];
  char        volume[MAX_PATH+1];
} vol_entry_t;

typedef struct {
  __uint64_t disksize;
  int          blocksize;
  int          disknum;
  int          volnum[MAX_VOLUMES+1];
  char        diskname[MAX_PATH+1];
} disk_entry_t;

class DiskWriter : public Protocol {
public:
  int diskcount;
  bool bPatchDisk;

  DiskWriter();
  ~DiskWriter();

  int WriteData(unsigned char *writeBuffer, int64_t writeOffset, uint32_t writeBytes, uint32_t *bytesWritten, uint8_t partNum);
  int ReadData(unsigned char *readBuffer, int64_t readOffset, uint32_t readBytes, uint32_t *bytesRead, uint8_t partNum);

  int FastCopy(int hRead, int64_t sectorRead, int hWrite, int64_t sectorWrite, __uint64_t sectors, uint8_t partNum=0);
  int OpenDevice(int dn);
  int OpenDiskFile(char *oFile, __uint64_t sectors);
  void CloseDevice();
  int InitDiskList(bool verbose = false);
  int DeviceReset(void);
  int ProgramPatchEntry(PartitionEntry pe, char *key);
  int ProgramRawCommand(char *key);

  // Functions for testing purposes
  int CorruptionTest(__uint64_t offset);
  int DiskTest(__uint64_t offset);
  int WipeLayout();

protected:

private:
  int hVolume;
  int disk_num;
  int hFS;
  //OVERLAPPED ovl;
  disk_entry_t *disks;
  vol_entry_t *volumes;

  int GetVolumeInfo(vol_entry_t *vol);
  int GetDiskInfo(disk_entry_t *de);
  char *TCharToString(char *p);
  int UnmountVolume(vol_entry_t vol);
  int LockDevice();
  int UnlockDevice();
  bool IsDeviceWriteable();
  int GetRawDiskSize(__uint64_t *ds);
  int RawReadTest(__uint64_t offset);
};
