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

#include <stdio.h>
#include <stdlib.h>
#include "firehose.h"
#include "xmlparser.h"
#include "partition.h"
#include <exception>
using namespace std;

Firehose::~Firehose()
{
  if(m_payload != NULL ) {
    free(m_payload);
    m_payload = NULL;
  }

  if (m_buffer != NULL) {
     free(m_buffer);
     m_buffer = NULL;
  }
  if (program_pkt != NULL) {
	  free(program_pkt);
	  program_pkt = NULL;
  }
}

Firehose::Firehose(SerialPort *port,uint32_t maxPacketSize, int hLogFile)
{
  // Initialize the serial port
  dwMaxPacketSize = maxPacketSize;
  diskSectors = 0;
  hLog = hLogFile;
  sport = port;
  m_payload = NULL;
  program_pkt = NULL;
  m_buffer_len = 0;
  m_buffer = NULL;
  m_buffer_ptr = NULL;
}

int Firehose::ReadData(unsigned char *pOutBuf, uint32_t dwBufSize, bool bXML)
{
  uint32_t dwBytesRead = 0;
  bool bFoundXML = false;
  int status = 0;

  if (bXML) {
    memset(pOutBuf, 0, dwBufSize);
    // First check if there is data available in our buffer
    for (int i=0; i < 3;i++){
      for (; m_buffer_len > 0 && dwBytesRead < dwBufSize;m_buffer_len--) {
        *pOutBuf++ = *m_buffer_ptr++;
        dwBytesRead++;

        // Check for end of XML 
        if (dwBytesRead >= 7 && (strncmp(((char *)pOutBuf) - 7, "</data>", 7) == 0)) {
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
        if ((status = sport->Read(m_buffer, &m_buffer_len)) <  0) {
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
        memcpy(pOutBuf, m_buffer_ptr, dwBufSize);
        m_buffer_ptr += dwBufSize;
        return dwBufSize;
      }

      // Need all the data so copy it all and read any more data that may be present
      memcpy(pOutBuf, m_buffer_ptr, m_buffer_len);
      pOutBuf += m_buffer_len;
      dwBytesRead = dwBufSize - m_buffer_len;
    }

    status = sport->Read(pOutBuf, &dwBytesRead);
    dwBytesRead += m_buffer_len;
    m_buffer_len = 0;
  }

  if (status != 0)
  {
    return status;
  } 
  
  return dwBytesRead;
}

int Firehose::ConnectToFlashProg(fh_configure_t *cfg)
{
  int status = 0;
  uint32_t dwBytesRead = dwMaxPacketSize;
  uint32_t retry = 0;

  printf("%s\n", __func__);
  // Allocation our global buffers only once when connecting to flash programmer
  if (m_payload == NULL || program_pkt == NULL || m_buffer == NULL) {
    m_payload = (unsigned char *)malloc(dwMaxPacketSize);
    program_pkt = (char *)malloc(MAX_XML_LEN);
    m_buffer = (unsigned char *)malloc(dwMaxPacketSize);
    if (m_payload == NULL || program_pkt == NULL || m_buffer == NULL) {
      return ENOMEM;
    }
  }

  // Read any pending data from the flash programmer
  memset(m_payload, 0, dwMaxPacketSize);
  //dwBytesRead = ReadData((unsigned char *)m_payload, dwMaxPacketSize, false);
  Log((char*)m_payload);

  // If this is UFS and didn't specify the disk_sector_size then update default to 4096
  if ((strcasecmp(cfg->MemoryName, "ufs") == 0) && (DISK_SECTOR_SIZE == 512))
  {
    Log("Programming UFS device using SECTOR_SIZE=4096\n");
    DISK_SECTOR_SIZE = 4096;
  }
  else {
    Log("Programming device using SECTOR_SIZE=%i\n", DISK_SECTOR_SIZE);
  }
  

  sprintf(program_pkt, "<?xml version = \"1.0\" ?><data><configure MemoryName=\"%s\" ZLPAwareHost=\"%i\" SkipStorageInit=\"%i\" SkipWrite=\"%i\" MaxPayloadSizeToTargetInBytes=\"%i\" AckRawDataEveryNumPackets=\"%i\"/></data>",
    cfg->MemoryName, cfg->ZLPAwareHost, cfg->SkipStorageInit, cfg->SkipWrite, dwMaxPacketSize, cfg->AckRawDataEveryNumPackets);
  Log(program_pkt);
  status = sport->Write((unsigned char *)program_pkt, strlen(program_pkt));
  if (status == 0) {
    // Wait until we get the ACK or NAK back
    for (; retry < MAX_RETRY; retry++)
    {
      status = ReadStatus();
      if (status == 0)
      {
        // Received an ACK to configure request we are good to go
        break;
      }
      else if (status == ERROR_INVALID_DATA)
      {
        // Received NAK to configure request check reason if it is MaxPayloadSizeToTarget then reduce and retry
        XMLParser xmlparse;
        uint64_t u64MaxSize = 0;
        // Make sure we got a configure response and set our max packet size
        xmlparse.ParseXMLInteger((char *)m_payload, "MaxPayloadSizeToTargetInBytes", &u64MaxSize);

        // If device can't handle a packet this large then change the the largest size they can use and reconfigure
        if ((u64MaxSize > 0) && (u64MaxSize  <  dwMaxPacketSize)) {
          dwMaxPacketSize = (uint32_t)u64MaxSize;
          Log("We are decreasing our max packet size %i\n", dwMaxPacketSize);
          return ConnectToFlashProg(cfg);
        }
      }
    }

    if (retry == MAX_RETRY) 
    {
      Log("ERROR: No response to configure packet\n");
      return EBUSY;
    }
  }

  // read out any pending data 
  //dwBytesRead = ReadData((unsigned char *)m_payload, dwMaxPacketSize, false);

  Log((char *)m_payload);

  return status;
}

int Firehose::DeviceReset()
{
	int status = 0;
    char reset_pkt[] = "<?xml version=\"1.0\" ?><data><power value=\"reset\"/></data>";
	status = sport->Write((unsigned char *)reset_pkt, sizeof(reset_pkt));
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
    	char *ptr = strstr((char *)m_payload, "rawmode");
    	if (ptr != NULL ){
    		m_read_back_verify = false;
    		if (strstr(ptr, "true") != NULL) {
    		    m_rawmode = true;
    		} else {
    			m_rawmode = false;
    		}
    	} else {
    		m_read_back_verify = true;
    	}
      return 0;
    }
    else if (strstr((char *)m_payload, "NAK") != NULL) {
      Log("\n---Target returned NAK---\n");
      return ERROR_INVALID_DATA;
    }
  }
  return EBUSY;
}

