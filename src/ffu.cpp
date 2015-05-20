/*****************************************************************************
* ffu.cpp
*
* This class implements FFU image support
*
* Copyright (c) 2007-2011
* Qualcomm Technologies Incorporated.
* All Rights Reserved.
* Qualcomm Confidential and Proprietary
*
*****************************************************************************/
/*=============================================================================
Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/ffu.cpp#20 $
$DateTime: 2015/05/07 21:41:17 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
11/04/12   pgw     Initial version.
=============================================================================*/

#include "stdio.h"
#include "stdlib.h"
#include "ffu.h"
#include "sysdeps.h"
#include "string.h"

// Constructor
FFUImage::FFUImage()
{
  hFFU = -1;
  ValidationEntries = NULL;
  BlockDataEntries = NULL;
  GptEntries = NULL;

  // Default sector size can be overridden
  DISK_SECTOR_SIZE = 512;
}

FFUImage::~FFUImage()
{
  if(GptEntries) free(GptEntries);
  if(BlockDataEntries) free(BlockDataEntries);
  if(ValidationEntries) free(ValidationEntries);
  ValidationEntries=NULL;
  BlockDataEntries = NULL;
  GptEntries = NULL;
}

// Set the disk sector size normally 512 for eMMC or 4096 for UFS
void FFUImage::SetDiskSectorSize(int size)
{
  DISK_SECTOR_SIZE = size;
}

// Given a 64 bit memory address, set the Offset and OffsetHigh of an OVERLAPPED variable 
void FFUImage::SetOffset(void** ovlVariable, uint64_t offset)
{
/*  ResetEvent(ovlVariable->hEvent);
  ovlVariable->Offset = offset & 0xffffffff;
  ovlVariable->OffsetHigh = (offset & 0xffffffff00000000) >> 32;*/
}

// Compute the size of the padding at the end of each section of the FFU, and return
// the offset into the file of the next starting area.
uint64_t FFUImage::GetNextStartingArea(uint64_t chunkSizeInBytes, uint64_t sizeOfArea)
{
  uint64_t extraSpacePastBoundary = sizeOfArea % chunkSizeInBytes;
  // If the area does not end on a chunk boundary (i.e. there is a padding), compute next boundary.
  if (extraSpacePastBoundary != 0) {
    return sizeOfArea + (chunkSizeInBytes - extraSpacePastBoundary);
  } else {
    return  sizeOfArea;
  }

}

int FFUImage::AddEntryToRawProgram(char *szRawProgram, char *szFileName, uint64_t ui64FileOffset, int64_t i64StartSector, uint64_t ui64NumSectors)
{
/*  HANDLE hRawPrg;
  char szXMLString[MAX_STRING_LEN * 2];
  char szFile[MAX_STRING_LEN];
  uint32_t dwBytesWrite;
  size_t sBytesConvert;
  int status = ERROR_SUCCESS;
  hRawPrg = CreateFile(szRawProgram,
    GENERIC_WRITE | GENERIC_READ,
    0,    // We want exclusive access to this disk
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL);
  if (hRawPrg == -1) {
    status = GetLastError();
  }
  else {
    SetFilePointer(hRawPrg, 0, 0, FILE_END);
    wcstombs_s(&sBytesConvert, szFile, sizeof(szFile), szFileName, sizeof(szFile));
    if (i64StartSector < 0) {
      sprintf_s(szXMLString, sizeof(szXMLString), "<program SECTOR_SIZE_IN_BYTES=\"%i\" file_sector_offset=\"%I64d\" filename=\"%s\" label=\"ffu_image_%I64d\" num_partition_sectors=\"%I64d\" physical_partition_number=\"0\" size_in_KB=\"%I64d\" sparse=\"false\" start_byte_hex=\"0x%I64x\" start_sector=\"NUM_DISK_SECTORS%I64d\"/>\n",
        DISK_SECTOR_SIZE,ui64FileOffset, szFile, i64StartSector, ui64NumSectors, ui64NumSectors / 2, i64StartSector * 512, i64StartSector);
    }
    else {
      sprintf_s(szXMLString, sizeof(szXMLString), "<program SECTOR_SIZE_IN_BYTES=\"%i\" file_sector_offset=\"%I64d\" filename=\"%s\" label=\"ffu_image_%I64d\" num_partition_sectors=\"%I64d\" physical_partition_number=\"0\" size_in_KB=\"%I64d\" sparse=\"false\" start_byte_hex=\"0x%I64x\" start_sector=\"%I64d\"/>\n",
        DISK_SECTOR_SIZE, ui64FileOffset, szFile, i64StartSector, ui64NumSectors, ui64NumSectors / 2, i64StartSector * 512, i64StartSector);
    }
    WriteFile(hRawPrg, szXMLString, strlen(szXMLString), &dwBytesWrite, NULL);
    CloseHandle(hRawPrg);
  }

  return status;*/
	return 0;
}

