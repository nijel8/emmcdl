/*****************************************************************************
 * partition.cpp
 *
 * This class can perform emergency flashing given the transport layer
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/dload.cpp#5 $
$DateTime: 2015/04/29 17:06:00 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
11/08/11   pgw     Initial version.
=============================================================================*/

#include "tchar.h"
#include "dload.h"
#include "partition.h"
#include "diskwriter.h"

//using namespace std;

void Dload::HexToByte(const char *hex, BYTE *bin, int len)
{
  for(int i=0; i < len; i++) {
   BYTE val1 = hex[i*2] - '0';
   BYTE val2 = hex[i*2+1] - '0';
   if( val1 > 9 ) val1 = val1 - 7;
   if( val2 > 9 ) val2 = val2 - 7;
   bin[i] = (val1 << 4) + val2;
  }
}

Dload::Dload(SerialPort *port)
{
  // Initialize the serial port
  bSectorAddress = false;
  sport = port;
}

int Dload::ConnectToFlashProg(BYTE ver)
{
  BYTE hello[] = {EHOST_HELLO_REQ,'Q','C','O','M',' ','f','a','s','t',' ','d','o','w','n','l','o','a','d',' ','p','r','o','t','o','c','o','l',' ','h','o','s','t',ver,2,1};
  BYTE security[] = {EHOST_SECURITY_REQ,0};
  BYTE rsp[1060];
  int rspSize = sizeof(rsp);
  int status = ERROR_SUCCESS; 
  int i=0;

  if( ver > 2 ) {
    hello[35] |= FEATURE_SECTOR_ADDRESSES;
    bSectorAddress = true;
  }

  // For hello and security packets send 10 times each with 500ms delay
  // When device reconnects initially connection may take a while and be flakey
  for(; i < 10; i++) {
    rspSize = sizeof(rsp);
    status = sport->SendSync(hello,sizeof(hello),rsp,&rspSize);
    if( (status == ERROR_SUCCESS) && (rspSize > 0) && (rsp[0] == EHOST_HELLO_RSP)) {
      break;
    }
    sport->Flush();
    Sleep(500);
  }

  // Clear out any stale data so we start on a new buffer
  sport->Flush();
  if( i == 10 ) {
    return ERROR_INVALID_HANDLE;
  }


  printf("Got hello response\n");
  for(; i < 10; i++) { 
    rspSize = sizeof(rsp);
    sport->SendSync(security,sizeof(security),rsp,&rspSize);
    if( (rspSize > 0) && (rsp[0] == EHOST_SECURITY_RSP)) {
      return ERROR_SUCCESS;
    }
    sport->Flush();
    Sleep(500);
  }

  return ERROR_INVALID_HANDLE;
}

int Dload::DeviceReset()
{
  BYTE reset[] = {EHOST_RESET_REQ};
  BYTE rsp[32];
  int rspSize;

  rspSize = sizeof(rsp);
  sport->SendSync(reset,sizeof(reset),rsp,&rspSize);
  if( (rspSize == 0) || (rsp[0] != EHSOT_RESET_ACK)) {
    return ERROR_INVALID_HANDLE;
  }

  return ERROR_SUCCESS;
}

int Dload::LoadPartition(TCHAR *szPrtnFile)
{
  HANDLE hFile;
  int status = ERROR_SUCCESS;
  BYTE rsp[32];
  DWORD bytesRead=0;
  int rspSize;
  BYTE partition[1040] = {EHOST_PARTITION_REQ,0};
  
  hFile = CreateFile(szPrtnFile,GENERIC_READ, FILE_SHARE_READ,NULL,OPEN_EXISTING,NULL,NULL);
  if( hFile == INVALID_HANDLE_VALUE ) return ERROR_INVALID_HANDLE;

  // Download the partition table as one large chunk
  if(!ReadFile(hFile,&partition[2],1024,&bytesRead,NULL) ) {
    status = GetLastError();
  }

  if( bytesRead > 0 ) {
    rspSize = sizeof(rsp);
    sport->SendSync(partition,bytesRead+2,rsp,&rspSize);
    if( (rspSize == 0) || (rsp[0] != EHOST_PARTITION_RSP) ) {
      status = ERROR_WRITE_FAULT;
    }
  }
  CloseHandle(hFile);

  if( status == ERROR_HANDLE_EOF) status = ERROR_SUCCESS;
  return status;
}

