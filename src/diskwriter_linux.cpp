/*****************************************************************************
 * diskwritter.cpp
 *
 * This file implements disk information and dumping files to disk
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/diskwriter.cpp#12 $
$DateTime: 2015/04/29 17:06:00 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
10/10/11   pgw     Keep volume open otherwise windows will remount it on us
06/28/11   pgw     Aligned all buffers to 128 bytes for ARM SDCC suport
05/18/11   pgw     Changed to __uint64_t use local handle rather than passing it in
                   updated performance tests and API's.
04/13/11   pgw     Fixed bug for empty card readers.
03/21/11   pgw     Initial version.
=============================================================================*/

#include "diskwriter.h"
#include <string.h>
#include <stdio.h>
#include "sahara.h"


DiskWriter::DiskWriter()
{
//  ovl.hEvent = CreateEvent(NULL, false, false,NULL);
  hVolume = -1;
  bPatchDisk = false;

  // Create some nice 128 byte aligned buffers required by ARM
  disks = NULL;
  volumes = NULL;
  gpt_entries = (gpt_entry_t*)malloc(sizeof(gpt_entry_t)*256);
  disks = (disk_entry_t*)malloc(sizeof(disk_entry_t)*MAX_DISKS);
  volumes = (vol_entry_t*)malloc(sizeof(vol_entry_t)*MAX_VOLUMES);
  
}

DiskWriter::~DiskWriter()
{
  if(disks) free(disks);
  if(volumes) free(volumes);
//  emmcdl_close(ovl.hEvent);
}

int DiskWriter::ProgramRawCommand(char *key)
{
  return EINVAL;
}

int DiskWriter::DeviceReset(void)
{
  return EINVAL;
}


int DiskWriter::GetVolumeInfo(vol_entry_t *vol)
{
 /* char mount[MAX_PATH+1];
  uint32_t mountsize;

  //try {
    // Clear out our mount path
    memset(mount,0,sizeof(mount));

    if( GetVolumePathNamesForVolumeName(vol->rootpath,mount,MAX_PATH,&mountsize) ) {
      uint32_t maxcomplen;
      uint32_t fsflags;
      strcpy(vol->mount, mount);
      // Chop of the last trailing char
      vol->mount[strlen(vol->mount)-1] = 0;
      vol->drivetype = GetDriveType(vol->mount);
      // We only fill out information for DRIVE_FIXED and DRIVE_REMOVABLE
      if( vol->drivetype == DRIVE_REMOVABLE || vol->drivetype == DRIVE_FIXED ) {
        GetVolumeInformation(vol->mount,vol->volume,MAX_PATH+1,(LPuint32_t)&vol->serialnum,&maxcomplen,&fsflags,vol->fstype,MAX_PATH);
        // Create handle to volume and get disk number
        char volPath[MAX_PATH+1] = _T("\\\\.\\");
        strcat(volPath,vol->mount);

        // Create the file using the volume name
        hDisk = emmcdl_creat( volPath,O_RDWR);
        if( hDisk != -1 ) {
          // Now get physical disk number
          _STORAGE_DEVICE_NUMBER devInfo;
          uint32_t bytesRead;
          DeviceIoControl( hDisk,IOCTL_STORAGE_GET_DEVICE_NUMBER,NULL,0,&devInfo,sizeof(devInfo),&bytesRead,NULL);
          vol->disknum = devInfo.DeviceNumber;
          emmcdl_close(hDisk);
        }

      } 
    }
  //} catch(exception &e) {
  //  ("Exception in GetVolumeInfo: %s\n", e.what());
  //}*/

  return 0;
}

