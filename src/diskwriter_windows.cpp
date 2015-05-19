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
#ifdef _WIN32
#include "winerror.h"
#include <winioctl.h>
#include "tchar.h"
#include "windows.h"
// Only List COM port information for desktop version
#ifndef ARM
#include "setupapi.h"
#define INITGUID 1    // This is needed to properly define the GUID's in devpkey.h
#include "devpkey.h"
#endif //ARM
#endif
#include "sahara.h"


DiskWriter::DiskWriter()
{
  ovl.hEvent = CreateEvent(NULL, FALSE, FALSE,NULL);
  hVolume = INVALID_HANDLE_VALUE;
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
  CloseHandle(ovl.hEvent);
}

int DiskWriter::ProgramRawCommand(char *key)
{
  UNREFERENCED_PARAMETER(key);
  return ERROR_INVALID_FUNCTION;
}

int DiskWriter::DeviceReset(void)
{
  return ERROR_INVALID_FUNCTION;
}


int DiskWriter::GetVolumeInfo(vol_entry_t *vol)
{
  char mount[MAX_PATH+1];
  uint32_t mountsize;

  //try {
    // Clear out our mount path
    memset(mount,0,sizeof(mount));

    if( GetVolumePathNamesForVolumeName(vol->rootpath,mount,MAX_PATH,&mountsize) ) {
      uint32_t maxcomplen;
      uint32_t fsflags;
      wcscpy_s(vol->mount, mount);
      // Chop of the last trailing char
      vol->mount[wcslen(vol->mount)-1] = 0; 
      vol->drivetype = GetDriveType(vol->mount);
      // We only fill out information for DRIVE_FIXED and DRIVE_REMOVABLE
      if( vol->drivetype == DRIVE_REMOVABLE || vol->drivetype == DRIVE_FIXED ) {
        GetVolumeInformation(vol->mount,vol->volume,MAX_PATH+1,(LPuint32_t)&vol->serialnum,&maxcomplen,&fsflags,vol->fstype,MAX_PATH);
        // Create handle to volume and get disk number
        char volPath[MAX_PATH+1] = _T("\\\\.\\");
        wcscat_s(volPath,vol->mount);

        // Create the file using the volume name
        hDisk = CreateFile( volPath,
                          GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          NULL,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);
        if( hDisk != INVALID_HANDLE_VALUE ) {
          // Now get physical disk number
          _STORAGE_DEVICE_NUMBER devInfo;
          uint32_t bytesRead;
          DeviceIoControl( hDisk,IOCTL_STORAGE_GET_DEVICE_NUMBER,NULL,0,&devInfo,sizeof(devInfo),&bytesRead,NULL);
          vol->disknum = devInfo.DeviceNumber;
          CloseHandle(hDisk);
        }

      } 
    }
  //} catch(exception &e) {
  //  wprintf(L"Exception in GetVolumeInfo: %s\n", e.what());
  //}

  return ERROR_SUCCESS;
}

int DiskWriter::GetDiskInfo(disk_entry_t *de)
{
  char tPath[MAX_PATH+1];
  
  int status = ERROR_SUCCESS;

  // Create path to physical drive
  swprintf_s(tPath, _T("\\\\.\\PhysicalDrive%i"), de->disknum);
  // Create the file using the physical drive
  hDisk = CreateFile(tPath,
    GENERIC_READ,
    (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED,
    NULL);
  if (hDisk != INVALID_HANDLE_VALUE) {
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
      wcscpy_s(de->diskname, MAX_PATH, tPath);
      de->disksize = *(__uint64_t *)(&info.DiskSize);
      de->blocksize = info.Geometry.BytesPerSector;
    }
    else {
      status = ERROR_NO_VOLUME_LABEL;
    }
    CloseHandle(hDisk);
  }
  else {
    status = GetLastError();
  }


  if (status != ERROR_SUCCESS) {
    de->disknum = -1;
    de->disksize = (__uint64_t )-1;
    de->diskname[0] = 0;
    de->volnum[0] = -1;
  }

  return ERROR_SUCCESS;
}