int Dload::OpenPartition(int partition)
{
  BYTE openmulti[] = {EHOST_OPEN_MULTI_REQ,(BYTE)partition};
  BYTE rsp[32];
  int rspSize;

  // Flush out any previous transactions
  sport->Flush();
  
  // Open the partition
  rspSize = sizeof(rsp);
  sport->SendSync(openmulti,sizeof(openmulti),rsp,&rspSize);
  if( (rspSize == 0) || (rsp[0] != EHOST_OPEN_MULTI_RSP)) {
    wprintf(L"Could not open partition\n");
    return ERROR_INVALID_HANDLE;
  }

  return ERROR_SUCCESS;
}

int Dload::ClosePartition()
{
  BYTE close[] = {EHOST_CLOSE_REQ};
  BYTE rsp[32];
  int rspSize;
  
  // Close the partition we were writing to
  rspSize = sizeof(rsp);
  sport->SendSync(close,sizeof(close),rsp,&rspSize);
  if( (rspSize == 0) || (rsp[0] != EHOST_CLOSE_RSP)) {
    return ERROR_INVALID_HANDLE;
  }
  return ERROR_SUCCESS;
}

int Dload::FastCopySerial(HANDLE hInFile, DWORD offset, DWORD sectors)
{
  int status = ERROR_SUCCESS;
  BYTE rsp[32];
  DWORD bytesRead=0;
  DWORD count = 0;
  DWORD readSize = 1024;
  int rspSize;
  BYTE streamwrite[1050] = {EHOST_STREAM_WRITE_REQ,0};

  // For now don't worry about sliding window do a write and wait for response
  while( (status == ERROR_SUCCESS) && (count < sectors)) {
    if( (count + 2) > sectors) {
      readSize = 512;
    }
    // If HANDLE value is invalid then just write 0's
    if( hInFile != INVALID_HANDLE_VALUE ) {
      if(!ReadFile(hInFile,&streamwrite[5],readSize,&bytesRead,NULL) ) {
        status = GetLastError();
      }
    } else {
      memset(&streamwrite[5],0,readSize);
	  bytesRead = readSize;
    }

    count += (bytesRead/512);
    if( bytesRead > 0 ) {
      // Write out the bytes we read to destination
      streamwrite[1] = offset & 0xff;
      streamwrite[2] = (offset >> 8) & 0xff;
      streamwrite[3] = (offset >> 16) & 0xff;
      streamwrite[4] = (offset >> 24) & 0xff;
      rspSize = sizeof(rsp);

      sport->SendSync(streamwrite,bytesRead+5,rsp,&rspSize);
      if( (rspSize == 0) || (rsp[0] != EHOST_STREAM_WRITE_RSP) ) {
        wprintf(L"Device returned error: %i\n",rsp[0]);
        if( rsp[0] == EHOST_LOG && rspSize >= 2) {
          rsp[rspSize-2] = 0;
          printf((char *)&rsp[1]);
        }
        status = ERROR_WRITE_FAULT;
      }
      if( bSectorAddress ) {
        offset += bytesRead/SECTOR_SIZE;
        wprintf(L"Destination sector: %i\r",(int)offset);
      } else {
        offset += bytesRead;
        wprintf(L"Destination offset: %i\r",(int)offset);
      }
    } else {
      // If we didn't read anything then break
      break;
    }
  }

  printf("\n");

  // If we hit end of file that means we sent it all
  if( status == ERROR_HANDLE_EOF) status = ERROR_SUCCESS;
  return status;
}