int DiskWriter::GetDiskInfo(disk_entry_t *de)
{
 /* char tPath[MAX_PATH+1];
  
  int status = 0;

  // Create path to physical drive
  sw_s(tPath, _T("\\\\.\\PhysicalDrive%i"), de->disknum);
  // Create the file using the physical drive
  hDisk = emmcdl_creat(tPath, O_RDWR | O_EXCL | O_ASYNC);
  if (hDisk != -1) {
    DISK_GEOMETRY_EX info;
    uint32_t bytesRead;
    if (DeviceIoControl(hDisk,
      IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,    // dwIoControlCode
      NULL,                          // lpInBuffer
      0,                             // nInBufferSize
      &info,                         // output buffer
      sizeof(info),                  // size of output buffer
      &bytesRead,                    // number of bytes returned
      &ovl                           // OVERLAPPED structure
      ))
    {
      strcpy(de->diskname, tPath);
      de->disksize = *(__uint64_t *)(&info.DiskSize);
      de->blocksize = info.Geometry.BytesPerSector;
    }
    else {
      status = ERROR_NO_VOLUME_LABEL;
    }
    emmcdl_close(hDisk);
  }
  else {
    status = errno;
  }


  if (status != 0) {
    de->disknum = -1;
    de->disksize = (__uint64_t )-1;
    de->diskname[0] = 0;
    de->volnum[0] = -1;
  }
*/
  return 0;
}

int DiskWriter::InitDiskList(bool verbose)
{
/*
  int vHandle;
  char VolumeName[MAX_PATH+1];
  bool bValid = true;
  int i=0;
#ifndef ARM
  HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT,NULL,NULL,DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);
  DEVPROPTYPE ulPropertyType = DEVPROP_TYPE_STRING;
  uint32_t dwSize;
#endif //ARM

  if( disks == NULL || volumes == NULL ) {
    return EINVAL;
  }

  ("Finding all devices in emergency download mode...\n");

#ifndef ARM
  if( hDevInfo != -1 ) {
    // Successfully got a list of ports
    for(int i=0; ;i++) {
      char szBuffer[512];
      SP_DEVINFO_DATA DeviceInfoData;
      DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
      if( !SetupDiEnumDeviceInfo(hDevInfo,i,&DeviceInfoData) && (errno == ERROR_NO_MORE_ITEMS) ) {
        // No more ports
        break;
      }
      // successfully found entry print out the info
      if( SetupDiGetDeviceProperty(hDevInfo,&DeviceInfoData,&DEVPKEY_Device_FriendlyName,&ulPropertyType,(unsigned char*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {
        if( (errno == 0) && strstr(szBuffer,"QDLoader 9008") != NULL ) {
          w(szBuffer);
          // Get the serial number and display it if verbose is enabled
          if (verbose)
          {
            char *port = strstr(szBuffer, "COM");
            if (port != NULL) {
              SerialPort spTemp;
              pbl_info_t pbl_info;
              spTemp.Open(atoi((port + 3)));
              Sahara sh(&spTemp);
              int status = sh.DumpDeviceInfo(&pbl_info);
              if (status == 0) {
                (": SERIAL=0x%x : HW_ID=0x%x", pbl_info.serial, pbl_info.msm_id);
              }
            }
          }
          w(_T("\n"));
        }
      }
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);
  }  
#endif //ARM

  memset(volumes,0,sizeof(vol_entry_t)*MAX_VOLUMES);
  
  // Set all disks to invalid
  for(i=0; i < MAX_VOLUMES; i++) {
    volumes[i].disknum = -1;
  }

  ("\nFinding all disks on computer ...\n");
  // First loop through all the volumes

  vHandle = FindFirstVolume(VolumeName,MAX_PATH);
  for(i=0; bValid && (i < MAX_VOLUMES); i++) {
    strcpy(volumes[i].rootpath,VolumeName);
    GetVolumeInfo(&volumes[i]);
    bValid = FindNextVolume(vHandle,VolumeName,MAX_PATH);
  }

  // Now loop through all physical disks to find all the useful information
  for(i=0; i< MAX_DISKS; i++) {
    disks[i].disknum = i;
    // If we can't successfully get the information then just continue
    if( GetDiskInfo(&disks[i]) != 0) {
      continue;
    }
    // Find volume for this disk if it exists
    int v = 0;
    for(int j=0; j < MAX_VOLUMES; j++) {
      if( volumes[j].disknum == i ) {
        disks[i].volnum[v++] = j; 
      }
    }
    disks[i].volnum[v] = 0;

    // If disk info is valid print it out
    if( disks[i].disknum != -1 ) {
      ("%i. %s  Size: %I64dMB, (%I64d sectors), size: %i Mount:%s, Name:[%s]\n",i,disks[i].diskname, disks[i].disksize/(1024*1024),(disks[i].disksize/(disks[i].blocksize)), disks[i].blocksize, volumes[disks[i].volnum[0]].mount, volumes[disks[i].volnum[0]].volume);
    }
  }
*/

  
  return 0;
}

