/*****************************************************************************
 * firehose.cpp
 *
 * This class can perform emergency flashing given the transport layer
 *
 * Copyright (c) 2007-2013
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/firehose.cpp#20 $
$DateTime: 2015/05/07 21:41:17 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
02/06/13   pgw     Initial version.
=============================================================================*/

#include "stdio.h"
#include "tchar.h"
#include "firehose.h"
#include "xmlparser.h"
#include "partition.h"
#include <exception>

Firehose::~Firehose()
{
  if(m_payload != NULL ) {
    free(m_payload);
    m_payload = NULL;
  }
  if (program_pkt != NULL) {
	  free(program_pkt);
	  program_pkt = NULL;
  }
}

Firehose::Firehose(SerialPort *port,HANDLE hLogFile)
{
  // Initialize the serial port
  bSectorAddress = true;
  dwMaxPacketSize = 1024*1024;
  diskSectors = 0;
  hLog = hLogFile;
  sport = port;
  m_payload = NULL;
  program_pkt = NULL;
  m_buffer_len = 0;
  m_buffer_ptr = NULL;
}

int Firehose::ReadData(BYTE *pOutBuf, DWORD dwBufSize, bool bXML)
{
  DWORD dwBytesRead = 0;
  bool bFoundXML = false;
  int status = ERROR_SUCCESS;

  if (bXML) {
    memset(pOutBuf, 0, dwBufSize);
    // First check if there is data available in our buffer
    for (int i=0; i < 3;i++){
      for (; m_buffer_len > 0;m_buffer_len--) {
        *pOutBuf++ = *m_buffer_ptr++;
        dwBytesRead++;

        // Check for end of XML 
        if (strncmp(((char *)pOutBuf) - 7, "</data>", 7) == 0) {
          m_buffer_len--;
          bFoundXML = true;
          break;
        }
      }

      // If buffer length hits 0 means we didn't find end of XML so read more data
      if (!bFoundXML) {
        // Zero out the buffer to remove any other data and reset pointer
        memset(m_buffer, 0, dwMaxPacketSize);
        m_buffer_len = dwMaxPacketSize;
        m_buffer_ptr = m_buffer;
        if (sport->Read(m_buffer, &m_buffer_len) != ERROR_SUCCESS) {
          m_buffer_len = 0;
        }
      } else {
        break;
      }
    }
  } else {
    // First copy over any extra data that may be present from previous read
    dwBytesRead = dwBufSize;
    if (m_buffer_len > 0) {
      Log("Using %i bytes from mbuffer", m_buffer_len);
      // If there is more bytes available then we have space for then only copy out the bytes we need and return
      if (m_buffer_len >= dwBufSize) {
        m_buffer_len -= dwBufSize;
        memcpy_s(pOutBuf, dwBufSize, m_buffer_ptr, dwBufSize);
        m_buffer_ptr += dwBufSize;
        return dwBufSize;
      }

      // Need all the data so copy it all and read any more data that may be present
      memcpy_s(pOutBuf, dwBufSize, m_buffer_ptr, m_buffer_len);
      pOutBuf += m_buffer_len;
      dwBytesRead = dwBufSize - m_buffer_len;
    }

    status = sport->Read(pOutBuf, &dwBytesRead);
    dwBytesRead += m_buffer_len;
    m_buffer_len = 0;
  }

  if (status != ERROR_SUCCESS)
  {
    return status;
  } 
  
  return dwBytesRead;
}