int Firehose::ProgramPatchEntry(PartitionEntry pe, char *key)
{
  char tmp_key[MAX_STRING_LEN];
  size_t bytesOut;
  int status = 0;
  
  // Make sure we get a valid parameter passed in
  if (key == NULL) return EINVAL;

  strcpy(tmp_key,key);
  memset(program_pkt,0,MAX_XML_LEN);
  sprintf(program_pkt,"<?xml version=\"1.0\" ?><data>");
  const XMLParser xmlParser;
  xmlParser.StringReplace(tmp_key,".","");
  xmlParser.StringReplace(tmp_key,".","");
  strncpy(&program_pkt[strlen(program_pkt)],tmp_key,MAX_STRING_LEN);
  strcat(program_pkt,"></data>\n");
  status= sport->Write((unsigned char*)program_pkt,strlen(program_pkt));
  if( status != 0 ) return status;

  Log(program_pkt);
  // There is no response to patch requests

  return ReadStatus();
}

int Firehose::WriteData(unsigned char *writeBuffer, int64_t writeOffset, uint32_t writeBytes, uint32_t *bytesWritten, uint8_t partNum)
{
  uint32_t dwBytesRead;
  int status = 0;

  // If we are provided with a buffer read the data directly into there otherwise read into our internal buffer
  if (writeBuffer == NULL || bytesWritten == NULL) {
    return EINVAL;
  }

  *bytesWritten = 0;
  memset(program_pkt, 0, MAX_XML_LEN);
  if (writeOffset >= 0) {
    sprintf(program_pkt, "<?xml version=\"1.0\" ?><data>\n"
      "<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"%i\"/>"
      "\n</data>", DISK_SECTOR_SIZE, (int)writeBytes/DISK_SECTOR_SIZE, partNum, (int)(writeOffset/DISK_SECTOR_SIZE));
  }
  else { // If start sector is negative write to back of disk
    sprintf(program_pkt, "<?xml version=\"1.0\" ?><data>\n"
      "<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"NUM_DISK_SECTORS%i\"/>"
      "\n</data>", DISK_SECTOR_SIZE, (int)writeBytes / DISK_SECTOR_SIZE, partNum, (int)(writeOffset / DISK_SECTOR_SIZE));
  }
  status = sport->Write((unsigned char*)program_pkt, strlen(program_pkt));
  Log((char *)program_pkt);

  // This should have log information from device
  // Get the response after read is done
  if (ReadStatus() != 0) return status;

  struct timespec ts;
  int ret;

  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret < 0) {
      return ret;
  }

  uint64_t ticks =  ts.tv_sec * 1000;

  //uint64_t dwBufSize = writeBytes;

  // loop through and write the data
  dwBytesRead = dwMaxPacketSize;
  for (uint32_t i = 0; i < writeBytes; i += dwMaxPacketSize) {
    if ((writeBytes - i)  < dwMaxPacketSize) {
      dwBytesRead = (int)(writeBytes - i);
    }
    status = sport->Write(&writeBuffer[i], dwBytesRead);
    if (status != 0) {
      return status;
    }
    *bytesWritten += dwBytesRead;
    printf("Sectors remaining %i\r", (int)(writeBytes - i));
  }

  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret < 0) {
      return ret;
  }
  printf("\nDownloaded raw image size %i at speed %i KB/s\n", (int)writeBytes, (int)(((writeBytes / 1024) * 1000) / (ts.tv_sec*1000 - ticks + 1)));

  // Get the response after read is done
  status = ReadStatus();

  // Read and display any other log packets we may have

  return status;
}

