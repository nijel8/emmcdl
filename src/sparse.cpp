/*****************************************************************************
* sparse.cpp
*
* This class implements Sparse image support
*
* Copyright (c) 2007-2015
* Qualcomm Technologies Incorporated.
* All Rights Reserved.
* Qualcomm Confidential and Proprietary
*
*****************************************************************************/
/*=============================================================================
Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/sparse.cpp#2 $
$DateTime: 2015/04/01 17:01:45 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
02/05/15   pgw     Initial version.
=============================================================================*/

#include "stdio.h"
#include "stdlib.h"
#include "sparse.h"
#include "string.h"

// Constructor
SparseImage::SparseImage()
{
  bSparseImage = false;
  hSparseImage = -1;
}

// Destructor
SparseImage::~SparseImage()
{
  if (bSparseImage)
  {
    emmcdl_close(hSparseImage);
  }
}

// This will load a sparse image into memory and read headers if it is a sparse image
// RETURN: 
// 0 - Image and headers we loaded successfully
// 
int SparseImage::PreLoadImage(char *szSparseFile)
{
  //uint32_t_t dwBytesRead;
  hSparseImage = emmcdl_open(szSparseFile, O_RDONLY);
  if (hSparseImage < 0) return -ENOENT;

  // Load the sparse file header and verify it is valid
  if (emmcdl_read(hSparseImage, &SparseHeader, sizeof(SparseHeader)) >= 0) {
    // Check the magic number in the sparse header to see if this is a vaild sparse file
    if (SparseHeader.dwMagic != SPARSE_MAGIC) {
      emmcdl_close(hSparseImage);
      return -9;
    }
  }
  else {
    // Couldn't read the file likely someone else has it open for exclusive access
    emmcdl_close(hSparseImage);
    return errno;
  }

  bSparseImage = true;
  return 0;
}

int SparseImage::ProgramImage(Protocol *pProtocol, int64_t dwOffset)
{
  CHUNK_HEADER ChunkHeader;
  unsigned char *bpDataBuf;
  uint32_t dwBytesRead;
  uint32_t dwBytesOut;
  int status = 0;

  // Make sure we have first successfully found a sparse file and headers are loaded okay
  if (!bSparseImage)
  {
    return -EBADF;
  }

  // Allocated a buffer 

  // Main loop through all block entries in the sparse image
  for (uint32_t i=0; i < SparseHeader.dwTotalChunks; i++){
    // Read chunk header 
    if (emmcdl_read(hSparseImage, &ChunkHeader, sizeof(ChunkHeader))){
      if (ChunkHeader.wChunkType == SPARSE_RAW_CHUNK){
        uint32_t dwChunkSize = ChunkHeader.dwChunkSize*SparseHeader.dwBlockSize;
        // Allocate a buffer the size of the chunk and read in the data
        bpDataBuf = (unsigned char *)malloc(dwChunkSize);
        if (bpDataBuf == NULL){
          return -ENOMEM;
        }
        if (emmcdl_read(hSparseImage, bpDataBuf, dwChunkSize)) {
          // Now we have the data so use whatever protocol we need to write this out
          pProtocol->WriteData(bpDataBuf, dwOffset, dwChunkSize, &dwBytesOut,0);
          dwOffset += ChunkHeader.dwChunkSize;
        }
        else {
          status = errno;
          break;
        }
        free(bpDataBuf);
      }
      else if (ChunkHeader.wChunkType == SPARSE_FILL_CHUNK){
        // Fill chunk with data byte given for now just skip over this area
        dwOffset += ChunkHeader.dwChunkSize*SparseHeader.dwBlockSize;
      }
      else if (ChunkHeader.wChunkType == SPARSE_DONT_CARE){
        // Skip the specified number of bytes in the output file
        dwOffset += ChunkHeader.dwChunkSize*SparseHeader.dwBlockSize;
      }
      else {
        // We have no idea what type of chunk this is return a failure and close file
        status = ERROR_INVALID_DATA;
        break;
      }
    }
    else {
      // Failed to read data something is wrong with the file
      status = errno;
      break;
    }
  }

  // If we failed to load the file close the handle and set sparse image back to false
  if (status != 0) {
    bSparseImage = false;
    emmcdl_close(hSparseImage);
  }

  return status;
}

