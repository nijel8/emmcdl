/*****************************************************************************
* sparse.h
*
* This file has structures and functions for sparse format
*
* Copyright (c) 2007-2015
* Qualcomm Technologies Incorporated.
* All Rights Reserved.
* Qualcomm Confidential and Proprietary
*
*****************************************************************************/
/*=============================================================================
Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/sparse.h#1 $
$DateTime: 2015/02/10 11:56:41 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
02/05/15   pgw     Initial version.
=============================================================================*/
#pragma once
#include "protocol.h"
#include <windows.h>

#define SPARSE_MAGIC      0xED26FF3A
#define SPARSE_RAW_CHUNK  0xCAC1
#define SPARSE_FILL_CHUNK 0xCAC2
#define SPARSE_DONT_CARE  0xCAC3

// Security Header struct. The first data read in from the FFU.
typedef struct _SPARSE_HEADER
{
  UINT32 dwMagic;           // 0xed26ff3a
  UINT16 wVerMajor;         // 0x1 Major version, reject version with higher major version
  UINT16 wVerMinor;         // 0x0 Minor version, accept version with higher minor version
  UINT16 wSparseHeaderSize; // 28 bytes for first revision of the file format
  UINT16 wChunkHeaderSize;  // 12 bytes for first revision of the file format
  UINT32 dwBlockSize;       // 4096 block size in bytes must be multiple of 4
  UINT32 dwTotalBlocks;     // Total blocks in the non-sparse output file
  UINT32 dwTotalChunks;     // Total chunks in the sparse input image
  UINT32 dwImageChecksum;   // CRC32 checksum of the original data counting do not care 802.3 polynomial
} SPARSE_HEADER;

typedef struct _CHUNK_HEADER
{
  UINT16 wChunkType;        // 0xCAC1 -> raw; 0xCAC2 -> fill; 0xCAC3 -> don't care
  UINT16 wReserved;         // Reserved should be all 0
  UINT32 dwChunkSize;       // Number of blocks this chunk takes in output image
  UINT32 dwTotalSize;       // Number of bytes in input file including chunk header round up to next block size
} CHUNK_HEADER;

class SparseImage {
public:
  int PreLoadImage(TCHAR *szSparseFile);
  int ProgramImage(Protocol *pProtocol, __int64 dwOffset);

  SparseImage();
  ~SparseImage();

private:
  SPARSE_HEADER SparseHeader;
  HANDLE hSparseImage;
  bool bSparseImage;

};