int Firehose::ConnectToFlashProg(fh_configure_t *cfg)
{
  int status = ERROR_SUCCESS;
  DWORD dwBytesRead = dwMaxPacketSize;
  DWORD retry = 0;

  // Allocation our global buffers only once when connecting to flash programmer
  if (m_payload == NULL || program_pkt == NULL || m_buffer == NULL) {
    m_payload = (BYTE *)malloc(dwMaxPacketSize);
    program_pkt = (char *)malloc(MAX_XML_LEN);
    m_buffer = (BYTE *)malloc(dwMaxPacketSize);
    if (m_payload == NULL || program_pkt == NULL || m_buffer == NULL) {
      return ERROR_HV_INSUFFICIENT_MEMORY;
    }
  }

  // Read any pending data from the flash programmer
  memset(m_payload, 0, dwMaxPacketSize);
  dwBytesRead = ReadData((BYTE *)m_payload, dwMaxPacketSize, false);
  Log((char*)m_payload);

  // If this is UFS and didn't specify the disk_sector_size then update default to 4096
  if ((_stricmp(cfg->MemoryName, "ufs") == 0) && (DISK_SECTOR_SIZE == 512))
  {
    Log(L"Programming UFS device using SECTOR_SIZE=4096\n");
    DISK_SECTOR_SIZE = 4096;
  }
  else {
    Log(L"Programming device using SECTOR_SIZE=%i\n", DISK_SECTOR_SIZE);
  }
  

  sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version = \"1.0\" ?><data><configure MemoryName=\"%s\" ZLPAwareHost=\"%i\" SkipStorageInit=\"%i\" SkipWrite=\"%i\" MaxPayloadSizeToTargetInBytes=\"%i\"/></data>",
    cfg->MemoryName, cfg->ZLPAwareHost, cfg->SkipStorageInit, cfg->SkipWrite, dwMaxPacketSize);
  Log(program_pkt);
  status = sport->Write((BYTE *)program_pkt, strnlen_s(program_pkt,MAX_XML_LEN));
  if (status == ERROR_SUCCESS) {
    // Wait until we get the ACK or NAK back
    for (; retry < MAX_RETRY; retry++)
    {
      status = ReadStatus();
      if (status == ERROR_SUCCESS)
      {
        // Received an ACK to configure request we are good to go
        break;
      }
      else if (status == ERROR_INVALID_DATA)
      {
        // Received NAK to configure request check reason if it is MaxPayloadSizeToTarget then reduce and retry
        XMLParser xmlparse;
        UINT64 u64MaxSize = 0;
        // Make sure we got a configure response and set our max packet size
        xmlparse.ParseXMLInteger((char *)m_payload, "MaxPayloadSizeToTargetInBytes", &u64MaxSize);

        // If device can't handle a packet this large then change the the largest size they can use and reconfigure
        if ((u64MaxSize > 0) && (u64MaxSize  <  dwMaxPacketSize)) {
          dwMaxPacketSize = (UINT32)u64MaxSize;
          Log("We are decreasing our max packet size %i\n", dwMaxPacketSize);
          return ConnectToFlashProg(cfg);
        }
      }
    }

    if (retry == MAX_RETRY) 
    {
      Log(L"ERROR: No response to configure packet\n");
      return ERROR_NOT_READY;
    }
  }

  // read out any pending data 
  dwBytesRead = ReadData((BYTE *)m_payload, dwMaxPacketSize, false);
  Log((char *)m_payload);

  return status;
}

int Firehose::DeviceReset()
{
	int status = ERROR_SUCCESS;
  char reset_pkt[] = "<?xml version=\"1.0\" ?><data><power value=\"reset\"/></data>";
	status = sport->Write((BYTE *)reset_pkt, sizeof(reset_pkt));
	return status;
}

int Firehose::ReadStatus(void)
{
  // Make sure we read an ACK back
  while (ReadData(m_payload, MAX_XML_LEN, true) > 0)
  {
    // Check for ACK packet
    Log((char *)m_payload);
    if (strstr((char *)m_payload, "ACK") != NULL) {
      return ERROR_SUCCESS;
    }
    else if (strstr((char *)m_payload, "NAK") != NULL) {
      Log("\n---Target returned NAK---\n");
      return ERROR_INVALID_DATA;
    }
  }
  return ERROR_NOT_READY;
}