// Patch a file on computer and put it in %temp% directory
int DiskWriter::ProgramPatchEntry(PartitionEntry pe, char *key)
{
  uint32_t bytesIn, bytesOut;
  bool bPatchFile = false;
  int status = 0;

  // If filename is disk then patch after else patch the file
  if( (strcmp(pe.filename,"DISK") == 0) && bPatchDisk) {
    ("Patch file on disk\n");
  } else if( (strcmp(pe.filename,"DISK") != 0) && !bPatchDisk ) {
    // Copy file to local temp directory in case it is on share and patch there
    ("Patch file locally\n");
    hDisk = emmcdl_creat( pe.filename, O_RDWR | O_EXCL);
    if( hDisk == -1 ) {
      ("Failed to open file %s\n",pe.filename);
      return errno;
    }
    bPatchFile = true;
  } else {
    ("No file specified skipping command\n");
    return 0;
  }
    
  // If there is a CRC then calculate this value before we write it out
  if( pe.crc_size > 0 ) {
    if (ReadData(buffer1, pe.crc_start*DISK_SECTOR_SIZE, ((int)pe.crc_size + DISK_SECTOR_SIZE), &bytesIn, pe.physical_partition_number) == 0) {
      Partition p(1);
      pe.patch_value += p.CalcCRC32(buffer1,(int)pe.crc_size);
      ("Patch value 0x%x\n", (uint32_t)pe.patch_value );
    }
  }
  
  // read input
  if( (status = ReadData(buffer1,pe.start_sector*DISK_SECTOR_SIZE, DISK_SECTOR_SIZE, &bytesIn, 0)) == 0) {
    // Apply the patch
    memcpy(&(buffer1[pe.patch_offset]), &(pe.patch_value), (int)pe.patch_size);

    // Write it back out
    status = WriteData(buffer1,pe.start_sector*DISK_SECTOR_SIZE,DISK_SECTOR_SIZE, &bytesOut, 0);
  }

  // If hInFile is not disk file then close after patching it
  if( bPatchFile ) {
    emmcdl_close(hDisk);
  } 
  return status;
}

#define MAX_TEST_SIZE (4*1024*1024)
#define LOOP_COUNT 1000