int FFUImage::TerminateRawProgram(char *szFileName)
{
/*  HANDLE hRawPrg;
  uint32_t dwBytesOut;
  char szXMLString[] = "</data>\n";
  hRawPrg = CreateFile( szFileName,
    GENERIC_WRITE | GENERIC_READ,
    0,    // We want exclusive access to this disk
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL);
  
  if( hRawPrg == -1 ) {
    return GetLastError();
  }

  SetFilePointer(hRawPrg,0,0,FILE_END);

  // write out all the entries for gpt_main0.bin
  WriteFile(hRawPrg,szXMLString,sizeof(szXMLString)-1,&dwBytesOut,NULL);

  CloseHandle(hRawPrg);
  return ERROR_SUCCESS;*/
  return 0;
}

int FFUImage::CreateRawProgram(char *szFFUFile, char *szFileName)
{
  /*HANDLE hRawPrg;
  uint32_t dwBytesOut;
  char szPath[256] = { 0 };
  char szOut[256] = { 0 };
  char szXMLString[256] = { 0 };
  sprintf_s(szXMLString, sizeof(szXMLString), "<?xml version=\"1.0\" ?>\n<data>\n");
  hRawPrg = CreateFile(szFileName,
    GENERIC_WRITE | GENERIC_READ,
    0,    // We want exclusive access to this disk
    NULL,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    NULL);
  if (hRawPrg == -1) {
    return GetLastError();
  }

  // write out all the entries for gpt_main0.bin
  WriteFile(hRawPrg, szXMLString, strlen(szXMLString), &dwBytesOut, NULL);

  wcstombs_s((size_t *)&dwBytesOut, szPath, sizeof(szPath), szFFUFile, wcslen(szFFUFile));
  wcstombs_s((size_t *)&dwBytesOut, szOut, sizeof(szOut), szFileName, wcslen(szFileName));
  sprintf_s(szXMLString, sizeof(szXMLString), "<!-- emmcdl.exe -splitffu %s -o %s -->\n", szPath, szOut);
  WriteFile(hRawPrg, szXMLString, strlen(szXMLString), &dwBytesOut, NULL);

  CloseHandle(hRawPrg);
  return ERROR_SUCCESS;*/
	return 0;
}