int Firehose::ProgramPatchEntry(PartitionEntry pe, TCHAR *key)
{
  UNREFERENCED_PARAMETER(pe);
  TCHAR tmp_key[MAX_STRING_LEN];
  size_t bytesOut;
  int status = ERROR_SUCCESS;
  
  // Make sure we get a valid parameter passed in
  if (key == NULL) return ERROR_INVALID_PARAMETER;
  wcscpy_s(tmp_key,key);
  
  memset(program_pkt,0,MAX_XML_LEN);
  sprintf_s(program_pkt,MAX_XML_LEN,"<?xml version=\"1.0\" ?><data>");
  StringReplace(tmp_key,L".",L"");
  StringReplace(tmp_key,L".",L"");
  wcstombs_s(&bytesOut,&program_pkt[strlen(program_pkt)],2000,tmp_key,MAX_XML_LEN);
  strcat_s(program_pkt,MAX_XML_LEN,"></data>\n");
  status= sport->Write((BYTE*)program_pkt,strlen(program_pkt));
  if( status != ERROR_SUCCESS ) return status;

  Log(program_pkt);
  // There is no response to patch requests

  return ReadStatus();
}

int Firehose::WriteData(BYTE *writeBuffer, __int64 writeOffset, DWORD writeBytes, DWORD *bytesWritten, UINT8 partNum)
{
  DWORD dwBytesRead;
  int status = ERROR_SUCCESS;

  // If we are provided with a buffer read the data directly into there otherwise read into our internal buffer
  if (writeBuffer == NULL || bytesWritten == NULL) {
    return ERROR_INVALID_PARAMETER;
  }

  *bytesWritten = 0;
  memset(program_pkt, 0, MAX_XML_LEN);
  if (writeOffset >= 0) {
    sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>\n"
      "<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"%i\"/>"
      "\n</data>", DISK_SECTOR_SIZE, (int)writeBytes/DISK_SECTOR_SIZE, partNum, (int)(writeOffset/DISK_SECTOR_SIZE));
  }
  else { // If start sector is negative write to back of disk
    sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>\n"
      "<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"NUM_DISK_SECTORS%i\"/>"
      "\n</data>", DISK_SECTOR_SIZE, (int)writeBytes / DISK_SECTOR_SIZE, partNum, (int)(writeOffset / DISK_SECTOR_SIZE));
  }
  status = sport->Write((BYTE*)program_pkt, strlen(program_pkt));
  Log((char *)program_pkt);

  // This should have log information from device
  // Get the response after read is done
  if (ReadStatus() != ERROR_SUCCESS) goto WriteSectorsExit;

  UINT64 ticks = GetTickCount64();
  //UINT64 dwBufSize = writeBytes;

  // loop through and write the data
  dwBytesRead = dwMaxPacketSize;
  for (DWORD i = 0; i < writeBytes; i += dwMaxPacketSize) {
    if ((writeBytes - i)  < dwMaxPacketSize) {
      dwBytesRead = (int)(writeBytes - i);
    }
    status = sport->Write(&writeBuffer[i], dwBytesRead);
    if (status != ERROR_SUCCESS) {
      goto WriteSectorsExit;
    }
    *bytesWritten += dwBytesRead;
    wprintf(L"Sectors remaining %i\r", (int)(writeBytes - i));
  }

  wprintf(L"\nDownloaded raw image size %i at speed %i KB/s\n", (int)writeBytes, (int)(((writeBytes / 1024) * 1000) / (GetTickCount64() - ticks + 1)));

  // Get the response after read is done
  status = ReadStatus();

  // Read and display any other log packets we may have

WriteSectorsExit:
  return status;
}