int Firehose::ReadData(unsigned char *readBuffer, int64_t readOffset, uint32_t readBytes, uint32_t *bytesRead, uint8_t partNum)
{
  uint32_t dwBytesRead;
  int status = 0;

  // If we are provided with a buffer read the data directly into there otherwise read into our internal buffer
  if (readBuffer == NULL || bytesRead == NULL) {
    return EINVAL;
  }

  memset(program_pkt, 0, MAX_XML_LEN);
  if (readOffset >= 0) {
    sprintf(program_pkt, "<?xml version=\"1.0\" ?><data>\n"
      "<read SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"%i\"/>"
      "\n</data>", DISK_SECTOR_SIZE, (int)readBytes/DISK_SECTOR_SIZE, partNum, (int)(readOffset/DISK_SECTOR_SIZE));
  }
  else { // If start sector is negative read from back of disk
    sprintf(program_pkt, "<?xml version=\"1.0\" ?><data>\n"
      "<read SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"NUM_DISK_SECTORS%i\"/>"
      "\n</data>", DISK_SECTOR_SIZE, (int)readBytes / DISK_SECTOR_SIZE, partNum, (int)(readOffset / DISK_SECTOR_SIZE));
  }
  status = sport->Write((unsigned char*)program_pkt, strlen(program_pkt));
  Log((char *)program_pkt);

  // Wait until device returns with ACK or NAK
  while ((status = ReadStatus()) == EBUSY);
  if (status == ERROR_INVALID_DATA) return status;

  struct timespec ts;
  int ret;

  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret < 0) {
      return ret;
  }
  uint64_t ticks = ts.tv_sec * 1000;
  uint32_t bytesToRead = dwMaxPacketSize;

  for (uint32_t tmp_sectors = (uint32_t)readBytes/DISK_SECTOR_SIZE; tmp_sectors > 0; tmp_sectors -= (bytesToRead / DISK_SECTOR_SIZE)) {
    if (tmp_sectors < dwMaxPacketSize / DISK_SECTOR_SIZE) {
      bytesToRead = tmp_sectors*DISK_SECTOR_SIZE;
    }
    else {
      bytesToRead = dwMaxPacketSize;
    }

    uint32_t offset = 0;
    while (offset < bytesToRead) {
      dwBytesRead = ReadData(&readBuffer[offset], bytesToRead - offset, false);
      offset += dwBytesRead;
    }

    // Now either write the data to the buffer or handle given
    readBuffer += bytesToRead;
    *bytesRead += bytesToRead;
    printf("Sectors remaining %i\r", (int)tmp_sectors);
  }

  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret < 0) {
      return ret;
  }
  printf("\nDownloaded raw image at speed %i KB/s\n", (int)(((readBytes*1000)/1024) / (ts.tv_sec*1000 - ticks + 1)));

  // Get the response after read is done first response should be finished command
  status = ReadStatus();

  return status;
}