int FFUImage::ReadGPT(void)
{
 /* // Read in the data chunk for GPT
  if (hFFU == -1 ) return ERROR_INVALID_HANDLE;
  uint32_t bytesRead = 0;
  uint64_t readOffset = PayloadDataStart + FFUStoreHeader.dwFinalTableIndex*FFUStoreHeader.dwBlockSizeInBytes;

  GptEntries = (gpt_entry_t*)malloc(sizeof(gpt_entry_t)*128);
  SetOffset(&OvlRead, readOffset);

  // Read contents directly from FFU into the partition table
  if (!ReadFile(hFFU, &GptProtectiveMBR, sizeof(GptProtectiveMBR), &bytesRead, &OvlRead)){
    return GetLastError();
  }
  readOffset += sizeof(GptProtectiveMBR);
  SetOffset(&OvlRead, readOffset);
  if (!ReadFile(hFFU, &GptHeader, sizeof(GptHeader), &bytesRead, &OvlRead)){
    return GetLastError();
  }

  readOffset += sizeof(GptHeader);
  SetOffset(&OvlRead, readOffset);
  if (!ReadFile(hFFU, GptEntries, sizeof(gpt_entry_t)* 128, &bytesRead, &OvlRead)){
    return GetLastError();
  }

  return ERROR_SUCCESS;*/
	return 0;
}


int FFUImage::ParseHeaders(void)
{
 /* int hashedChunkSizeInBytes;
  int hashedChunkSizeInKB;
  uint64_t currentAreaEndSpot = 0;
  uint64_t nextAreaStartSpot = 0;
  uint32_t bytesRead = 0;

  if( hFFU == -1 ) return ERROR_INVALID_HANDLE;

   Read the Security Header
  SetOffset(&OvlRead, 0);
  if( !ReadFile(hFFU, &FFUSecurityHeader, sizeof(FFUSecurityHeader), &bytesRead, &OvlRead) ) {
    return ERROR_READ_FAULT;
  }

  hashedChunkSizeInKB = FFUSecurityHeader.dwChunkSizeInKb;
  hashedChunkSizeInBytes = hashedChunkSizeInKB * 1024;

   Read the Image Header

  // Get the location in the file of the ImageHeader
  currentAreaEndSpot = sizeof(FFUSecurityHeader) + FFUSecurityHeader.dwCatalogSize + FFUSecurityHeader.dwHashTableSize;
  nextAreaStartSpot = GetNextStartingArea(hashedChunkSizeInBytes, currentAreaEndSpot);

  // Read memory
  SetOffset(&OvlRead, nextAreaStartSpot);
  if (!ReadFile(hFFU, &FFUImageHeader, sizeof(FFUImageHeader), &bytesRead, &OvlRead)) 
    return GetLastError();

   Read the StoreHeader

  // Get the location in the file of the StoreHeader
  currentAreaEndSpot = nextAreaStartSpot + sizeof(FFUImageHeader) + FFUImageHeader.ManifestLength;
  nextAreaStartSpot = GetNextStartingArea(hashedChunkSizeInBytes, currentAreaEndSpot);

  // Read memory
  SetOffset(&OvlRead, nextAreaStartSpot);
  if (!ReadFile(hFFU, &FFUStoreHeader, sizeof(FFUStoreHeader), &bytesRead, &OvlRead)) {
    return GetLastError();
  }

  nextAreaStartSpot += sizeof(STORE_HEADER);
  nextAreaStartSpot = GetNextStartingArea(sizeof(BLOCK_DATA_ENTRY), nextAreaStartSpot);

  // Verify the values read in are sane
  if( (FFUStoreHeader.dwValidateDescriptorLength < 1000000 ) && 
      (FFUStoreHeader.dwWriteDescriptorLength < 1000000) && 
      (FFUStoreHeader.dwWriteDescriptorCount < 0x10000000) ) {

    ValidationEntries = (unsigned char *)malloc(FFUStoreHeader.dwValidateDescriptorLength);
    BlockDataEntries = (BLOCK_DATA_ENTRY*)malloc(FFUStoreHeader.dwWriteDescriptorLength);

    if(ValidationEntries == NULL || BlockDataEntries == NULL ) {
      wprintf(L"Failed to allocated memory for headers\n");
      return ERROR_OUTOFMEMORY;
    }

    // Get the location in the file of the Validation Entries, read from file
    if (FFUStoreHeader.dwValidateDescriptorLength > 0){
      SetOffset(&OvlRead, nextAreaStartSpot);
      if (!ReadFile(hFFU, ValidationEntries, FFUStoreHeader.dwValidateDescriptorLength, &bytesRead, &OvlRead)) 
        return GetLastError();
      nextAreaStartSpot += FFUStoreHeader.dwValidateDescriptorLength;
    }

    // Get location of start of block data entries they are aligned to the size of a BLOCK_DATA_ENTRY
    if (FFUStoreHeader.dwWriteDescriptorLength > 0){
      SetOffset(&OvlRead, nextAreaStartSpot);
      if (!ReadFile(hFFU, BlockDataEntries, FFUStoreHeader.dwWriteDescriptorLength, &bytesRead, &OvlRead))
        return GetLastError();
      nextAreaStartSpot += FFUStoreHeader.dwWriteDescriptorLength;
    }

    // Get the location of the payload data
    nextAreaStartSpot = GetNextStartingArea(hashedChunkSizeInBytes, nextAreaStartSpot);
    PayloadDataStart = nextAreaStartSpot;
  }
  return ERROR_SUCCESS;*/
	return 0;
}

