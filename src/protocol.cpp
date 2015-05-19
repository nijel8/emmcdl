/*****************************************************************************
* protocol.cpp
*
* This class can perform emergency flashing given the transport layer
*
* Copyright (c) 2007-2015
* Qualcomm Technologies Incorporated.
* All Rights Reserved.
* Qualcomm Confidential and Proprietary
*
*****************************************************************************/
/*=============================================================================
Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/protocol.cpp#4 $
$DateTime: 2015/05/07 21:41:17 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
12/09/14   pgw     Initial version.
=============================================================================*/

#include "protocol.h"

Protocol::Protocol(void)
{
  hDisk = INVALID_HANDLE_VALUE;
  buffer1 = buffer2 = NULL;
  gpt_entries = NULL;
  bVerbose = true;

  // Set default sector size
  DISK_SECTOR_SIZE = 512;

  bufAlloc1 = (BYTE *)malloc(MAX_TRANSFER_SIZE + 0x200);
  if (bufAlloc1) buffer1 = (BYTE *)(((DWORD)bufAlloc1 + 0x200) & ~0x1ff);
  bufAlloc2 = (BYTE *)malloc(MAX_TRANSFER_SIZE + 0x200);
  if (bufAlloc2) buffer2 = (BYTE *)(((DWORD)bufAlloc2 + 0x200) & ~0x1ff);
}

Protocol::~Protocol(void)
{
  // Free any remaining buffers we may have
  if (bufAlloc1) free(bufAlloc1);
  if (bufAlloc2) free(bufAlloc2);
  if (gpt_entries) free(gpt_entries);
}

void Protocol::Log(char *str, ...)
{
  // For now map the log to dump output to console
  if (bVerbose) {
    va_list ap;
    va_start(ap, str);
    vprintf(str, ap);
    va_end(ap);
    printf("\n");
  }
}

void Protocol::Log(TCHAR *str, ...)
{
  // For now map the log to dump output to console
  if (bVerbose) {
    va_list ap;
    va_start(ap, str);
    vwprintf(str, ap);
    va_end(ap);
    wprintf(L"\n");
  }
}

void Protocol::EnableVerbose()
{
  bVerbose = true;
}

int Protocol::LoadPartitionInfo(TCHAR *szPartName, PartitionEntry *pEntry)
{
  int status = ERROR_SUCCESS;

  // First of all read in the GPT information and see if it was successful
  status = ReadGPT(false);
  if (status == ERROR_SUCCESS) {
    // Check to make sure partition name is found
    status = ERROR_NOT_FOUND;
    memset(pEntry, 0, sizeof(PartitionEntry));
    for (int i = 0; i < 128; i++) {
      if (wcscmp(szPartName, gpt_entries[i].part_name) == 0) {
        pEntry->start_sector = gpt_entries[i].first_lba;
        pEntry->num_sectors = gpt_entries[i].last_lba - gpt_entries[i].first_lba + 1;
        pEntry->physical_partition_number = 0;
        return ERROR_SUCCESS;
      }
    }
  }
  return status;
}

int Protocol::WriteGPT(TCHAR *szPartName, TCHAR *szBinFile)
{
  int status = ERROR_SUCCESS;
  PartitionEntry partEntry;

  TCHAR *cmd_pkt = (TCHAR *)malloc(MAX_XML_LEN*sizeof(TCHAR));
  if (cmd_pkt == NULL) {
    return ERROR_HV_INSUFFICIENT_MEMORY;
  }

  if (LoadPartitionInfo(szPartName, &partEntry) == ERROR_SUCCESS){
    Partition partition;
    wcscpy_s(partEntry.filename, szBinFile);
    partEntry.eCmd = CMD_PROGRAM;
    swprintf_s(cmd_pkt, MAX_XML_LEN, L"<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"0\" start_sector=\"%i\"/", DISK_SECTOR_SIZE, (int)partEntry.num_sectors, (int)partEntry.start_sector);
    status = partition.ProgramPartitionEntry(this, partEntry, cmd_pkt);
  }

  // Free our memory for command packet again
  free(cmd_pkt);
  return status;
}