int DiskWriter::CorruptionTest(__uint64_t offset)
{
  /*int status = 0;
  bool bWriteDone = false;
  bool bReadDone = false;
  uint32_t bytesOut = 0;
  uint64_t ticks;
  
  char *bufAlloc = NULL;
  char *temp1 = NULL;

  bufAlloc = new char[MAX_TEST_SIZE + 1024*1024];

  // Round off to 1MB boundary
  temp1 = (char *)(((uintptr_t)bufAlloc + 1024*1024) & ~0xFFFFF);
  ticks = GetTickCount64();
  ("OffsetLow is at 0x%x\n", (uint32_t)offset);
  ("OffsetHigh is at 0x%x\n", (uint32_t)(offset >> 32));
  ("temp1 = 0x%x\n",(uintptr_t)temp1);

  for(int i=0; i < MAX_TEST_SIZE; i++) {
    temp1[i] = (char)i;
  }

  for(int i=0; i < LOOP_COUNT; i++) {
    ovl.OffsetHigh = (offset >> 32);
    ovl.Offset = offset & 0xFFFFFFFF;
  
    bWriteDone = emmcdl_write(hDisk,&temp1[i],MAX_TEST_SIZE,NULL,&ovl);
    if( !bWriteDone ) status = errno;
    if( !bWriteDone && (status != 0) ) {
      ("status %i bWriteDone %i bytesWrite %i\n", status, (int)bWriteDone, (int)bytesOut);
      break;
    }

    ovl.Offset = offset & 0xFFFFFFFF;
    ovl.OffsetHigh = (offset >> 32);

    bReadDone = emmcdl_read(hDisk,&temp1[i],MAX_TEST_SIZE,NULL,&ovl);
    if( !bReadDone ) status = errno;
    if( !bReadDone && (status != 0) ) {
      ("status %i bReadDone %i bytesRead %i\n", status, (int)bReadDone, (int)bytesOut);
      break;
    }

    for(int i=0; i < MAX_TEST_SIZE; i++) {
      if( temp1[i] != (char)i ) {
        // If it is different dump out buffer
        ("Failed: Offset:  at %i expected %i found %i\n",i,(i&0xff),(int)temp1[i]);
      }
    }
    (".");
    offset += 0x40;
  }

  delete[] bufAlloc;*/
  return 0;
}

int DiskWriter::DiskTest(__uint64_t offset)
{
/*  int status = 0;
  bool bWriteDone = false;
  bool bReadDone = false;
  uint32_t bytesOut = 0;
  uint64_t ticks;
  uint32_t iops;

  char *bufAlloc = NULL;
  char *temp1 = NULL;
  ("Testing this stuff out\n");

  bufAlloc = new char[MAX_TEST_SIZE + 1024*1024];

  // Round off to 1MB boundary
  temp1 = (char *)(((uintptr_t)bufAlloc + 1024*1024) & ~0xFFFFF);
  
  ("Sequential write test 4MB buffer %i\n", (uintptr_t)temp1);
  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);
  
  ticks = GetTickCount64();
  for(int i=0; i < 50; i++) {
    bWriteDone = emmcdl_write(hDisk,temp1,MAX_TEST_SIZE,&bytesOut,&ovl);
    if( !bWriteDone ) status = errno;
    if( !bWriteDone && (status != 0) ) {
      ("status %i bWriteDone %i bytesOut %i\n", status, (int)bWriteDone, (int)bytesOut);
      break;
    }
  }
  ticks = GetTickCount64() - ticks;
  ("Sequential Write transfer rate: %i KB/s\n", (int)(50*MAX_TEST_SIZE/ticks));

  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);
  
  ("Random write test 4KB buffer\n");
  ticks = GetTickCount64();
  for(iops=0; (GetTickCount64() - ticks) < 2000; iops++) {
    bReadDone = emmcdl_write(hDisk,temp1,4*1024,NULL,&ovl);
    if( !bWriteDone ) status = errno;
    if( !bWriteDone && (status != 0) ) {
      ("status %i Offset: %i bWriteDone %i bytesWrite %i\n", status, (uint32_t)ovl.Offset, (int)bWriteDone, (int)bytesOut);
      break;
    }
    ovl.Offset += 0x1000;
  }
  ticks = GetTickCount64() - ticks;
  ("Random 4K write IOPS = %i\n", (int)(iops/2));
  
  ("Flush Buffer!\n");
  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);
  ticks = GetTickCount64();
  for(int i=0; i < 100; i++) {
    bReadDone = emmcdl_read(hDisk,temp1,MAX_TEST_SIZE,NULL,&ovl);
    if( !bReadDone ) status = errno;
    if( !bReadDone && (status != 0) ) {
      ("status %i bReadDone %i bytesRead %i\n", status, (int)bReadDone, (int)bytesOut);
      break;
    }
  }
  
  
  ("Sequential read test 4MB buffer %i\n", (uintptr_t)temp1);
  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);
  ticks = GetTickCount64();
  for(int i=0; i < 50; i++) {
    bReadDone = emmcdl_read(hDisk,temp1,MAX_TEST_SIZE,NULL,&ovl);
    if( !bReadDone ) status = errno;
    if( !bReadDone && (status != 0) ) {
      ("status %i bReadDone %i bytesRead %i\n", status, (int)bReadDone, (int)bytesOut);
      break;
    }
  }

  ticks = GetTickCount64() - ticks;
  ("Sequential Read transfer rate: %i KB/s in %i ms\n", (int)(50*MAX_TEST_SIZE/ticks), (int)ticks);
  
  ("Random read  test 4KB buffer\n");
  ticks = GetTickCount64();
  for(iops=0; (GetTickCount64() - ticks) < 2000; iops++) {
    bReadDone = emmcdl_read(hDisk,temp1,4*1024,NULL,&ovl);
    if( !bReadDone ) status = errno;
    if( !bReadDone && (status != 0) ) {
      ("status %i bReadDone %i bytesRead %i\n", status, (int)bReadDone, (int)bytesOut);
      break;
    }
    ovl.Offset += 0x100000;
  }
  ("Random 4K read IOPS = %i\n", (int)(iops/2));
  
  delete[] bufAlloc;
  return status;*/
	return 0;
}