int Firehose::CreateGPP(uint32_t dwGPP1, uint32_t dwGPP2, uint32_t dwGPP3, uint32_t dwGPP4)
{
  int status = 0;

  sprintf(program_pkt, "<?xml version=\"1.0\" ?><data>\n"
                        "<createstoragedrives DRIVE4_SIZE_IN_KB=\"%i\" DRIVE5_SIZE_IN_KB=\"%i\" DRIVE6_SIZE_IN_KB=\"%i\" DRIVE7_SIZE_IN_KB=\"%i\" />"
                        "</data>",dwGPP1,dwGPP2,dwGPP3,dwGPP4);
  
  status= sport->Write((unsigned char*)program_pkt,strlen(program_pkt));
  Log((char *)program_pkt);
  
  // Read response
  status = ReadStatus();

  // Read and display any other log packets we may have
  if (ReadData(m_payload, dwMaxPacketSize, false) > 0) Log((char*)m_payload);

  return status;
}

int Firehose::SetActivePartition(int prtn_num)
{
  int status = 0;

  sprintf(program_pkt, "<?xml version=\"1.0\" ?><data>\n"
                        "<setbootablestoragedrive value=\"%i\" />"
                        "</data>\n",prtn_num);

  status= sport->Write((unsigned char*)program_pkt,strlen(program_pkt));
  Log((char *)program_pkt);

  // Read response
  status = ReadStatus();

  // Read and display any other log packets we may have
  if (ReadData(m_payload, dwMaxPacketSize, false) > 0) Log((char*)m_payload);

  return status;
}

int Firehose::PeekLogBuf(int64_t start, int64_t size)
{
    char peek[512];
    int status;
    unsigned char *logbuf = (unsigned char *)malloc(MAX_XML_LEN);
    memset(logbuf, 0, MAX_XML_LEN);
    printf("%s\n", __func__);
    sprintf(peek,"<?xml version=\"1.0\" ?><data>"
    		"<peek SizeInBytes=\"%ld\" address64=\"%ld\"/>"
    		"</data>\n"
    		, start, size);
    status = sport->Write((unsigned char*)peek, strlen(peek));
    Log("peek command: %s\n",(char *)peek);

    // Read and log any response to command we sent
    status = ReadData(logbuf, MAX_XML_LEN, false);
    Log("%s\n",(char *)logbuf);
    free(logbuf);
    return status;
}

int Firehose::ProgramRawCommand(char *key)
{
  uint32_t dwBytesRead;
  size_t bytesOut;
  int status = 0;
  memset(program_pkt, 0, MAX_XML_LEN);
  sprintf(program_pkt, "<?xml version=\"1.0\" ?><data>");
  strncpy(&program_pkt[strlen(program_pkt)], key, MAX_XML_LEN - strlen(program_pkt));
  strcat(program_pkt, "></data>\n");
  status = sport->Write((unsigned char*)program_pkt, strlen(program_pkt));
  Log("Programming RAW command: %s\n",(char *)program_pkt);

  // Read and log any response to command we sent
  dwBytesRead = ReadData(m_payload, dwMaxPacketSize, false);
  Log("%s\n",(char *)m_payload);

  return status;
}

