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
#include <stdlib.h>

Protocol::Protocol(void)
{
  //hDisk = -1;
  //modify for 0 is output to uart
  hDisk = 0;
  buffer1 = buffer2 = NULL;
  gpt_entries = NULL;
  bVerbose = true;

  disk_size = 0;
  // Set default sector size
  DISK_SECTOR_SIZE = 512;

  bufAlloc1 = (unsigned char *)malloc(MAX_TRANSFER_SIZE + 0x200);
  if (bufAlloc1) buffer1 = (unsigned char *)(((uintptr_t)bufAlloc1 + 0x200) & ~0x1ff);
  bufAlloc2 = (unsigned char *)malloc(MAX_TRANSFER_SIZE + 0x200);
  if (bufAlloc2) buffer2 = (unsigned char *)(((uintptr_t)bufAlloc2 + 0x200) & ~0x1ff);
}

Protocol::~Protocol(void)
{
  // Free any remaining buffers we may have
  if (bufAlloc1) free(bufAlloc1);
  if (bufAlloc2) free(bufAlloc2);
  if (gpt_entries) free(gpt_entries);
}

void Protocol::Log(const char *str, ...)
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

void Protocol::EnableVerbose()
{
  bVerbose = true;
}

int Protocol::LoadPartitionInfo(char *szPartName, PartitionEntry *pEntry)
{
  int status = 0;

  // First of all read in the GPT information and see if it was successful
  status = ReadGPT(false);
  if (status == 0) {
    // Check to make sure partition name is found
    status = ENOENT;
    memset(pEntry, 0, sizeof(PartitionEntry));
    for (int i = 0; i < 128; i++) {
      if (strcmp(szPartName, gpt_entries[i].part_name) == 0) {
        pEntry->start_sector = gpt_entries[i].first_lba;
        pEntry->num_sectors = gpt_entries[i].last_lba - gpt_entries[i].first_lba + 1;
        pEntry->physical_partition_number = 0;
        return 0;
      }
    }
  }
  return status;
}

int Protocol::WriteGPT(char *szPartName, char *szBinFile)
{
  int status = 0;
  PartitionEntry partEntry;

  char *cmd_pkt = (char *)malloc(MAX_XML_LEN*sizeof(char));
  if (cmd_pkt == NULL) {
    return ENOMEM;
  }

  if (LoadPartitionInfo(szPartName, &partEntry) == 0){
    Partition partition;
    strcpy(partEntry.filename, szBinFile);
    partEntry.eCmd = CMD_PROGRAM;
    sprintf(cmd_pkt, "<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"0\" start_sector=\"%i\"/", DISK_SECTOR_SIZE, (int)partEntry.num_sectors, (int)partEntry.start_sector);
    status = partition.ProgramPartitionEntry(this, partEntry, cmd_pkt);
  }

  // Free our memory for command packet again
  free(cmd_pkt);
  return status;
}

int Protocol::ReadGPT(bool debug)
{
  int status = 0;
  uint32_t bytesRead;
  gpt_header_t gpt_hdr;
  gpt_entries = (gpt_entry_t*)malloc(sizeof(gpt_entry_t)* 128);

  if (gpt_entries == NULL) {
    return ENOMEM;
  }

  status = ReadData((unsigned char *)&gpt_hdr, DISK_SECTOR_SIZE, DISK_SECTOR_SIZE, &bytesRead,0);
  
  if ((status == 0) && (memcmp("EFI PART", gpt_hdr.signature, 8) == 0)) {
    if (debug) printf("\nSuccessfully found GPT partition\n");
    status = ReadData((unsigned char *)gpt_entries, 2*DISK_SECTOR_SIZE, 32*DISK_SECTOR_SIZE, &bytesRead,0);
    if ((status == 0) && debug) {
      for (int i = 0; (i < gpt_hdr.num_entries) && (i < 128); i++) {
        if (gpt_entries[i].first_lba > 0)
          Log("%i. Partition Name: %s Start LBA: %I64d Size in LBA: %I64d", i + 1, gpt_entries[i].part_name, gpt_entries[i].first_lba, gpt_entries[i].last_lba - gpt_entries[i].first_lba + 1);
      }
    }
  }
  else {
    if (debug) Log("\nNo valid GPT found");
    free(gpt_entries);
    gpt_entries = NULL;
    status = ERROR_INVALID_DATA;
  }
  return status;
}


__uint64_t Protocol::GetNumDiskSectors()
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

int Protocol::GetDiskHandle(void)
{
  return hDisk;
}

int Protocol::DumpDiskContents(__uint64_t start_sector, __uint64_t num_sectors, char *szOutFile, uint8_t partNum, char *szPartName)
{
  int status = 0;

  // If there is a partition name provided load the info for the partition name
  if (szPartName != NULL) {
    PartitionEntry pe;
    if (LoadPartitionInfo(szPartName, &pe) == 0) {
      start_sector = pe.start_sector;
      num_sectors = pe.num_sectors;
    }
    else {
      return ENOENT;
    }

  }

  // Create the output file and dump contents at offset for num blocks
  int hOutFile = emmcdl_creat(szOutFile,O_CREAT | O_EXCL | O_RDWR);

  // Make sure we were able to successfully create file handle
  if (hOutFile < 0) {
    return errno;
  }

  status = FastCopy(hDisk, start_sector, hOutFile, 0, num_sectors, partNum);
  emmcdl_close(hOutFile);

  return status;
}

int Protocol::WipeDiskContents(__uint64_t start_sector, __uint64_t num_sectors, char *szPartName)
{
  PartitionEntry pe;
  char *cmd_pkt;
  int status = 0;

  // If there is a partition name provided load the info for the partition name
  if (szPartName != NULL) {
    PartitionEntry pe;
    if (LoadPartitionInfo(szPartName, &pe) == 0) {
      start_sector = pe.start_sector;
      num_sectors = pe.num_sectors;
    }
    else {
      return ENOENT;
    }

  }

  cmd_pkt = (char *)malloc(MAX_XML_LEN*sizeof(char));
  if (cmd_pkt == NULL) {
    return ENOMEM;
  }

  memset(&pe, 0, sizeof(pe));
  strcpy(pe.filename, "ZERO");
  pe.start_sector = start_sector;
  pe.num_sectors = num_sectors;
  pe.eCmd = CMD_ERASE;
  pe.physical_partition_number = 0;  // By default the wipe disk only works on physical sector 0
  sprintf(cmd_pkt, "<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"0\" start_sector=\"%i\"/", DISK_SECTOR_SIZE, (int)num_sectors, (int)start_sector);
  Partition partition;
  status = partition.ProgramPartitionEntry(this,pe, cmd_pkt);

  free(cmd_pkt); // Free our buffer
  return status;
}