int FFUImage::DumpRawProgram(char *szFFUFile, char *szRawProgram)
{
  /*BOOL  bPendingData = FALSE;
  uint32_t dwNextBlockIndex = 0;
  uint32_t dwBlockOffset = 0;
  uint32_t dwStartBlock = 0;
  BLOCK_DATA_ENTRY* BlockDataEntriesPtr = BlockDataEntries;
  int status = ERROR_SUCCESS;
  int64_t diskOffset = 0;

  for (UINT32 i = 0; i < FFUStoreHeader.dwWriteDescriptorCount; i++) {
    for (UINT32 j = 0; j < BlockDataEntriesPtr->dwLocationCount; j++){
      if (BlockDataEntriesPtr->rgDiskLocations[j].dwDiskAccessMethod == DISK_BEGIN) {
        // If this block does not follow the previous block create a new chunk 
        if (BlockDataEntriesPtr->rgDiskLocations[j].dwBlockIndex != dwNextBlockIndex) {
          // Add this entry to the rawprogram0.xml
          if (bPendingData) {
            AddEntryToRawProgram(szRawProgram, szFFUFile, (PayloadDataStart + dwStartBlock*FFUStoreHeader.dwBlockSizeInBytes) / 512,
              (diskOffset / 512), (dwBlockOffset - dwStartBlock) * 128 * 2);
          }
          diskOffset = (uint64_t)BlockDataEntriesPtr->rgDiskLocations[j].dwBlockIndex*(uint64_t)FFUStoreHeader.dwBlockSizeInBytes;
          dwStartBlock = dwBlockOffset;
          bPendingData = TRUE;
        }

        dwNextBlockIndex = BlockDataEntriesPtr->rgDiskLocations[j].dwBlockIndex + 1;

      }
      else if (BlockDataEntriesPtr->rgDiskLocations[j].dwDiskAccessMethod == DISK_END) {
        // Add this entry to the rawprogram0.xml
        if (bPendingData) {
          AddEntryToRawProgram(szRawProgram, szFFUFile, (PayloadDataStart + dwStartBlock*FFUStoreHeader.dwBlockSizeInBytes) / 512,
            (diskOffset / 512), (i - dwStartBlock) * 128 * 2);
        }

        diskOffset = (int64_t)-1 * (int64_t)FFUStoreHeader.dwBlockSizeInBytes;  // End of disk is -1

        // Add this entry to the rawprogram0.xml special case this should be last chunk in the disk
        AddEntryToRawProgram(szRawProgram, szFFUFile, (PayloadDataStart + dwBlockOffset*FFUStoreHeader.dwBlockSizeInBytes) / 512,
          (diskOffset / 512), FFUStoreHeader.dwBlockSizeInBytes / 512);
        bPendingData = FALSE;
      }

    }

    // Increment our offset by number of blocks
    if (BlockDataEntriesPtr->dwLocationCount > 0) {
      BlockDataEntriesPtr = (BLOCK_DATA_ENTRY *)((uint32_t)BlockDataEntriesPtr + (sizeof(BLOCK_DATA_ENTRY)+(BlockDataEntriesPtr->dwLocationCount - 1)*sizeof(DISK_LOCATION)));
    }
    else {
      BlockDataEntriesPtr = (BLOCK_DATA_ENTRY *)((uint32_t)BlockDataEntriesPtr + sizeof(BLOCK_DATA_ENTRY));
    }
    dwBlockOffset++;
  }

  if (bPendingData) {
    // Add this entry to the rawprogram0.xml
    AddEntryToRawProgram(szRawProgram, szFFUFile, (PayloadDataStart + dwStartBlock*FFUStoreHeader.dwBlockSizeInBytes) / 512,
      (diskOffset / 512), (dwBlockOffset - dwStartBlock) * 128 * 2);
  }

  return status;*/
}