int Firehose::FastCopy(int hRead, int64_t sectorRead, int hWrite, int64_t sectorWrite, __uint64_t sectors, uint8_t partNum)
{
  size_t dwBytesRead = 0;
  bool bReadStatus = true;
  int64_t dwWriteOffset = sectorWrite*DISK_SECTOR_SIZE;
  int64_t dwReadOffset = sectorRead*DISK_SECTOR_SIZE;
  int status = 0;

  // If we are provided with a buffer read the data directly into there otherwise read into our internal buffer
  if (hWrite < 0 ) {
    return EINVAL;
  }

  if (hRead < 0) {
    printf("hRead = INVALID_HANDLE_VALUE, zeroing input buffer\n");
    memset(m_payload, 0, dwMaxPacketSize);
    dwBytesRead = dwMaxPacketSize;
  }
  else {
    // Move file pointer to the give location if value specified is > 0
    if (sectorRead > 0) {
      //int64_t sectorReadHigh = dwReadOffset >> 32;
      //status = SetFilePointer(hRead, (LONG)dwReadOffset, &sectorReadHigh, FILE_BEGIN);
      status = emmcdl_lseek(hRead,dwReadOffset, SEEK_SET);
      if (status < 0) {
        status = errno;
        printf("Failed to set offset 0x%lx status: %i\n", sectorRead, status);
        return status;
      }
    }
  }

  // Determine if we are writing to firehose or reading from firehose
  memset(program_pkt, 0, MAX_XML_LEN);
  if (hWrite == hDisk){
    if (sectorWrite < 0){
      sprintf(program_pkt,  "<?xml version=\"1.0\" ?><data>\n"
        "<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"NUM_DISK_SECTORS%i\"/>"
        "\n</data>", DISK_SECTOR_SIZE, (int)sectors, partNum, (int)sectorWrite);
    }
    else
    {
      sprintf(program_pkt,  "<?xml version=\"1.0\" ?><data>\n"
        "<program SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"%i\"/>"
        "\n</data>", DISK_SECTOR_SIZE, (int)sectors, partNum, (int)sectorWrite);
    }
  }
  else
  {
    sprintf(program_pkt,  "<?xml version=\"1.0\" ?><data>\n"
      "<read SECTOR_SIZE_IN_BYTES=\"%i\" num_partition_sectors=\"%i\" physical_partition_number=\"%i\" start_sector=\"%i\"/>"
      "\n</data>", DISK_SECTOR_SIZE, (int)sectors, partNum, (int)sectorRead);
  }

  // Write out the command and wait for ACK/NAK coming back
  status = sport->Write((unsigned char*)program_pkt, strlen(program_pkt));
  Log((char *)program_pkt);

  // Wait until device returns with ACK or NAK
  while ((status = ReadStatus()) == EBUSY);

  if (status == 0)
  {
    uint32_t bytesToRead;
    for (int64_t tmp_sectors = (int64_t)sectors; tmp_sectors > 0; tmp_sectors -= (bytesToRead / DISK_SECTOR_SIZE)) {
      bytesToRead = dwMaxPacketSize&(~(DISK_SECTOR_SIZE - 1));
      if (tmp_sectors < dwMaxPacketSize / DISK_SECTOR_SIZE) {
        bytesToRead = tmp_sectors*DISK_SECTOR_SIZE;
      }
      if (hWrite == hDisk)
      {
        // Writing to disk and reading from file...
        dwBytesRead = 0;
        if (hRead != -1) {
			uint32_t curpos = 0;
        	do {
				dwBytesRead = emmcdl_read(hRead, m_payload+curpos, bytesToRead - curpos);
				if (dwBytesRead < 0 && errno == EAGAIN) {
					// Read not complete
					printf("\n%s EAGAIN\n", __func__);
					continue;
				} else if (dwBytesRead > 0) {
					curpos += dwBytesRead;
				}
        	} while(dwBytesRead < 0);
        }

        if (dwBytesRead >= 0) {
          status = sport->Write(m_payload, bytesToRead);
          if (status != 0) {
            break;
          }
          dwWriteOffset += dwBytesRead;
          if (sport->InputBufferCount() > 0) {
                 Log("\n");
                 status =  ReadStatus();
        	  if (status == EBUSY || status == 0) {
        	      continue;
        	  } else {
            	  status = ERROR_INVALID_DATA;
            	  break;
        	  }
            }
        }
        else {
          // If there is partial data read out then write out next chunk to finish this up
          status = errno;
          break;
        }

      }// Else this is a read command so read data from device in dwMaxPacketSize chunks
      else {
        uint32_t offset = 0;
        while (offset < bytesToRead) {
          dwBytesRead = ReadData(&m_payload[offset], bytesToRead - offset, false);
          offset += dwBytesRead;
        }
        // Now either write the data to the buffer or handle given
        if (!emmcdl_write(hWrite, m_payload, bytesToRead))  {
          // Failed to write out the data
          status = errno;
          break;
        }
      }
      printf("Sectors remaining %8i\r", (int)(tmp_sectors - (bytesToRead / DISK_SECTOR_SIZE)));
      //emmcdl_sleep_ms(10);
    }
  }

  // Get the response after raw transfer is completed
  if (status == 0) {
    status = ReadStatus();
  }

  return status;
}