int Dload::LoadImage(TCHAR *szSingleImage)
{
  HANDLE hFile;
  int status = ERROR_SUCCESS;

  hFile = CreateFile(szSingleImage,GENERIC_READ, FILE_SHARE_READ,NULL,OPEN_EXISTING,NULL,NULL);
  if( hFile == INVALID_HANDLE_VALUE ) {
    wprintf(L"Filename not found %s\n",szSingleImage);
    return ERROR_INVALID_HANDLE;
  }

  status = FastCopySerial(hFile,0,(DWORD)-1);
  CloseHandle(hFile);

  return status;
}

int Dload::LoadFlashProg(TCHAR *szFlashPrg)
{
  BYTE write32[280] = {CMD_WRITE_32BIT, 0};
  BYTE *writePtr;
  BYTE rsp[32];
  int rspSize;
  DWORD goAddr = 0;
  DWORD targetAddr=0;
  DWORD newAddr=0;
  DWORD bytesRead;
  DWORD status = ERROR_SUCCESS;

  char hexline[128];
  FILE *fMprgFile;
  _wfopen_s(&fMprgFile,szFlashPrg,L"r");

  if( fMprgFile == NULL ) {
    return ERROR_OPEN_FAILED;
  }

  while( !feof(fMprgFile) ) {
    BYTE id=0;
    bool bFirst = true;
    writePtr = &write32[7];
    // Read in data till we collect 256 bytes or hit end of file
    for(bytesRead=0; bytesRead < 256;) {
      BYTE len;
      fgets(hexline,sizeof(hexline),fMprgFile);
      if( strlen(hexline) > 9 ) {
        HexToByte(&hexline[1],&len,1);
        HexToByte(&hexline[3],rsp,2);
        HexToByte(&hexline[7],&id,1);
            
        // Only read in data chunks
        if( id == 0) {
          HexToByte(&hexline[9],writePtr,len);
          if( bFirst ) {
            bFirst = false;
            targetAddr = (targetAddr & 0xffff0000) + (rsp[0] << 8) + rsp[1];
          }
          writePtr += len;
          bytesRead += len;
        } else if( id == 1) {
          // End of file marker
          break;
        } else if( id == 5) {
          // File execute address
          HexToByte(&hexline[9],rsp,4);
          goAddr = (rsp[0] << 24) + (rsp[1] << 16) + (rsp[2] << 8) + rsp[3];
        } else if( id == 4) {
          // update the upper address bytes and write out chunk of last data
          HexToByte(&hexline[9],rsp,2);
          newAddr = (rsp[0] << 24) + (rsp[1] << 16);
          break;
        }
      }

      // If we got new address then break;
      if( id == 4 ) {
        break;
      }
    }


    if( bytesRead > 0 ) {
      // Collected 256 bytes program it
      write32[1] = (targetAddr >> 24) & 0xff;
      write32[2] = (targetAddr >> 16) & 0xff;
      write32[3] = (targetAddr >> 8) & 0xff;
      write32[4] = targetAddr & 0xff;
      write32[5] = (bytesRead >> 8) & 0xff;
      write32[6] = bytesRead & 0xff;
      rspSize = sizeof(rsp);
      //wprintf(L"Program at: 0x%x length %i\n",targetAddr,bytesRead);
      sport->SendSync(write32,bytesRead+7,rsp,&rspSize);
      if( (rspSize == 0) || (rsp[0] != CMD_ACK) ) {
        status = ERROR_WRITE_FAULT;
        break;
      }
    }

    targetAddr = newAddr;
  }
  fclose(fMprgFile);
  

  if( status == ERROR_SUCCESS ) {
    BYTE gocmd[5] = {CMD_GO,0};
    printf("sending go command 0x%x\n", (UINT32)goAddr);
    gocmd[1] = (goAddr >> 24) & 0xff;
    gocmd[2] = (goAddr >> 16) & 0xff;
    gocmd[3] = (goAddr >> 8) & 0xff;
    gocmd[4] = goAddr & 0xff;
  
    // Send GO command if we successfully uploaded to end of file
    rspSize = sizeof(rsp);
    sport->SendSync(gocmd,sizeof(gocmd),rsp,&rspSize);
    if( (rspSize > 0) && (rsp[0] == CMD_ACK)) {
      status = ERROR_SUCCESS;
    } else {
      status = ERROR_WRITE_FAULT;
    }
  }
  
  return status;
}