int FFUImage::FFUToRawProgram(char *szFFUName, char *szImageFile)
{
 /* char *szFFUFile = wcsrchr(szFFUName, L'\\');
  int status = PreLoadImage(szFFUName);
  if (status != ERROR_SUCCESS) goto FFUToRawProgramExit;
  status = CreateRawProgram(szFFUName, szImageFile);
  if (status != ERROR_SUCCESS) goto FFUToRawProgramExit;
  // Only put the filename in the output 
  if (szFFUFile != NULL) szFFUFile++;
  else szFFUFile = szFFUName;
  status = DumpRawProgram(szFFUFile, szImageFile);
  if (status != ERROR_SUCCESS) goto FFUToRawProgramExit;
  status = TerminateRawProgram(szImageFile);

FFUToRawProgramExit:
  CloseFFUFile();
  return status;*/
}

int FFUImage::FFUDumpDisk(Protocol *proto)
{
  /*int status = ERROR_SUCCESS;
  uint32_t bytesRead = 0;
  uint32_t dwTotalBytes = 0;
  uint32_t dwBlockCount = 0;
  uint32_t lastLBA = 0;
  int64_t diskStart = 0;
  int64_t diskOffset = 0;
  BLOCK_DATA_ENTRY* BlockDataEntriesPtr = BlockDataEntries;

   Copy the data to disk
  unsigned char *dataBlock = (unsigned char*)malloc(FFUStoreHeader.dwBlockSizeInBytes*64); // Create storage for 64*128 KB block
  if( dataBlock == NULL ) return ERROR_OUTOFMEMORY;
  for (UINT32 i = 0; i < FFUStoreHeader.dwWriteDescriptorCount; i++) {
    for (UINT32 j = 0; j < BlockDataEntriesPtr->dwLocationCount; j++){
      if (BlockDataEntriesPtr->rgDiskLocations[j].dwDiskAccessMethod == DISK_BEGIN) {
        // If our new block is not at the location of the last block + 1 then write out pending old data and update lastLBA 
        if (BlockDataEntriesPtr->rgDiskLocations[j].dwBlockIndex != lastLBA + 1 || (dwTotalBytes > FFUStoreHeader.dwBlockSizeInBytes * 60))
        {
          if (dwTotalBytes > 0) {
            printf("dwBlockIndex: %i\n", BlockDataEntriesPtr->rgDiskLocations[j].dwBlockIndex);
            printf("dwBlockSizeInBytes: %i\n", FFUStoreHeader.dwBlockSizeInBytes);
            status = proto->WriteData(dataBlock, diskStart, dwTotalBytes, &bytesRead,0);
            if (status != ERROR_SUCCESS) goto FFUDumpDiskCleanUp;
          }
          // reset the disk start address and total bytes read
          diskStart = (int64_t)BlockDataEntriesPtr->rgDiskLocations[j].dwBlockIndex*(int64_t)FFUStoreHeader.dwBlockSizeInBytes;
          dwTotalBytes = 0;
        }

        lastLBA = BlockDataEntriesPtr->rgDiskLocations[j].dwBlockIndex;

        // Create new file to dump this chunk
        SetOffset(&OvlRead, PayloadDataStart + dwBlockCount*FFUStoreHeader.dwBlockSizeInBytes);
        if (!ReadFile(hFFU, dataBlock + dwTotalBytes, FFUStoreHeader.dwBlockSizeInBytes, &bytesRead, &OvlRead)) {
          status = GetLastError();
		  goto FFUDumpDiskCleanUp;
		}
        dwTotalBytes += bytesRead;
      }
      else if (BlockDataEntriesPtr->rgDiskLocations[j].dwDiskAccessMethod == DISK_END) {
        // If there is any pending data from DISK_START write it out before reading in this data for disk end
        if (dwTotalBytes > 0) {
          status = proto->WriteData(dataBlock, diskStart, dwTotalBytes, &bytesRead,0);
          dwTotalBytes = 0;
        }
        diskOffset = -1 * (int64_t)(BlockDataEntriesPtr->rgDiskLocations[j].dwBlockIndex + 1)*(int64_t)FFUStoreHeader.dwBlockSizeInBytes;  // End of disk is -1
        // Create new file to dump this chunk
        SetOffset(&OvlRead, PayloadDataStart + dwBlockCount*FFUStoreHeader.dwBlockSizeInBytes);
        if (!ReadFile(hFFU, dataBlock, FFUStoreHeader.dwBlockSizeInBytes, &bytesRead, &OvlRead)) {
          status = GetLastError();
          goto FFUDumpDiskCleanUp;
        }

        // Write the data into a file
        status = proto->WriteData(dataBlock, diskOffset, bytesRead, &bytesRead,0);
      }

    }
	// Increment our offset by number of blocks
	if (BlockDataEntriesPtr->dwLocationCount > 0) {
		BlockDataEntriesPtr = (BLOCK_DATA_ENTRY *)((uint32_t)BlockDataEntriesPtr + (sizeof(BLOCK_DATA_ENTRY)+(BlockDataEntriesPtr->dwLocationCount - 1)*sizeof(DISK_LOCATION)));
	}
	else {
		BlockDataEntriesPtr = (BLOCK_DATA_ENTRY *)((uint32_t)BlockDataEntriesPtr + sizeof(BLOCK_DATA_ENTRY));
	}
	dwBlockCount++;
  }

  // Only write out the data 
  if (dwTotalBytes > 0)
  {
    status = proto->WriteData(dataBlock, diskStart, dwTotalBytes, &bytesRead,0);
  }
  // If we programmed successfully reset the device
  proto->DeviceReset();

FFUDumpDiskCleanUp:
  free(dataBlock);
  return status;*/

}