int Firehose::ReadData(BYTE *readBuffer, __int64 readOffset, DWORD readBytes, DWORD *bytesRead, UINT8 partNum)
{
  DWORD dwBytesRead;
  int status = ERROR_SUCCESS;

  // If we are provided with a buffer read the data directly into there otherwise read into our internal buffer
  if (readBuffer == NULL || bytesRead == NULL) {
    return ERROR_INVALID_PARAMETER;
  }

  memset(program_pkt, 0, MAX_XML_LEN);
  if (readOffset >= 0) {
    sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>\n"
      "<read SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"%i\"/>"
      "\n</data>", DISK_SECTOR_SIZE, (int)readBytes/DISK_SECTOR_SIZE, partNum, (int)(readOffset/DISK_SECTOR_SIZE));
  }
  else { // If start sector is negative read from back of disk
    sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>\n"
      "<read SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"NUM_DISK_SECTORS%i\"/>"
      "\n</data>", DISK_SECTOR_SIZE, (int)readBytes / DISK_SECTOR_SIZE, partNum, (int)(readOffset / DISK_SECTOR_SIZE));
  }
  status = sport->Write((BYTE*)program_pkt, strlen(program_pkt));
  Log((char *)program_pkt);

  // Wait until device returns with ACK or NAK
  while ((status = ReadStatus()) == ERROR_NOT_READY);
  if (status == ERROR_INVALID_DATA) goto ReadSectorsExit;

  UINT64 ticks = GetTickCount64();
  DWORD bytesToRead = dwMaxPacketSize;

  for (UINT32 tmp_sectors = (UINT32)readBytes/DISK_SECTOR_SIZE; tmp_sectors > 0; tmp_sectors -= (bytesToRead / DISK_SECTOR_SIZE)) {
    if (tmp_sectors < dwMaxPacketSize / DISK_SECTOR_SIZE) {
      bytesToRead = tmp_sectors*DISK_SECTOR_SIZE;
    }
    else {
      bytesToRead = dwMaxPacketSize;
    }

    DWORD offset = 0;
    while (offset < bytesToRead) {
      dwBytesRead = ReadData(&readBuffer[offset], bytesToRead - offset, false);
      offset += dwBytesRead;
    }

    // Now either write the data to the buffer or handle given
    readBuffer += bytesToRead;
    *bytesRead += bytesToRead;
    wprintf(L"Sectors remaining %i\r", (int)tmp_sectors);
  }

  wprintf(L"\nDownloaded raw image at speed %i KB/s\n", (int)(readBytes / (GetTickCount64() - ticks + 1)));

  // Get the response after read is done first response should be finished command
  status = ReadStatus();

ReadSectorsExit:
  return status;
}

int Firehose::CreateGPP(DWORD dwGPP1, DWORD dwGPP2, DWORD dwGPP3, DWORD dwGPP4)
{
  int status = ERROR_SUCCESS;

  sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>\n"
                        "<createstoragedrives DRIVE4_SIZE_IN_KB=\"%i\" DRIVE5_SIZE_IN_KB=\"%i\" DRIVE6_SIZE_IN_KB=\"%i\" DRIVE7_SIZE_IN_KB=\"%i\" />"
                        "</data>",dwGPP1,dwGPP2,dwGPP3,dwGPP4);
  
  status= sport->Write((BYTE*)program_pkt,strlen(program_pkt));
  Log((char *)program_pkt);
  
  // Read response
  status = ReadStatus();

  // Read and display any other log packets we may have
  if (ReadData(m_payload, dwMaxPacketSize, false) > 0) Log((char*)m_payload);

  return status;
}

int Firehose::SetActivePartition(int prtn_num)
{
  int status = ERROR_SUCCESS;

  sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>\n"
                        "<setbootablestoragedrive value=\"%i\" />"
                        "</data>\n",prtn_num);

  status= sport->Write((BYTE*)program_pkt,strlen(program_pkt));
  Log((char *)program_pkt);

  // Read response
  status = ReadStatus();

  // Read and display any other log packets we may have
  if (ReadData(m_payload, dwMaxPacketSize, false) > 0) Log((char*)m_payload);

  return status;
}

int Firehose::ProgramRawCommand(TCHAR *key)
{
  DWORD dwBytesRead;
  size_t bytesOut;
  int status = ERROR_SUCCESS;
  memset(program_pkt, 0, MAX_XML_LEN);
  sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>");
  wcstombs_s(&bytesOut, &program_pkt[strlen(program_pkt)], MAX_STRING_LEN, key, MAX_XML_LEN);
  strcat_s(program_pkt, MAX_XML_LEN, "></data>\n");
  status = sport->Write((BYTE*)program_pkt, strlen(program_pkt));
  Log("Programming RAW command: %s\n",(char *)program_pkt);

  // Read and log any response to command we sent
  dwBytesRead = ReadData(m_payload, dwMaxPacketSize, false);
  Log("%s\n",(char *)m_payload);

  return status;
}