int Dload::GetDloadParams(BYTE *rsp, int len)
{
  BYTE params[] = {CMD_PREQ};
  int bytesRead = len;
  // For params response we need at least 256 bytes
  if( len < 16 ) {
    return -1;
  }
  sport->SendSync(params,sizeof(params),rsp,&bytesRead);
  if( (bytesRead > 0) && (rsp[0] == CMD_PARAMS) ) {
    return ERROR_SUCCESS;
  }
  return -2;
}

int Dload::IsDeviceInDload(void)
{
  BYTE nop[] = {CMD_NOP};
  BYTE rsp[32] = {0};
  int bytesRead = 32;

  sport->SendSync(nop,sizeof(nop),rsp,&bytesRead);
  if( rsp[0] == CMD_ACK ) {
    return ERROR_SUCCESS;
  }
  
  return ERROR_INVALID_HANDLE;
}

int Dload::SetActivePartition()
{
  // Set currently open partition to active
  BYTE active_prtn[38] = {EHOST_STREAM_DLOAD_REQ,0x2};
  BYTE rsp[128] = {0};
  int status = ERROR_SUCCESS;
  int bytesRead = sizeof(rsp);
  unsigned short crc;
  active_prtn[34] = 0xAD;
  active_prtn[35] = 0xDE;
  crc = CalcCRC16(&active_prtn[1],35);
  active_prtn[36] = crc & 0xff;
  active_prtn[37] = crc >> 8;
  status = sport->SendSync(active_prtn,sizeof(active_prtn),rsp,&bytesRead);
  if( (status == ERROR_SUCCESS) && (rsp[0] == EHOST_STREAM_DLOAD_RSP) ) {
    return ERROR_SUCCESS;
  }

  return status;
}

int Dload::CreateGPP(DWORD dwGPP1, DWORD dwGPP2, DWORD dwGPP3, DWORD dwGPP4)
{
  BYTE stream_dload[38] = {EHOST_STREAM_DLOAD_REQ,0x1};
  BYTE rsp[128] = {0};
  int bytesRead = sizeof(rsp);
  int status = ERROR_SUCCESS;
  unsigned short crc;
  stream_dload[2] = dwGPP1 & 0xff;
  stream_dload[3] = (dwGPP1 >> 8) & 0xff;
  stream_dload[4] = (dwGPP1 >> 16) & 0xff;
  stream_dload[5] = (dwGPP1 >> 24) & 0xff;
  stream_dload[6] = dwGPP2 & 0xff;
  stream_dload[7] = (dwGPP2 >> 8) & 0xff;
  stream_dload[8] = (dwGPP2 >> 16) & 0xff;
  stream_dload[9] = (dwGPP2 >> 24) & 0xff;
  stream_dload[10] = dwGPP3 & 0xff;
  stream_dload[11] = (dwGPP3 >> 8) & 0xff;
  stream_dload[12] = (dwGPP3 >> 16) & 0xff;
  stream_dload[13] = (dwGPP3 >> 24) & 0xff;
  stream_dload[14] = dwGPP4 & 0xff;
  stream_dload[15] = (dwGPP4 >> 8) & 0xff;
  stream_dload[16] = (dwGPP4 >> 16) & 0xff;
  stream_dload[17] = (dwGPP4 >> 24) & 0xff;
  stream_dload[34] = 0xAD;
  stream_dload[35] = 0xDE;
  crc = CalcCRC16(&stream_dload[1],35);
  stream_dload[36] = crc & 0xff;
  stream_dload[37] = crc >> 8;
  status = sport->SendSync(stream_dload,sizeof(stream_dload),rsp,&bytesRead);
  if( (status == ERROR_SUCCESS) && (rsp[0] == EHOST_STREAM_DLOAD_RSP) ) {
    // Return actual size of the card
    return ERROR_SUCCESS;
  }
  return status;
}


