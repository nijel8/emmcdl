/*****************************************************************************
 * ffu.h
 *
 * This file has structures and functions for FFU format
 *
 * Copyright (c) 2007-2012
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/ffu.h#7 $
$DateTime: 2015/05/07 21:41:17 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
11/04/12   pgw     Initial version.
=============================================================================*/
#pragma once
#include "partition.h"
#include "serialport.h"
#include "firehose.h"
#include <windows.h>

// Security Header struct. The first data read in from the FFU.
typedef struct _SECURITY_HEADER
{
    UINT32 cbSize;            // size of struct, overall
    BYTE   signature[12];     // "SignedImage "
    UINT32 dwChunkSizeInKb;   // size of a hashed chunk within the image
    UINT32 dwAlgId;           // algorithm used to hash
    UINT32 dwCatalogSize;     // size of catalog to validate
    UINT32 dwHashTableSize;   // size of hash table
} SECURITY_HEADER;

// Image Header struct found within Image Header region of FFU
typedef struct _IMAGE_HEADER
{
    DWORD  cbSize;           // sizeof(ImageHeader)
    BYTE   Signature[12];    // "ImageFlash  "
    DWORD  ManifestLength;   // in bytes
    DWORD  dwChunkSize;      // Used only during image generation.
} IMAGE_HEADER;

// Store Header struct found within Store Header region of FFU
typedef struct _STORE_HEADER
{
    UINT32 dwUpdateType; // indicates partial or full flash
    UINT16 MajorVersion, MinorVersion; // used to validate struct
    UINT16 FullFlashMajorVersion, FullFlashMinorVersion; // FFU version, i.e. the image format
    char szPlatformId[192]; // string which indicates what device this FFU is intended to be written to
    UINT32 dwBlockSizeInBytes; // size of an image block in bytes – the device’s actual sector size may differ
    UINT32 dwWriteDescriptorCount; // number of write descriptors to iterate through
    UINT32 dwWriteDescriptorLength; // total size of all the write descriptors, in bytes (included so they can be read out up front and interpreted later)
    UINT32 dwValidateDescriptorCount; // number of validation descriptors to check
    UINT32 dwValidateDescriptorLength; // total size of all the validation descriptors, in bytes
    UINT32 dwInitialTableIndex; // block index in the payload of the initial (invalid) GPT
    UINT32 dwInitialTableCount; // count of blocks for the initial GPT, i.e. the GPT spans blockArray[idx..(idx + count -1)]
    UINT32 dwFlashOnlyTableIndex; // first block index in the payload of the flash-only GPT (included so safe flashing can be accomplished)
    UINT32 dwFlashOnlyTableCount; // count of blocks in the flash-only GPT
    UINT32 dwFinalTableIndex; // index in the table of the real GPT
    UINT32 dwFinalTableCount; // number of blocks in the real GPT
} STORE_HEADER;


enum DISK_ACCESS_METHOD
{
    DISK_BEGIN  = 0,
    DISK_SEQ    = 1,
    DISK_END    = 2
};

// Struct used in BLOCK_DATA_ENTRY structs to define where to put data on disk
typedef struct _DISK_LOCATION
{
    UINT32 dwDiskAccessMethod;
    UINT32 dwBlockIndex;
} DISK_LOCATION; 

typedef struct _BLOCK_DATA_ENTRY
{
    UINT32 dwLocationCount;
    UINT32 dwBlockCount;
    DISK_LOCATION rgDiskLocations[1];
} BLOCK_DATA_ENTRY;

class FFUImage {
public:
  int PreLoadImage(TCHAR *szFFUPath);
  int ProgramImage(Protocol *proto, __int64 dwOffset);
  int CloseFFUFile(void);
  void SetDiskSectorSize(int size);
  int SplitFFUBin(TCHAR *szPartName, TCHAR *szOutputFile);
  int FFUToRawProgram(TCHAR *szFFUName, TCHAR *szImagePath);
  FFUImage();
  ~FFUImage();

private:
  void SetOffset(OVERLAPPED* ovlVariable, UINT64 offset);
  int CreateRawProgram(TCHAR *szFFUFile, TCHAR *szFileName);
  int TerminateRawProgram(TCHAR *szFileName);
  int DumpRawProgram(TCHAR *szFFUFile, TCHAR *szRawProgram);
  int FFUDumpDisk(Protocol *proto);
  int AddEntryToRawProgram(TCHAR *szRawProgram, TCHAR *szFileName, UINT64 ui64FileOffset, __int64 i64StartSector, UINT64 ui64NumSectors);
  int ReadGPT(void);
  int ParseHeaders(void);
  UINT64 GetNextStartingArea(UINT64 chunkSizeInBytes, UINT64 sizeOfArea);

  // Headers found within FFU image
  SECURITY_HEADER FFUSecurityHeader;
  IMAGE_HEADER FFUImageHeader;
  STORE_HEADER FFUStoreHeader;
  BYTE* ValidationEntries;
  BLOCK_DATA_ENTRY* BlockDataEntries;
  UINT64 PayloadDataStart;

  HANDLE hFFU;
  OVERLAPPED OvlRead;
  OVERLAPPED OvlWrite;

  // GPT Stuff
  BYTE GptProtectiveMBR[512];
  gpt_header_t GptHeader;
  gpt_entry_t *GptEntries;
  int DISK_SECTOR_SIZE;

};