int Protocol::ReadGPT(bool debug)
{
  int status = ERROR_SUCCESS;
  DWORD bytesRead;
  gpt_header_t gpt_hdr;
  gpt_entries = (gpt_entry_t*)malloc(sizeof(gpt_entry_t)* 128);

  if (gpt_entries == NULL) {
    return ERROR_OUTOFMEMORY;
  }

  status = ReadData((BYTE *)&gpt_hdr, DISK_SECTOR_SIZE, DISK_SECTOR_SIZE, &bytesRead,0);
  
  if ((status == ERROR_SUCCESS) && (memcmp("EFI PART", gpt_hdr.signature, 8) == 0)) {
    if (debug) wprintf(L"\nSuccessfully found GPT partition\n");
    status = ReadData((BYTE *)gpt_entries, 2*DISK_SECTOR_SIZE, 32*DISK_SECTOR_SIZE, &bytesRead,0);
    if ((status == ERROR_SUCCESS) && debug) {
      for (int i = 0; (i < gpt_hdr.num_entries) && (i < 128); i++) {
        if (gpt_entries[i].first_lba > 0)
          Log(L"%i. Partition Name: %s Start LBA: %I64d Size in LBA: %I64d", i + 1, gpt_entries[i].part_name, gpt_entries[i].first_lba, gpt_entries[i].last_lba - gpt_entries[i].first_lba + 1);
      }
    }
  }
  else {
    if (debug) Log(L"\nNo valid GPT found");
    free(gpt_entries);
    gpt_entries = NULL;
    status = ERROR_INVALID_DATA;
  }
  return status;
}


uint64 Protocol::GetNumDiskSectors()
{
  return disk_size / DISK_SECTOR_SIZE;

}

void Protocol::SetDiskSectorSize(int size)
{
  DISK_SECTOR_SIZE = size;
}

int Protocol::GetDiskSectorSize(void)
{
  return DISK_SECTOR_SIZE;
}

HANDLE Protocol::GetDiskHandle(void)
{
  return hDisk;
}

int Protocol::DumpDiskContents(uint64 start_sector, uint64 num_sectors, TCHAR *szOutFile, UINT8 partNum, TCHAR *szPartName)
{
  int status = ERROR_SUCCESS;

  // If there is a partition name provided load the info for the partition name
  if (szPartName != NULL) {
    PartitionEntry pe;
    if (LoadPartitionInfo(szPartName, &pe) == ERROR_SUCCESS) {
      start_sector = pe.start_sector;
      num_sectors = pe.num_sectors;
    }
    else {
      return ERROR_FILE_NOT_FOUND;
    }

  }

  // Create the output file and dump contents at offset for num blocks
  HANDLE hOutFile = CreateFile(szOutFile,
    GENERIC_WRITE | GENERIC_READ,
    0,    // We want exclusive access to this disk
    NULL,
    CREATE_ALWAYS,
    0, 
    NULL);

  // Make sure we were able to successfully create file handle
  if (hOutFile == INVALID_HANDLE_VALUE) {
    return GetLastError();
  }

  status = FastCopy(hDisk, start_sector, hOutFile, 0, num_sectors, partNum);
  CloseHandle(hOutFile);

  return status;
}

int Protocol::WipeDiskContents(uint64 start_sector, uint64 num_sectors, TCHAR *szPartName)
{
  PartitionEntry pe;
  TCHAR *cmd_pkt;
  int status = ERROR_SUCCESS;

  // If there is a partition name provided load the info for the partition name
  if (szPartName != NULL) {
    PartitionEntry pe;
    if (LoadPartitionInfo(szPartName, &pe) == ERROR_SUCCESS) {
      start_sector = pe.start_sector;
      num_sectors = pe.num_sectors;
    }
    else {
      return ERROR_FILE_NOT_FOUND;
    }

  }

  cmd_pkt = (TCHAR *)malloc(MAX_XML_LEN*sizeof(TCHAR));
  if (cmd_pkt == NULL) {
    return ERROR_HV_INSUFFICIENT_MEMORY;
  }

  memset(&pe, 0, sizeof(pe));
  wcscpy_s(pe.filename, L"ZERO");
  pe.start_sector = start_sector;
  pe.num_sectors = num_sectors;
  pe.eCmd = CMD_ERASE;
  pe.physical_partition_number = 0;  // By default the wipe disk only works on physical sector 0
  swprintf_s(cmd_pkt, MAX_XML_LEN, L"<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"0\" start_sector=\"%i\"/", DISK_SECTOR_SIZE, (int)num_sectors, (int)start_sector);
  Partition partition;
  status = partition.ProgramPartitionEntry(this,pe, cmd_pkt);

  free(cmd_pkt); // Free our buffer
  return status;
}