uint64 Dload::GetNumDiskSectors()
{
  BYTE stream_dload[38] = {EHOST_STREAM_DLOAD_REQ,0x0};
  BYTE rsp[128] = {0};
  int bytesRead = sizeof(rsp);
  int status = ERROR_SUCCESS;
  unsigned short crc;
  stream_dload[34] = 0xAD;
  stream_dload[35] = 0xDE;
  crc = CalcCRC16(&stream_dload[1],35);
  stream_dload[36] = crc & 0xff;
  stream_dload[37] = crc >> 8;
  status = sport->SendSync(stream_dload,sizeof(stream_dload),rsp,&bytesRead);
  if( (status == ERROR_SUCCESS) && (rsp[0] == EHOST_STREAM_DLOAD_RSP) ) {
    // Return actual size of the card
    return (rsp[5] | rsp[6] << 8 | rsp[7] << 16 | rsp[8] << 24);
  }
  return status;
}

int Dload::ProgramPartitionEntry(PartitionEntry pe)
{
  int status = ERROR_SUCCESS;
  HANDLE hInFile = INVALID_HANDLE_VALUE;

  if( wcscmp(pe.filename,L"ZERO") == 0 ) {
    wprintf(L"Zeroing out area\n");
  } else {
    // Open the file that we are supposed to dump
    hInFile = CreateFile( pe.filename,
                          GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          NULL,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);
    if( hInFile == INVALID_HANDLE_VALUE ) {
      status = GetLastError();
    } else {
      LONG dwOffsetHigh = (pe.offset >> 32);
      status = SetFilePointer(hInFile,(LONG)pe.offset,&dwOffsetHigh,FILE_BEGIN);
    }
  }
  
  if( status == ERROR_SUCCESS ) {
    // Fast copy from input file to Serial port
    wprintf(L"\nIn offset: %I64d out offset: %I64d sectors: %I64d\n",pe.offset, pe.start_sector,pe.num_sectors);
    status = FastCopySerial(hInFile,(DWORD)pe.start_sector,(DWORD)pe.num_sectors);
  }
  
  if( hInFile != INVALID_HANDLE_VALUE ) CloseHandle(hInFile);
  return status;
}

int Dload::WipeDiskContents(uint64 start_sector, uint64 num_sectors)
{
  PartitionEntry pe;
  memset(&pe,0,sizeof(pe));
  wcscpy_s(pe.filename,L"ZERO");
  pe.start_sector = start_sector;
  pe.num_sectors = num_sectors;
  return ProgramPartitionEntry(pe);
}

 int Dload::WriteRawProgramFile(TCHAR *szXMLFile)
 {
  int status = ERROR_SUCCESS;
  Partition *p;
  p = new Partition(GetNumDiskSectors());

  wprintf(L"Parsing partition table: %s\n", szXMLFile);
  status = p->PreLoadImage(szXMLFile);
  if( status != ERROR_SUCCESS ) {
    wprintf(L"Partition table failed to load\n");
    delete p;
    return status;
  }

  PartitionEntry pe;
  TCHAR keyName[MAX_STRING_LEN];
  TCHAR *key;
  while( p->GetNextXMLKey(keyName,&key) == ERROR_SUCCESS ) {
    // parse the XML key if we don't understand it then continue
    if( p->ParseXMLKey(key,&pe) != ERROR_SUCCESS ) continue;
    // If there is a CRC then calculate this value before we write it out
    if( pe.eCmd == CMD_PROGRAM ) {
      status = ProgramPartitionEntry(pe);
    } else if( pe.eCmd == CMD_PATCH ) {
      // Use the diskwriter to patch the actual file on disk
      Partition *part = new Partition(0);
      part->ProgramPartitionEntry(NULL, pe, NULL);
      delete part;
    } else if( pe.eCmd == CMD_ZEROOUT ) {
      status = WipeDiskContents(pe.start_sector,pe.num_sectors);
    }

    if( status != ERROR_SUCCESS ) {
      break;
    }
  }

  p->CloseXML();
  delete p;
  return status;
 }