int FFUImage::ProgramImage(Protocol *proto, int64_t dwOffset)
{
 /* UNREFERENCED_PARAMETER(dwOffset); // This value is embedded in the FFU
  int status = ERROR_SUCCESS;

  // Make sure parameters passed in are valid
  if( proto == NULL ) {
    return ERROR_INVALID_PARAMETER;
  }

  status = FFUDumpDisk(proto);
  return status;*/
}

int FFUImage::PreLoadImage(char *szFFUPath)
{
 /* int status = ERROR_SUCCESS;
  OvlRead.hEvent = CreateEvent(NULL, FALSE, NULL, NULL);
  OvlWrite.hEvent = CreateEvent(NULL, FALSE, NULL, NULL);
  if( (OvlRead.hEvent == NULL) || (OvlWrite.hEvent == NULL)) {
    return ERROR_OUTOFMEMORY;
  }
  ResetEvent(OvlRead.hEvent);
  ResetEvent(OvlWrite.hEvent);

  hFFU = CreateFile( szFFUPath,
    GENERIC_READ,
    FILE_SHARE_READ, // Open file for read and let others access it as well
    NULL,
    4,
    FILE_ATTRIBUTE_NORMAL,
    NULL);
  if (hFFU == -1) return GetLastError();

  status = ParseHeaders();
  if( status != ERROR_SUCCESS ) return status;
  status = ReadGPT();

  return status;*/
}

