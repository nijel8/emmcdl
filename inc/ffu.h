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
#include "sysdeps.h"

// Security Header struct. The first data read in from the FFU.
typedef struct _SECURITY_HEADER
{
    uint32_t cbSize;            // size of struct, overall
    unsigned char   signature[12];     // "SignedImage "
    uint32_t dwChunkSizeInKb;   // size of a hashed chunk within the image
    uint32_t dwAlgId;           // algorithm used to hash
    uint32_t dwCatalogSize;     // size of catalog to validate
    uint32_t dwHashTableSize;   // size of hash table
} SECURITY_HEADER;

// Image Header struct found within Image Header region of FFU
typedef struct _IMAGE_HEADER
{
    uint32_t  cbSize;           // sizeof(ImageHeader)
    unsigned char   Signature[12];    // "ImageFlash  "
    uint32_t  ManifestLength;   // in bytes
    uint32_t  dwChunkSize;      // Used only during image generation.
} IMAGE_HEADER;

// Store Header struct found within Store Header region of FFU
typedef struct _STORE_HEADER
{
    uint32_t dwUpdateType; // indicates partial or full flash
    uint16_t MajorVersion, MinorVersion; // used to validate struct
    uint16_t FullFlashMajorVersion, FullFlashMinorVersion; // FFU version, i.e. the image format
    char szPlatformId[192]; // string which indicates what device this FFU is intended to be written to
    uint32_t dwBlockSizeInBytes; // size of an image block in bytes � the device�s actual sector size may differ
    uint32_t dwWriteDescriptorCount; // number of write descriptors to iterate through
    uint32_t dwWriteDescriptorLength; // total size of all the write descriptors, in bytes (included so they can be read out up front and interpreted later)
    uint32_t dwValidateDescriptorCount; // number of validation descriptors to check
    uint32_t dwValidateDescriptorLength; // total size of all the validation descriptors, in bytes
    uint32_t dwInitialTableIndex; // block index in the payload of the initial (invalid) GPT
    uint32_t dwInitialTableCount; // count of blocks for the initial GPT, i.e. the GPT spans blockArray[idx..(idx + count -1)]
    uint32_t dwFlashOnlyTableIndex; // first block index in the payload of the flash-only GPT (included so safe flashing can be accomplished)
    uint32_t dwFlashOnlyTableCount; // count of blocks in the flash-only GPT
    uint32_t dwFinalTableIndex; // index in the table of the real GPT
    uint32_t dwFinalTableCount; // number of blocks in the real GPT
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
    uint32_t dwDiskAccessMethod;
    uint32_t dwBlockIndex;
} DISK_LOCATION; 

typedef struct _BLOCK_DATA_ENTRY
{
    uint32_t dwLocationCount;
    uint32_t dwBlockCount;
    DISK_LOCATION rgDiskLocations[1];
} BLOCK_DATA_ENTRY;

class FFUImage {
public:
  int PreLoadImage(char *szFFUPath);
  int ProgramImage(Protocol *proto, int64_t dwOffset);
  int CloseFFUFile(void);
  void SetDiskSectorSize(int size);
  int SplitFFUBin(char *szPartName, char *szOutputFile);
  int FFUToRawProgram(char *szFFUName, char *szImagePath);
  FFUImage();
  ~FFUImage();

private:
  void SetOffset(void** ovlVariable, uint64_t offset);
  int CreateRawProgram(char *szFFUFile, char *szFileName);
  int TerminateRawProgram(char *szFileName);
  int DumpRawProgram(char *szFFUFile, char *szRawProgram);
  int FFUDumpDisk(Protocol *proto);
  int AddEntryToRawProgram(char *szRawProgram, char *szFileName, uint64_t ui64FileOffset, int64_t i64StartSector, uint64_t ui64NumSectors);
  int ReadGPT(void);
  int ParseHeaders(void);
  uint64_t GetNextStartingArea(uint64_t chunkSizeInBytes, uint64_t sizeOfArea);

  // Headers found within FFU image
  SECURITY_HEADER FFUSecurityHeader;
  IMAGE_HEADER FFUImageHeader;
  STORE_HEADER FFUStoreHeader;
  unsigned char* ValidationEntries;
  BLOCK_DATA_ENTRY* BlockDataEntries;
  uint64_t PayloadDataStart;

  int hFFU;
  void* OvlRead;
  void* OvlWrite;

  // GPT Stuff
  unsigned char GptProtectiveMBR[512];
  gpt_header_t GptHeader;
  gpt_entry_t *GptEntries;
  int DISK_SECTOR_SIZE;

};