int DiskWriter::WriteData(unsigned char *writeBuffer, int64_t writeOffset, uint32_t writeBytes, uint32_t *bytesWritten, uint8_t partNum)
{
/*  OVERLAPPED ovlWrite;
  bool bWriteDone;
  int status = 0;

  // Check inputs
  if (bytesWritten == NULL) {
    return EINVAL;
  }

  // If disk handle not opened then return error
  if ((hDisk == NULL) || (hDisk == -1)) {
    return EINVAL;
  }

  ovlWrite.hEvent = CreateEvent(NULL, false, false, NULL);
  if (ovlWrite.hEvent == NULL) {
    return EINVAL;
  }

  ResetEvent(ovlWrite.hEvent);
  ovlWrite.Offset = (uint32_t)(writeOffset);
  ovlWrite.OffsetHigh = ((writeOffset) >> 32);

  // Write the data to the disk/file at the given offset
  bWriteDone = emmcdl_write(hDisk, writeBuffer, writeBytes, bytesWritten, &ovlWrite);
  if (!bWriteDone) status = errno;
  
  // If not done and IO is pending wait for it to complete
  if (!bWriteDone && (status == ERROR_IO_PENDING)) {
    status = 0;
    bWriteDone = GetOverlappedResult(hDisk, &ovlWrite, bytesWritten, true);
    if (!bWriteDone) status = errno;
  }

  emmcdl_close(ovlWrite.hEvent);
  return status;*/
	return 0;
}

int DiskWriter::ReadData(unsigned char *readBuffer, int64_t readOffset, uint32_t readBytes, uint32_t *bytesRead, uint8_t partNum)
{
 /* OVERLAPPED ovlRead;
  bool bReadDone = true;
  int status = 0;

  // Check input parameters
  if (bytesRead == NULL) {
    return EINVAL;
  }

  // Make sure we have a valid handle to our disk/file
  if ((hDisk == NULL) || (hDisk == -1)) {
    return EINVAL;
  }

  ovlRead.hEvent = CreateEvent(NULL, false, false, NULL);
  if (ovlRead.hEvent == NULL) {
    return EINVAL;
  }

  ResetEvent(ovlRead.hEvent);
  ovlRead.Offset = (uint32_t)readOffset;
  ovlRead.OffsetHigh = (readOffset >> 32);

  bReadDone = emmcdl_read(hDisk, readBuffer, readBytes, bytesRead, &ovlRead);
  if (!bReadDone) status = errno;
  
  // If not done and IO is pending wait for it to complete
  if (!bReadDone && (status == ERROR_IO_PENDING)) {
    status = 0;
    bReadDone = GetOverlappedResult(hDisk, &ovlRead, bytesRead, true);
    if (!bReadDone) status = errno;
  }

  emmcdl_close(ovlRead.hEvent);
  return status;*/
	return 0;
}