int FFUImage::SplitFFUBin( char *szPartName, char *szOutputFile)
{
  /*uint32_t bytesRead = 0;
  uint32_t start_sector = 0;
  uint32_t end_sector = 0;
  int status = ERROR_SUCCESS;
  HANDLE hImage = -1;
  uint32_tLONG diskOffset = 0;
  UINT32 i=0;
  char szNewFile[256];

  for(i=0; i < 128; i++) {
    // If all is selected then dump all partitions
    if( wcscmp(szPartName,_T("all")) == 0  ) {
      // Ignore partitions without name
      if(wcscmp(GptEntries[i].part_name,_T("")) == 0)
      {
        continue;
      }
      SplitFFUBin(GptEntries[i].part_name,szOutputFile);
    } else if( wcscmp(GptEntries[i].part_name,szPartName) == 0 ) {
      start_sector = (uint32_t)GptEntries[i].first_lba;
      end_sector = (uint32_t)GptEntries[i].last_lba;
      break;
    }
  }

  // If no partition was found or crashdump partition return
  if( (i == 128) || (wcscmp(GptEntries[i].part_name,L"CrashDump") == 0) ) return ERROR_SUCCESS;

  swprintf_s(szNewFile,_T("%s\\%s.bin"),szOutputFile,GptEntries[i].part_name);

  for (i = 0; i < FFUStoreHeader.dwWriteDescriptorCount; i++) {
    if(BlockDataEntries[i].dwBlockCount == 0)
    {
      // No blocks attached to this entry
      continue;
    }

    diskOffset = BlockDataEntries[i].rgDiskLocations[0].dwBlockIndex;
    diskOffset *= FFUStoreHeader.dwBlockSizeInBytes;

    if( ((diskOffset/512) >= start_sector) &&  ((diskOffset/512) <= end_sector)) {
      unsigned char *dataBlock = (unsigned char*)malloc(FFUStoreHeader.dwBlockSizeInBytes); // Create storage for 128 KB block
      if(dataBlock == NULL ) return ERROR_OUTOFMEMORY;

      SetOffset(&OvlRead, PayloadDataStart+(i)*FFUStoreHeader.dwBlockSizeInBytes );
      if (!ReadFile(hFFU, dataBlock, FFUStoreHeader.dwBlockSizeInBytes, &bytesRead, &OvlRead)) {
        status = GetLastError();
        break;
      }

      // Only create the file if there is data to write to it
      if( hImage == -1 ) {
        wprintf(_T("Exporting file: %s\n"),szNewFile);
        hImage = CreateFile( szNewFile,
          GENERIC_WRITE | GENERIC_READ,
          0,    // We want exclusive access to this disk
          NULL,
          CREATE_ALWAYS,
          FILE_ATTRIBUTE_NORMAL,
          NULL);
        if( hImage == -1 ) return GetLastError();
      }

      uint32_tLONG tmp = start_sector; // Needed to force 64bit calc
      tmp = tmp * 512;
      diskOffset -= tmp;

      SetOffset(&OvlWrite, diskOffset);
      WriteFile(hImage, dataBlock, FFUStoreHeader.dwBlockSizeInBytes, &bytesRead, &OvlWrite);
      free(dataBlock);
    }
  }
  //}

  if(hImage != -1 ) CloseHandle(hImage);
  return status;*/
}

int FFUImage::CloseFFUFile(void)
{
/*  CloseHandle(OvlRead.hEvent);
  CloseHandle(OvlWrite.hEvent);
  CloseHandle(hFFU);
  hFFU = -1;
  return ERROR_SUCCESS;*/
}