int Firehose::FastCopy(HANDLE hRead, __int64 sectorRead, HANDLE hWrite, __int64 sectorWrite, uint64 sectors, UINT8 partNum)
{
  DWORD dwBytesRead = 0;
  BOOL bReadStatus = TRUE;
  __int64 dwWriteOffset = sectorWrite*DISK_SECTOR_SIZE;
  __int64 dwReadOffset = sectorRead*DISK_SECTOR_SIZE;
  int status = ERROR_SUCCESS;

  // If we are provided with a buffer read the data directly into there otherwise read into our internal buffer
  if (hWrite == NULL ) {
    return ERROR_INVALID_PARAMETER;
  }

  if (hRead == INVALID_HANDLE_VALUE) {
    wprintf(L"hRead = INVALID_HANDLE_VALUE, zeroing input buffer\n");
    memset(m_payload, 0, dwMaxPacketSize);
    dwBytesRead = dwMaxPacketSize;
  }
  else {
    // Move file pointer to the give location if value specified is > 0
    if (sectorRead > 0) {
      LONG sectorReadHigh = dwReadOffset >> 32;
      status = SetFilePointer(hRead, (LONG)dwReadOffset, &sectorReadHigh, FILE_BEGIN);
      if (status == INVALID_SET_FILE_POINTER) {
        status = GetLastError();
        wprintf(L"Failed to set offset 0x%I64x status: %i\n", sectorRead, status);
        return status;
      }
    }
  }

  // Determine if we are writing to firehose or reading from firehose
  memset(program_pkt, 0, MAX_XML_LEN);
  if (hWrite == hDisk){
    if (sectorWrite < 0){
      sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>\n"
        "<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"NUM_DISK_SECTORS%i\"/>"
        "\n</data>", DISK_SECTOR_SIZE, (int)sectors, partNum, (int)sectorWrite);
    }
    else
    {
      sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>\n"
        "<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"%i\"/>"
        "\n</data>", DISK_SECTOR_SIZE, (int)sectors, partNum, (int)sectorWrite);
    }
  }
  else
  {
    sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>\n"
      "<read SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"%i\"/>"
      "\n</data>", DISK_SECTOR_SIZE, (int)sectors, partNum, (int)sectorRead);
  }

  // Write out the command and wait for ACK/NAK coming back
  status = sport->Write((BYTE*)program_pkt, strlen(program_pkt));
  Log((char *)program_pkt);

  // Wait until device returns with ACK or NAK
  while ((status = ReadStatus()) == ERROR_NOT_READY);

  if (status == ERROR_SUCCESS)
  {
    DWORD bytesToRead;
    for (UINT32 tmp_sectors = (UINT32)sectors; tmp_sectors > 0; tmp_sectors -= (bytesToRead / DISK_SECTOR_SIZE)) {
      bytesToRead = dwMaxPacketSize;
      if (tmp_sectors < dwMaxPacketSize / DISK_SECTOR_SIZE) {
        bytesToRead = tmp_sectors*DISK_SECTOR_SIZE;
      }
      if (hWrite == hDisk)
      {
        // Writing to disk and reading from file...
        dwBytesRead = 0;
        if (hRead != INVALID_HANDLE_VALUE) {
          bReadStatus = ReadFile(hRead, m_payload, bytesToRead, &dwBytesRead, NULL);
        }
        if (bReadStatus) {
          status = sport->Write(m_payload, bytesToRead);
          if (status != ERROR_SUCCESS) {
            break;
          }
          dwWriteOffset += dwBytesRead;
        }
        else {
          // If there is partial data read out then write out next chunk to finish this up
          status = GetLastError();
          break;
        }

      }// Else this is a read command so read data from device in dwMaxPacketSize chunks
      else {
        DWORD offset = 0;
        while (offset < bytesToRead) {
          dwBytesRead = ReadData(&m_payload[offset], bytesToRead - offset, false);
          offset += dwBytesRead;
        }
        // Now either write the data to the buffer or handle given
        if (!WriteFile(hWrite, m_payload, bytesToRead, &dwBytesRead, NULL))  {
          // Failed to write out the data
          status = GetLastError();
          break;
        }
      }
      wprintf(L"Sectors remaining %i\r", (int)tmp_sectors);
    }
  }

  // Get the response after raw transfer is completed
  if (status == ERROR_SUCCESS) {
    status = ReadStatus();
  }

  return status;
}