int DiskWriter::UnmountVolume(vol_entry_t vol)
{
  uint32_t bytesRead = 0;

/*
  //try {
    char volPath[MAX_PATH+1] = _T("\\\\.\\");
    strcat(volPath,vol.mount);
    // Create the file using the volume name
    hVolume = emmcdl_creat( volPath, O_RDWR);
    if( hVolume != -1 ) {
      DeviceIoControl( hVolume,FSCTL_DISMOUNT_VOLUME,NULL,0,NULL,0,&bytesRead,NULL);
      ("Unmount drive success.\n");
    } else {
      ("Warning: Failed to unmount drive continuing...\n");
    }
  //} catch (exception &e) {
  //  ("Exception in LockDevice: %s\n", e.what());
  //}
*/
  return 0;
}

int DiskWriter::OpenDiskFile(char *oFile, __uint64_t sectors)
{
  int status = 0;
  if( oFile == NULL ) {
    return EINVAL;
  }

  // First try to create new if it doesn't exist
  hDisk = emmcdl_creat( oFile,O_RDWR | O_CREAT | O_ASYNC);
  // If file  
  if( hDisk == -1 ) {
    status = errno;
    // apply on top of existing file.
    if( status == EEXIST ) {
      status = 0;
      hDisk = emmcdl_creat( oFile,O_RDWR | O_EXCL | O_ASYNC);
     }
    if( hDisk == -1 ) {
      status = errno;
    }
  }
  disk_size = sectors*DISK_SECTOR_SIZE;
  return status;
}

int DiskWriter::OpenDevice(int dn)
{
/*  // Make sure our parameters are okay
  
  disk_entry_t de = disks[dn];
  // Lock all volumes associated with this physical disk
  if (de.volnum[0] != 0) {
    if (UnmountVolume(volumes[de.volnum[0]]) != 0) {
      ("Warning: Failed to unmount volume %s\n", volumes[de.volnum[0]].mount);
      //return EINVAL;
    }
  }

  // All associated volumes have been unlocked so now open handle to physical disk
  char tPath[MAX_PATH];
  sw_s(tPath, _T("\\\\.\\PhysicalDrive%i"), dn);
  w(tPath);
  // Create the file using the volume name
  hDisk = emmcdl_creat(tPath, O_RDWR | O_EXCL);

  disk_num = dn;
  disk_size = disks[dn].disksize;*/
  return 0;
}

void DiskWriter::CloseDevice()
{
  disk_num = -1;
  if(hDisk != -1 ) emmcdl_close(hDisk);
  if(hVolume != -1 ) emmcdl_close(hVolume);
}

bool DiskWriter::IsDeviceWriteable()
{
/*  uint32_t bytesRead;
    if( DeviceIoControl( hDisk,IOCTL_DISK_IS_WRITABLE,NULL,0,NULL,0,&bytesRead,NULL) ) {
      return true;
    }*/
  return false;
}

int DiskWriter::WipeLayout()
{
/*  uint32_t bytesRead;
    if( DeviceIoControl( hDisk,IOCTL_DISK_DELETE_DRIVE_LAYOUT,NULL,0,NULL,0,&bytesRead,&ovl) ) {

      if( DeviceIoControl( hDisk,IOCTL_DISK_UPDATE_PROPERTIES,NULL,0,NULL,0,&bytesRead,&ovl) ) {
        return 0;
      }
    }
  return ERROR_GEN_FAILURE;*/
	return 0;
}