int DiskWriter::InitDiskList(bool verbose)
{
  HANDLE vHandle;
  char VolumeName[MAX_PATH+1];
  BOOL bValid = true;
  int i=0;
#ifndef ARM
  HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT,NULL,NULL,DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);
  DEVPROPTYPE ulPropertyType = DEVPROP_TYPE_STRING;
  uint32_t dwSize;
#endif //ARM

  if( disks == NULL || volumes == NULL ) {
    return ERROR_INVALID_PARAMETER;
  }

  wprintf(L"Finding all devices in emergency download mode...\n");

#ifndef ARM
  if( hDevInfo != INVALID_HANDLE_VALUE ) {
    // Successfully got a list of ports
    for(int i=0; ;i++) {
      WCHAR szBuffer[512];
      SP_DEVINFO_DATA DeviceInfoData;
      DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
      if( !SetupDiEnumDeviceInfo(hDevInfo,i,&DeviceInfoData) && (GetLastError() == ERROR_NO_MORE_ITEMS) ) {
        // No more ports
        break;
      }
      // successfully found entry print out the info
      if( SetupDiGetDeviceProperty(hDevInfo,&DeviceInfoData,&DEVPKEY_Device_FriendlyName,&ulPropertyType,(unsigned char*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {
        if( (GetLastError() == ERROR_SUCCESS) && wcsstr(szBuffer,L"QDLoader 9008") != NULL ) {
          wprintf(szBuffer);
          // Get the serial number and display it if verbose is enabled
          if (verbose)
          {
            WCHAR *port = wcsstr(szBuffer, L"COM");
            if (port != NULL) {
              SerialPort spTemp;
              pbl_info_t pbl_info;
              spTemp.Open(_wtoi((port + 3)));
              Sahara sh(&spTemp);
              int status = sh.DumpDeviceInfo(&pbl_info);
              if (status == ERROR_SUCCESS) {
                wprintf(L": SERIAL=0x%x : HW_ID=0x%x", pbl_info.serial, pbl_info.msm_id);
              }
            }
          }
          wprintf(_T("\n"));
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

  wprintf(L"\nFinding all disks on computer ...\n");
  // First loop through all the volumes

  vHandle = FindFirstVolume(VolumeName,MAX_PATH);
  for(i=0; bValid && (i < MAX_VOLUMES); i++) {
    wcscpy_s(volumes[i].rootpath,MAX_PATH,VolumeName);
    GetVolumeInfo(&volumes[i]);
    bValid = FindNextVolume(vHandle,VolumeName,MAX_PATH);
  }

  // Now loop through all physical disks to find all the useful information
  for(i=0; i< MAX_DISKS; i++) {
    disks[i].disknum = i;
    // If we can't successfully get the information then just continue
    if( GetDiskInfo(&disks[i]) != ERROR_SUCCESS) {
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
      wprintf(L"%i. %s  Size: %I64dMB, (%I64d sectors), size: %i Mount:%s, Name:[%s]\n",i,disks[i].diskname, disks[i].disksize/(1024*1024),(disks[i].disksize/(disks[i].blocksize)), disks[i].blocksize, volumes[disks[i].volnum[0]].mount, volumes[disks[i].volnum[0]].volume);
    }
  }

  
  return ERROR_SUCCESS;
}

// Patch a file on computer and put it in %temp% directory
int DiskWriter::ProgramPatchEntry(PartitionEntry pe, char *key)
{
  UNREFERENCED_PARAMETER(key);
  uint32_t bytesIn, bytesOut;
  BOOL bPatchFile = FALSE;
  int status = ERROR_SUCCESS;

  // If filename is disk then patch after else patch the file
  if( (wcscmp(pe.filename,L"DISK") == 0) && bPatchDisk) {
    wprintf(L"Patch file on disk\n");
  } else if( (wcscmp(pe.filename,L"DISK") != 0) && !bPatchDisk ) {
    // Copy file to local temp directory in case it is on share and patch there
    wprintf(L"Patch file locally\n");
    hDisk = CreateFile( pe.filename,
                          (GENERIC_READ | GENERIC_WRITE),
                          0,
                          NULL,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);
    if( hDisk == INVALID_HANDLE_VALUE ) {
      wprintf(L"Failed to open file %s\n",pe.filename);
      return GetLastError();
    }
    bPatchFile = TRUE;
  } else {
    wprintf(L"No file specified skipping command\n");
    return ERROR_SUCCESS;
  }
    
  // If there is a CRC then calculate this value before we write it out
  if( pe.crc_size > 0 ) {
    if (ReadData(buffer1, pe.crc_start*DISK_SECTOR_SIZE, ((int)pe.crc_size + DISK_SECTOR_SIZE), &bytesIn, pe.physical_partition_number) == ERROR_SUCCESS) {
      Partition p(1);
      pe.patch_value += p.CalcCRC32(buffer1,(int)pe.crc_size);
      printf("Patch value 0x%x\n", (UINT32)pe.patch_value );
    }
  }
  
  // read input
  if( (status = ReadData(buffer1,pe.start_sector*DISK_SECTOR_SIZE, DISK_SECTOR_SIZE, &bytesIn, 0)) == ERROR_SUCCESS) {
    // Apply the patch
    memcpy_s(&(buffer1[pe.patch_offset]), (int)(MAX_TRANSFER_SIZE-pe.patch_offset), &(pe.patch_value), (int)pe.patch_size);

    // Write it back out
    status = WriteData(buffer1,pe.start_sector*DISK_SECTOR_SIZE,DISK_SECTOR_SIZE, &bytesOut, 0);
  }

  // If hInFile is not disk file then close after patching it
  if( bPatchFile ) {
    CloseHandle(hDisk);
  } 
  return status;
}

#define MAX_TEST_SIZE (4*1024*1024)
#define LOOP_COUNT 1000

int DiskWriter::CorruptionTest(__uint64_t offset)
{
  int status = ERROR_SUCCESS;
  BOOL bWriteDone = FALSE;
  BOOL bReadDone = FALSE;
  uint32_t bytesOut = 0;
  UINT64 ticks;
  
  char *bufAlloc = NULL;
  char *temp1 = NULL;

  bufAlloc = new char[MAX_TEST_SIZE + 1024*1024];

  // Round off to 1MB boundary
  temp1 = (char *)(((uint32_t)bufAlloc + 1024*1024) & ~0xFFFFF);
  ticks = GetTickCount64();
  wprintf(L"OffsetLow is at 0x%x\n", (UINT32)offset);
  wprintf(L"OffsetHigh is at 0x%x\n", (UINT32)(offset >> 32));
  wprintf(L"temp1 = 0x%x\n",(UINT32)temp1);

  for(int i=0; i < MAX_TEST_SIZE; i++) {
    temp1[i] = (char)i;
  }

  for(int i=0; i < LOOP_COUNT; i++) {
    ovl.OffsetHigh = (offset >> 32);
    ovl.Offset = offset & 0xFFFFFFFF;
  
    bWriteDone = WriteFile(hDisk,&temp1[i],MAX_TEST_SIZE,NULL,&ovl);
    if( !bWriteDone ) status = GetLastError();
    if( !bWriteDone && (status != ERROR_SUCCESS) ) {
      wprintf(L"status %i bWriteDone %i bytesWrite %i\n", status, (int)bWriteDone, (int)bytesOut);
      break;
    }

    ovl.Offset = offset & 0xFFFFFFFF;
    ovl.OffsetHigh = (offset >> 32);

    bReadDone = ReadFile(hDisk,&temp1[i],MAX_TEST_SIZE,NULL,&ovl);
    if( !bReadDone ) status = GetLastError();
    if( !bReadDone && (status != ERROR_SUCCESS) ) {
      wprintf(L"status %i bReadDone %i bytesRead %i\n", status, (int)bReadDone, (int)bytesOut);
      break;
    }

    for(int i=0; i < MAX_TEST_SIZE; i++) {
      if( temp1[i] != (char)i ) {
        // If it is different dump out buffer
        wprintf(L"Failed: Offset:  at %i expected %i found %i\n",i,(i&0xff),(int)temp1[i]);
      }
    }
    wprintf(L".");
    offset += 0x40;
  }

  delete[] bufAlloc;
  return ERROR_SUCCESS;
}

int DiskWriter::DiskTest(__uint64_t offset)
{
  int status = ERROR_SUCCESS;
  BOOL bWriteDone = FALSE;
  BOOL bReadDone = FALSE;
  uint32_t bytesOut = 0;
  UINT64 ticks;
  uint32_t iops;

  char *bufAlloc = NULL;
  char *temp1 = NULL;
  wprintf(L"Testing this stuff out\n");

  bufAlloc = new char[MAX_TEST_SIZE + 1024*1024];

  // Round off to 1MB boundary
  temp1 = (char *)(((uint32_t)bufAlloc + 1024*1024) & ~0xFFFFF);
  
  wprintf(L"Sequential write test 4MB buffer %i\n", (int)temp1);
  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);
  
  ticks = GetTickCount64();
  for(int i=0; i < 50; i++) {
    bWriteDone = WriteFile(hDisk,temp1,MAX_TEST_SIZE,&bytesOut,&ovl);
    if( !bWriteDone ) status = GetLastError();
    if( !bWriteDone && (status != ERROR_SUCCESS) ) {
      wprintf(L"status %i bWriteDone %i bytesOut %i\n", status, (int)bWriteDone, (int)bytesOut);
      break;
    }
  }
  ticks = GetTickCount64() - ticks;
  wprintf(L"Sequential Write transfer rate: %i KB/s\n", (int)(50*MAX_TEST_SIZE/ticks));

  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);
  
  wprintf(L"Random write test 4KB buffer\n");
  ticks = GetTickCount64();
  for(iops=0; (GetTickCount64() - ticks) < 2000; iops++) {
    bReadDone = WriteFile(hDisk,temp1,4*1024,NULL,&ovl);
    if( !bWriteDone ) status = GetLastError();
    if( !bWriteDone && (status != ERROR_SUCCESS) ) {
      wprintf(L"status %i Offset: %i bWriteDone %i bytesWrite %i\n", status, (UINT32)ovl.Offset, (int)bWriteDone, (int)bytesOut);
      break;
    }
    ovl.Offset += 0x1000;
  }
  ticks = GetTickCount64() - ticks;
  wprintf(L"Random 4K write IOPS = %i\n", (int)(iops/2));
  
  wprintf(L"Flush Buffer!\n");
  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);
  ticks = GetTickCount64();
  for(int i=0; i < 100; i++) {
    bReadDone = ReadFile(hDisk,temp1,MAX_TEST_SIZE,NULL,&ovl);
    if( !bReadDone ) status = GetLastError();
    if( !bReadDone && (status != ERROR_SUCCESS) ) {
      wprintf(L"status %i bReadDone %i bytesRead %i\n", status, (int)bReadDone, (int)bytesOut);
      break;
    }
  }
  
  
  wprintf(L"Sequential read test 4MB buffer %i\n", (int)temp1);
  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);
  ticks = GetTickCount64();
  for(int i=0; i < 50; i++) {
    bReadDone = ReadFile(hDisk,temp1,MAX_TEST_SIZE,NULL,&ovl);
    if( !bReadDone ) status = GetLastError();
    if( !bReadDone && (status != ERROR_SUCCESS) ) {
      wprintf(L"status %i bReadDone %i bytesRead %i\n", status, (int)bReadDone, (int)bytesOut);
      break;
    }
  }

  ticks = GetTickCount64() - ticks;
  wprintf(L"Sequential Read transfer rate: %i KB/s in %i ms\n", (int)(50*MAX_TEST_SIZE/ticks), (int)ticks);
  
  wprintf(L"Random read  test 4KB buffer\n");
  ticks = GetTickCount64();
  for(iops=0; (GetTickCount64() - ticks) < 2000; iops++) {
    bReadDone = ReadFile(hDisk,temp1,4*1024,NULL,&ovl);
    if( !bReadDone ) status = GetLastError();
    if( !bReadDone && (status != ERROR_SUCCESS) ) {
      wprintf(L"status %i bReadDone %i bytesRead %i\n", status, (int)bReadDone, (int)bytesOut);
      break;
    }
    ovl.Offset += 0x100000;
  }
  wprintf(L"Random 4K read IOPS = %i\n", (int)(iops/2));
  
  delete[] bufAlloc;
  return status;
}

int DiskWriter::WriteData(unsigned char *writeBuffer, int64_t writeOffset, uint32_t writeBytes, uint32_t *bytesWritten, UINT8 partNum)
{
  UNREFERENCED_PARAMETER(partNum);
  OVERLAPPED ovlWrite;
  BOOL bWriteDone;
  int status = ERROR_SUCCESS;

  // Check inputs
  if (bytesWritten == NULL) {
    return ERROR_INVALID_PARAMETER;
  }

  // If disk handle not opened then return error
  if ((hDisk == NULL) || (hDisk == INVALID_HANDLE_VALUE)) {
    return ERROR_INVALID_HANDLE;
  }

  ovlWrite.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (ovlWrite.hEvent == NULL) {
    return ERROR_INVALID_HANDLE;
  }

  ResetEvent(ovlWrite.hEvent);
  ovlWrite.Offset = (uint32_t)(writeOffset);
  ovlWrite.OffsetHigh = ((writeOffset) >> 32);

  // Write the data to the disk/file at the given offset
  bWriteDone = WriteFile(hDisk, writeBuffer, writeBytes, bytesWritten, &ovlWrite);
  if (!bWriteDone) status = GetLastError();
  
  // If not done and IO is pending wait for it to complete
  if (!bWriteDone && (status == ERROR_IO_PENDING)) {
    status = ERROR_SUCCESS;
    bWriteDone = GetOverlappedResult(hDisk, &ovlWrite, bytesWritten, TRUE);
    if (!bWriteDone) status = GetLastError();
  }

  CloseHandle(ovlWrite.hEvent);
  return status;
}

int DiskWriter::ReadData(unsigned char *readBuffer, int64_t readOffset, uint32_t readBytes, uint32_t *bytesRead, UINT8 partNum)
{
  UNREFERENCED_PARAMETER(partNum);
  OVERLAPPED ovlRead;
  BOOL bReadDone = TRUE;
  int status = ERROR_SUCCESS;

  // Check input parameters
  if (bytesRead == NULL) {
    return ERROR_INVALID_PARAMETER;
  }

  // Make sure we have a valid handle to our disk/file
  if ((hDisk == NULL) || (hDisk == INVALID_HANDLE_VALUE)) {
    return ERROR_INVALID_HANDLE;
  }

  ovlRead.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (ovlRead.hEvent == NULL) {
    return ERROR_INVALID_HANDLE;
  }

  ResetEvent(ovlRead.hEvent);
  ovlRead.Offset = (uint32_t)readOffset;
  ovlRead.OffsetHigh = (readOffset >> 32);

  bReadDone = ReadFile(hDisk, readBuffer, readBytes, bytesRead, &ovlRead);
  if (!bReadDone) status = GetLastError();
  
  // If not done and IO is pending wait for it to complete
  if (!bReadDone && (status == ERROR_IO_PENDING)) {
    status = ERROR_SUCCESS;
    bReadDone = GetOverlappedResult(hDisk, &ovlRead, bytesRead, TRUE);
    if (!bReadDone) status = GetLastError();
  }

  CloseHandle(ovlRead.hEvent);
  return status;
}


int DiskWriter::UnmountVolume(vol_entry_t vol)
{
  uint32_t bytesRead = 0;

  //try {
    char volPath[MAX_PATH+1] = _T("\\\\.\\");
    wcscat_s(volPath,vol.mount);
    // Create the file using the volume name
    hVolume = CreateFile( volPath,
                        (GENERIC_READ | GENERIC_WRITE),
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

    if( hVolume != INVALID_HANDLE_VALUE ) {
      DeviceIoControl( hVolume,FSCTL_DISMOUNT_VOLUME,NULL,0,NULL,0,&bytesRead,NULL);
      wprintf(L"Unmount drive success.\n");
    } else {
      wprintf(L"Warning: Failed to unmount drive continuing...\n");
    }
  //} catch (exception &e) {
  //  wprintf(L"Exception in LockDevice: %s\n", e.what());
  //}
  return ERROR_SUCCESS;
}

int DiskWriter::OpenDiskFile(char *oFile, __uint64_t sectors)
{
  int status = ERROR_SUCCESS;
  if( oFile == NULL ) {
    return ERROR_INVALID_HANDLE;
  }

  // First try to create new if it doesn't exist
  hDisk = CreateFile( oFile,
                      GENERIC_WRITE | GENERIC_READ,
                      0,    // We want exclusive access to this disk
                      NULL,
                      CREATE_NEW,
                      FILE_FLAG_OVERLAPPED,
                      NULL);
  // If file  
  if( hDisk == INVALID_HANDLE_VALUE ) {
    status = GetLastError();
    // apply on top of existing file.
    if( status == ERROR_FILE_EXISTS ) {
      status = ERROR_SUCCESS;
      hDisk = CreateFile( oFile,
                        GENERIC_WRITE | GENERIC_READ,
                        0,    // We want exclusive access to this disk
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED,
                        NULL);
    }
    if( hDisk == INVALID_HANDLE_VALUE ) {
      status = GetLastError();
    }
  }
  disk_size = sectors*DISK_SECTOR_SIZE;
  return status;
}

int DiskWriter::OpenDevice(int dn)
{
  // Make sure our parameters are okay
  
  disk_entry_t de = disks[dn];
  // Lock all volumes associated with this physical disk
  if (de.volnum[0] != 0) {
    if (UnmountVolume(volumes[de.volnum[0]]) != ERROR_SUCCESS) {
      wprintf(L"Warning: Failed to unmount volume %s\n", volumes[de.volnum[0]].mount);
      //return ERROR_INVALID_HANDLE;
    }
  }

  // All associated volumes have been unlocked so now open handle to physical disk
  char tPath[MAX_PATH];
  swprintf_s(tPath, _T("\\\\.\\PhysicalDrive%i"), dn);
  wprintf(tPath);
  // Create the file using the volume name
  hDisk = CreateFile(tPath,
    GENERIC_WRITE | GENERIC_READ,
    (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),    // open it this way so we can write to any disk...
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
    NULL);

  disk_num = dn;
  disk_size = disks[dn].disksize;
  return ERROR_SUCCESS;
}

void DiskWriter::CloseDevice()
{
  disk_num = -1;
  if(hDisk != INVALID_HANDLE_VALUE ) CloseHandle(hDisk);
  if(hVolume != INVALID_HANDLE_VALUE ) CloseHandle(hVolume);
}

bool DiskWriter::IsDeviceWriteable()
{
  uint32_t bytesRead;
    if( DeviceIoControl( hDisk,IOCTL_DISK_IS_WRITABLE,NULL,0,NULL,0,&bytesRead,NULL) ) {
      return true;
    }
  return false;
}

int DiskWriter::WipeLayout()
{
  uint32_t bytesRead;
    if( DeviceIoControl( hDisk,IOCTL_DISK_DELETE_DRIVE_LAYOUT,NULL,0,NULL,0,&bytesRead,&ovl) ) {

      if( DeviceIoControl( hDisk,IOCTL_DISK_UPDATE_PROPERTIES,NULL,0,NULL,0,&bytesRead,&ovl) ) {
        return ERROR_SUCCESS;
      }
    }
  return ERROR_GEN_FAILURE;
}

int DiskWriter::RawReadTest(__uint64_t offset)
{
  // Set up the overlapped structure
  OVERLAPPED ovlp;
  int status = ERROR_SUCCESS;
  LARGE_INTEGER large_val;
  large_val.QuadPart = offset;
  ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE,_T("DiskWriter"));
  if (!ovlp.hEvent) return GetLastError();
  ovlp.Offset = large_val.LowPart;
  ovlp.OffsetHigh = large_val.HighPart;

  uint32_t bytesIn = 0; 
  uint32_t dwResult;
  BOOL bResult = ReadFile(hDisk, buffer1, DISK_SECTOR_SIZE, NULL, &ovlp);
  dwResult = GetLastError();
  if( !bResult ) {
    // If Io is pending wait for it to finish
    if( dwResult == ERROR_IO_PENDING ) {
      // Wait for actual data to be read if pending
      wprintf(L"Wait for it to finish %I64d\n", offset);
      bResult = GetOverlappedResult(hDisk,&ovlp,&bytesIn,TRUE);
      if( !bResult ) {
        dwResult = GetLastError();
        if( dwResult != ERROR_IO_INCOMPLETE) {
          status = ERROR_READ_FAULT;
        }
      } else {
        // Finished read successfully
        status = ERROR_SUCCESS;
      }
    } else if( dwResult != ERROR_SUCCESS ) {
      wprintf(L"Status is: %i\n",(int)dwResult);
      status = ERROR_READ_FAULT;
    }
  }

  CloseHandle(ovlp.hEvent);
  return status;
}

int DiskWriter::FastCopy(HANDLE hRead, int64_t sectorRead, HANDLE hWrite, int64_t sectorWrite, __uint64_t sectors, UINT8 partNum)
{  // Set up the overlapped structure
  UNREFERENCED_PARAMETER(partNum);
  OVERLAPPED ovlWrite, ovlRead;
  int stride;
  __uint64_t sec;
  uint32_t bytesOut = 0;
  uint32_t bytesRead = 0;
  int readStatus = ERROR_SUCCESS;
  int status = ERROR_SUCCESS;
  BOOL bWriteDone = TRUE;
  BOOL bBuffer1 = TRUE;

  if (sectorWrite < 0)
  {
    sectorWrite = GetNumDiskSectors() + sectorWrite;
  }

  // Set initial stride size to size of buffer
  stride = MAX_TRANSFER_SIZE / DISK_SECTOR_SIZE;

  // Setup offsets for read and write
  ovlWrite.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (ovlWrite.hEvent == NULL) return ERROR_OUTOFMEMORY;
  ovlWrite.Offset = (uint32_t)(sectorWrite*DISK_SECTOR_SIZE);
  ovlWrite.OffsetHigh = ((sectorWrite*DISK_SECTOR_SIZE) >> 32);

  ovlRead.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (ovlRead.hEvent == NULL) return ERROR_OUTOFMEMORY;
  ovlRead.Offset = (uint32_t)(sectorRead*DISK_SECTOR_SIZE);
  ovlRead.OffsetHigh = ((sectorRead*DISK_SECTOR_SIZE) >> 32);

  if (hRead == INVALID_HANDLE_VALUE) {
    wprintf(L"hRead = INVALID_HANDLE_VALUE, zeroing input buffer\n");
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
    if (hRead != INVALID_HANDLE_VALUE) {
      bytesRead = 0;
      if(bBuffer1 ) readStatus = ReadFile(hRead, buffer1, stride*DISK_SECTOR_SIZE, &bytesRead, &ovlRead);
      else readStatus = ReadFile(hRead, buffer2, stride*DISK_SECTOR_SIZE, &bytesRead, &ovlRead);
      if (!readStatus) status = GetLastError();
      if (status == ERROR_IO_PENDING) readStatus = GetOverlappedResult(hRead, &ovlRead, &bytesRead, TRUE);
      if (!readStatus) {
        status = GetLastError();
        break;
      }
      status = ERROR_SUCCESS;

      // Need to round up to nearest sector size if read partial sector from input file
      bytesRead = (bytesRead + DISK_SECTOR_SIZE - 1) & ~(DISK_SECTOR_SIZE - 1);
    }

    ovlRead.Offset += bytesRead;
    if (ovlRead.Offset < bytesRead) ovlRead.OffsetHigh++;

    if (!bWriteDone) {
      // Wait for the previous write to complete before we start a new one
      bWriteDone = GetOverlappedResult(hWrite, &ovlWrite, &bytesOut, TRUE);
      if (!bWriteDone) {
        status = GetLastError();
        break;
      }
      status = ERROR_SUCCESS;
    }
    
    // Now start a write for the corresponding buffer we just read
    if (bBuffer1) bWriteDone = WriteFile(hWrite, buffer1, bytesRead, NULL, &ovlWrite);
    else bWriteDone = WriteFile(hWrite, buffer2, bytesRead, NULL, &ovlWrite);

    bBuffer1 = !bBuffer1;
    sec += stride;
    sectorRead += stride;

    ovlWrite.Offset += bytesRead;
    if (ovlWrite.Offset < bytesOut) ovlWrite.OffsetHigh++;

    wprintf(L"Sectors remaining: %i      \r", (int)(sectors - sec));
  }
  wprintf(L"\nStatus = %i\n",status);

  // If we hit end of file and we read some data then round up to nearest block and write it out and wait
  // for it to complete else the next operation might fail.
  if ((sec < sectors) && (bytesRead > 0) && (status == ERROR_SUCCESS)) {
    bytesRead = (bytesRead + DISK_SECTOR_SIZE - 1) & ~(DISK_SECTOR_SIZE - 1);
    if (bBuffer1) bWriteDone = WriteFile(hWrite, buffer1, bytesRead, NULL, &ovlWrite);
    else  bWriteDone = WriteFile(hWrite, buffer2, bytesRead, NULL, &ovlWrite);
    wprintf(L"Wrote last bytes: %i\n", (int)bytesRead);
  }

  // Wait for pending write transfers to complete
  if (!bWriteDone && (GetLastError() == ERROR_IO_PENDING)) {
    bWriteDone = GetOverlappedResult(hWrite, &ovlWrite, &bytesOut, TRUE);
    if (!bWriteDone) status = GetLastError();
  }

  CloseHandle(ovlRead.hEvent);
  CloseHandle(ovlWrite.hEvent);
  return status;
}

int DiskWriter::GetRawDiskSize( __uint64_t *ds)
{
  int status = ERROR_SUCCESS;
  // Start at 512 MB for good measure to get us to size quickly
  __uint64_t diff = DISK_SECTOR_SIZE * 1024;

  // Read data from various sectors till we figure out how big disk is
  if( ds == NULL || hDisk == INVALID_HANDLE_VALUE) {
    return ERROR_INVALID_PARAMETER;
  }

  // Keep doubling size till we can't read anymore
  *ds = 0;
  for(;;) {
    status = RawReadTest(*ds + diff);
    if (diff <= DISK_SECTOR_SIZE) {
      if( status == ERROR_SUCCESS) {
        *ds = *ds + diff;
      }
      break;
    }
    if( status == ERROR_SUCCESS) {
      *ds = *ds + diff;
      diff = diff * 2;
    } else {
      diff = diff / 2;
    }
  }
  wprintf(L"Value of ds is %I64d\n",*ds);

  return ERROR_SUCCESS;
}

int DiskWriter::LockDevice()
{
  uint32_t bytesRead = 0;
	  if( DeviceIoControl( hDisk,FSCTL_DISMOUNT_VOLUME,NULL,0,NULL,0,&bytesRead,NULL) ) {
      if( DeviceIoControl( hDisk,FSCTL_LOCK_VOLUME,NULL,0,NULL,0,&bytesRead,NULL) ) {
        wprintf(L"Locked volume and dismounted\n");
        return ERROR_SUCCESS;
	    }
    }
  return ERROR_GEN_FAILURE;
}

int DiskWriter::UnlockDevice()
{
  uint32_t bytesRead = 0;
    if( DeviceIoControl( hDisk,FSCTL_UNLOCK_VOLUME,NULL,0,NULL,0,&bytesRead,NULL) ) {
      return ERROR_SUCCESS;
    }
  return ERROR_GEN_FAILURE;
}