int DiskWriter::RawReadTest(__uint64_t offset)
{
 /* // Set up the overlapped structure
  OVERLAPPED ovlp;
  int status = 0;
  LARGE_INTEGER large_val;
  large_val.QuadPart = offset;
  ovlp.hEvent = CreateEvent(NULL, false, false,_T("DiskWriter"));
  if (!ovlp.hEvent) return errno;
  ovlp.Offset = large_val.LowPart;
  ovlp.OffsetHigh = large_val.HighPart;

  uint32_t bytesIn = 0; 
  uint32_t dwResult;
  bool bResult = emmcdl_read(hDisk, buffer1, DISK_SECTOR_SIZE, NULL, &ovlp);
  dwResult = errno;
  if( !bResult ) {
    // If Io is pending wait for it to finish
    if( dwResult == ERROR_IO_PENDING ) {
      // Wait for actual data to be read if pending
      ("Wait for it to finish %I64d\n", offset);
      bResult = GetOverlappedResult(hDisk,&ovlp,&bytesIn,true);
      if( !bResult ) {
        dwResult = errno;
        if( dwResult != ERROR_IO_INCOMPLETE) {
          status = ERROR_READ_FAULT;
        }
      } else {
        // Finished read successfully
        status = 0;
      }
    } else if( dwResult != 0 ) {
      ("Status is: %i\n",(int)dwResult);
      status = ERROR_READ_FAULT;
    }
  }

  emmcdl_close(ovlp.hEvent);
  return status;*/
	return 0;
}

int DiskWriter::FastCopy(int hRead, int64_t sectorRead, int hWrite, int64_t sectorWrite, __uint64_t sectors, uint8_t partNum)
{  // Set up the overlapped structure
 /* OVERLAPPED ovlWrite, ovlRead;
  int stride;
  __uint64_t sec;
  uint32_t bytesOut = 0;
  uint32_t bytesRead = 0;
  int readStatus = 0;
  int status = 0;
  bool bWriteDone = true;
  bool bBuffer1 = true;

  if (sectorWrite < 0)
  {
    sectorWrite = GetNumDiskSectors() + sectorWrite;
  }

  // Set initial stride size to size of buffer
  stride = MAX_TRANSFER_SIZE / DISK_SECTOR_SIZE;

  // Setup offsets for read and write
  ovlWrite.hEvent = CreateEvent(NULL, false, false, NULL);
  if (ovlWrite.hEvent == NULL) return ENOMEM;
  ovlWrite.Offset = (uint32_t)(sectorWrite*DISK_SECTOR_SIZE);
  ovlWrite.OffsetHigh = ((sectorWrite*DISK_SECTOR_SIZE) >> 32);

  ovlRead.hEvent = CreateEvent(NULL, false, false, NULL);
  if (ovlRead.hEvent == NULL) return ENOMEM;
  ovlRead.Offset = (uint32_t)(sectorRead*DISK_SECTOR_SIZE);
  ovlRead.OffsetHigh = ((sectorRead*DISK_SECTOR_SIZE) >> 32);

  if (hRead == -1) {
    ("hRead = -1, zeroing input buffer\n");
    memset(buffer1, 0, stride*DISK_SECTOR_SIZE);
    memset(buffer2, 0, stride*DISK_SECTOR_SIZE);
    bytesRead = stride*DISK_SECTOR_SIZE;
  }


  sec = 0;
  while (sec < sectors) {

    // Check if we have to read smaller number of sectors
    if ((sec + stride > sectors) && (sectors != 0)) {
      stride = (uint32_t)(sectors - sec);
    }

    // If read handle is valid then read file file and wait for response
    if (hRead != -1) {
      bytesRead = 0;
      if(bBuffer1 ) readStatus = emmcdl_read(hRead, buffer1, stride*DISK_SECTOR_SIZE, &bytesRead, &ovlRead);
      else readStatus = emmcdl_read(hRead, buffer2, stride*DISK_SECTOR_SIZE, &bytesRead, &ovlRead);
      if (!readStatus) status = errno;
      if (status == ERROR_IO_PENDING) readStatus = GetOverlappedResult(hRead, &ovlRead, &bytesRead, true);
      if (!readStatus) {
        status = errno;
        break;
      }
      status = 0;

      // Need to round up to nearest sector size if read partial sector from input file
      bytesRead = (bytesRead + DISK_SECTOR_SIZE - 1) & ~(DISK_SECTOR_SIZE - 1);
    }

    ovlRead.Offset += bytesRead;
    if (ovlRead.Offset < bytesRead) ovlRead.OffsetHigh++;

    if (!bWriteDone) {
      // Wait for the previous write to complete before we start a new one
      bWriteDone = GetOverlappedResult(hWrite, &ovlWrite, &bytesOut, true);
      if (!bWriteDone) {
        status = errno;
        break;
      }
      status = 0;
    }
    
    // Now start a write for the corresponding buffer we just read
    if (bBuffer1) bWriteDone = emmcdl_write(hWrite, buffer1, bytesRead, NULL, &ovlWrite);
    else bWriteDone = emmcdl_write(hWrite, buffer2, bytesRead, NULL, &ovlWrite);

    bBuffer1 = !bBuffer1;
    sec += stride;
    sectorRead += stride;

    ovlWrite.Offset += bytesRead;
    if (ovlWrite.Offset < bytesOut) ovlWrite.OffsetHigh++;

    ("Sectors remaining: %i      \r", (int)(sectors - sec));
  }
  ("\nStatus = %i\n",status);

  // If we hit end of file and we read some data then round up to nearest block and write it out and wait
  // for it to complete else the next operation might fail.
  if ((sec < sectors) && (bytesRead > 0) && (status == 0)) {
    bytesRead = (bytesRead + DISK_SECTOR_SIZE - 1) & ~(DISK_SECTOR_SIZE - 1);
    if (bBuffer1) bWriteDone = emmcdl_write(hWrite, buffer1, bytesRead, NULL, &ovlWrite);
    else  bWriteDone = emmcdl_write(hWrite, buffer2, bytesRead, NULL, &ovlWrite);
    ("Wrote last bytes: %i\n", (int)bytesRead);
  }

  // Wait for pending write transfers to complete
  if (!bWriteDone && (errno == ERROR_IO_PENDING)) {
    bWriteDone = GetOverlappedResult(hWrite, &ovlWrite, &bytesOut, true);
    if (!bWriteDone) status = errno;
  }

  emmcdl_close(ovlRead.hEvent);
  emmcdl_close(ovlWrite.hEvent);
  return status;*/
	return 0;
}

int DiskWriter::GetRawDiskSize( __uint64_t *ds)
{
 /* int status = 0;
  // Start at 512 MB for good measure to get us to size quickly
  __uint64_t diff = DISK_SECTOR_SIZE * 1024;

  // Read data from various sectors till we figure out how big disk is
  if( ds == NULL || hDisk == -1) {
    return EINVAL;
  }

  // Keep doubling size till we can't read anymore
  *ds = 0;
  for(;;) {
    status = RawReadTest(*ds + diff);
    if (diff <= DISK_SECTOR_SIZE) {
      if( status == 0) {
        *ds = *ds + diff;
      }
      break;
    }
    if( status == 0) {
      *ds = *ds + diff;
      diff = diff * 2;
    } else {
      diff = diff / 2;
    }
  }
  ("Value of ds is %I64d\n",*ds);*/

  return 0;
}

int DiskWriter::LockDevice()
{
/*  uint32_t bytesRead = 0;
	  if( DeviceIoControl( hDisk,FSCTL_DISMOUNT_VOLUME,NULL,0,NULL,0,&bytesRead,NULL) ) {
      if( DeviceIoControl( hDisk,FSCTL_LOCK_VOLUME,NULL,0,NULL,0,&bytesRead,NULL) ) {
        ("Locked volume and dismounted\n");
        return 0;
	    }
    }
  return ERROR_GEN_FAILURE;*/
  return 0;
}

int DiskWriter::UnlockDevice()
{
/*  uint32_t bytesRead = 0;
    if( DeviceIoControl( hDisk,FSCTL_UNLOCK_VOLUME,NULL,0,NULL,0,&bytesRead,NULL) ) {
      return 0;
    }
  return ERROR_GEN_FAILURE;*/
	return 0;
}
