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

#include "dload.h"
#include "partition.h"
#include "diskwriter.h"
#define ERROR_WRITE_FAULT -19
//using namespace std;

void Dload::HexToByte(const char *hex, unsigned char *bin, int len)
{
  for(int i=0; i < len; i++) {
   unsigned char val1 = hex[i*2] - '0';
   unsigned char val2 = hex[i*2+1] - '0';
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

int Dload::ConnectToFlashProg(unsigned char ver)
{
  unsigned char hello[] = {EHOST_HELLO_REQ,'Q','C','O','M',' ','f','a','s','t',' ','d','o','w','n','l','o','a','d',' ','p','r','o','t','o','c','o','l',' ','h','o','s','t',ver,2,1};
  unsigned char security[] = {EHOST_SECURITY_REQ,0};
  unsigned char rsp[1060];
  int rspSize = sizeof(rsp);
  int status = 0;
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
    if( (status == 0) && (rspSize > 0) && (rsp[0] == EHOST_HELLO_RSP)) {
      break;
    }
    sport->Flush();
    emmcdl_sleep_ms(500);
  }

  // Clear out any stale data so we start on a new buffer
  sport->Flush();
  if( i == 10 ) {
    return EINVAL;
  }


  printf("Got hello response\n");
  for(; i < 10; i++) { 
    rspSize = sizeof(rsp);
    sport->SendSync(security,sizeof(security),rsp,&rspSize);
    if( (rspSize > 0) && (rsp[0] == EHOST_SECURITY_RSP)) {
      return 0;
    }
    sport->Flush();
    emmcdl_sleep_ms(500);
  }

  return EINVAL;
}

int Dload::DeviceReset()
{
  unsigned char reset[] = {EHOST_RESET_REQ};
  unsigned char rsp[32];
  int rspSize;

  rspSize = sizeof(rsp);
  sport->SendSync(reset,sizeof(reset),rsp,&rspSize);
  if( (rspSize == 0) || (rsp[0] != EHSOT_RESET_ACK)) {
    return EINVAL;
  }

  return 0;
}

int Dload::LoadPartition(char *szPrtnFile)
{
  int hFile;
  int status = 0;
  unsigned char rsp[32];
  uint32_t bytesRead=0;
  int rspSize;
  unsigned char partition[1040] = {EHOST_PARTITION_REQ,0};
  
  hFile = emmcdl_creat(szPrtnFile,O_RDWR);
  if( hFile == -1 ) return EINVAL;

  // Download the partition table as one large chunk
  if((bytesRead = emmcdl_read(hFile,&partition[2],1024)) < 0 ) {
    status = errno;
  }

  if( bytesRead > 0 ) {
    rspSize = sizeof(rsp);
    sport->SendSync(partition,bytesRead+2,rsp,&rspSize);
    if( (rspSize == 0) || (rsp[0] != EHOST_PARTITION_RSP) ) {
      status = ERROR_WRITE_FAULT;
    }
  }
  emmcdl_close(hFile);

  return status;
}

int Dload::OpenPartition(int partition)
{
  unsigned char openmulti[] = {EHOST_OPEN_MULTI_REQ,(unsigned char)partition};
  unsigned char rsp[32];
  int rspSize;

  // Flush out any previous transactions
  sport->Flush();
  
  // Open the partition
  rspSize = sizeof(rsp);
  sport->SendSync(openmulti,sizeof(openmulti),rsp,&rspSize);
  if( (rspSize == 0) || (rsp[0] != EHOST_OPEN_MULTI_RSP)) {
    printf("Could not open partition\n");
    return EINVAL;
  }

  return 0;
}

int Dload::ClosePartition()
{
  unsigned char close[] = {EHOST_CLOSE_REQ};
  unsigned char rsp[32];
  int rspSize;
  
  // Close the partition we were writing to
  rspSize = sizeof(rsp);
  sport->SendSync(close,sizeof(close),rsp,&rspSize);
  if( (rspSize == 0) || (rsp[0] != EHOST_CLOSE_RSP)) {
    return EINVAL;
  }
  return 0;
}

int Dload::FastCopySerial(int hInFile, uint32_t offset, uint32_t sectors)
{
  int status = 0;
  unsigned char rsp[32];
  uint32_t bytesRead=0;
  uint32_t count = 0;
  uint32_t readSize = 1024;
  int rspSize;
  unsigned char streamwrite[1050] = {EHOST_STREAM_WRITE_REQ,0};

  // For now don't worry about sliding window do a write and wait for response
  while( (status == 0) && (count < sectors)) {
    if( (count + 2) > sectors) {
      readSize = 512;
    }
    // If int value is invalid then just write 0's
    if( hInFile != -1 ) {
      if((bytesRead = emmcdl_read(hInFile,&streamwrite[5],readSize)) < 0 ) {
        status = errno;
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
        printf("Device returned error: %i\n",rsp[0]);
        if( rsp[0] == EHOST_LOG && rspSize >= 2) {
          rsp[rspSize-2] = 0;
          printf("%s", &rsp[1]);
        }
        status = ERROR_WRITE_FAULT;
      }
      if( bSectorAddress ) {
        offset += bytesRead/SECTOR_SIZE;
        printf("Destination sector: %i\r",(int)offset);
      } else {
        offset += bytesRead;
        printf("Destination offset: %i\r",(int)offset);
      }
    } else {
      // If we didn't read anything then break
      break;
    }
  }

  printf("\n");

  // If we hit end of file that means we sent it all
  return status;
}

int Dload::LoadImage(char *szSingleImage)
{
  int hFile;
  int status = 0;

  hFile = emmcdl_creat(szSingleImage, O_RDWR);
  if( hFile == -1 ) {
    printf("Filename not found %s\n",szSingleImage);
    return EINVAL;
  }

  status = FastCopySerial(hFile,0,(uint32_t)-1);
  emmcdl_close(hFile);

  return status;
}

int Dload::LoadFlashProg(char *szFlashPrg)
{
  unsigned char write32[280] = {CMD_WRITE_32BIT, 0};
  unsigned char *writePtr;
  unsigned char rsp[32];
  int rspSize;
  uint32_t goAddr = 0;
  uint32_t targetAddr=0;
  uint32_t newAddr=0;
  uint32_t bytesRead;
  uint32_t status = 0;

  char hexline[128];
  FILE *fMprgFile = fopen(szFlashPrg,"rb");

  if( fMprgFile == NULL ) {
    return EBADF;
  }

  while( !feof(fMprgFile) ) {
    unsigned char id=0;
    bool bFirst = true;
    writePtr = &write32[7];
    // Read in data till we collect 256 bytes or hit end of file
    for(bytesRead=0; bytesRead < 256;) {
      unsigned char len;
      char* unused = fgets(hexline,sizeof(hexline),fMprgFile);
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
      printf("Program at: 0x%x length %i\n",targetAddr,bytesRead);
      sport->SendSync(write32,bytesRead+7,rsp,&rspSize);
      if( (rspSize == 0) || (rsp[0] != CMD_ACK) ) {
        status = ERROR_WRITE_FAULT;
        break;
      }
    }

    targetAddr = newAddr;
  }
  fclose(fMprgFile);
  

  if( status == 0 ) {
    unsigned char gocmd[5] = {CMD_GO,0};
    printf("sending go command 0x%x\n", (uint32_t)goAddr);
    gocmd[1] = (goAddr >> 24) & 0xff;
    gocmd[2] = (goAddr >> 16) & 0xff;
    gocmd[3] = (goAddr >> 8) & 0xff;
    gocmd[4] = goAddr & 0xff;
  
    // Send GO command if we successfully uploaded to end of file
    rspSize = sizeof(rsp);
    sport->SendSync(gocmd,sizeof(gocmd),rsp,&rspSize);
    if( (rspSize > 0) && (rsp[0] == CMD_ACK)) {
      status = 0;
    } else {
      status = ERROR_WRITE_FAULT;
    }
  }
  
  return status;
}

int Dload::GetDloadParams(unsigned char *rsp, int len)
{
  unsigned char params[] = {CMD_PREQ};
  int bytesRead = len;
  // For params response we need at least 256 bytes
  if( len < 16 ) {
    return -1;
  }
  sport->SendSync(params,sizeof(params),rsp,&bytesRead);
  if( (bytesRead > 0) && (rsp[0] == CMD_PARAMS) ) {
    return 0;
  }
  return -2;
}

int Dload::IsDeviceInDload(void)
{
  unsigned char nop[] = {CMD_NOP};
  unsigned char rsp[32] = {0};
  int bytesRead = 32;

  printf("%s 0Downloading flash programmer: \n",__func__);
  sport->SendSync(nop,sizeof(nop),rsp,&bytesRead);
  for (int i=0;i < 32;i++)
      printf("%x: ",rsp[i]);
  if( rsp[0] == CMD_ACK ) {
    return 0;
  }
  
  printf("\n%s 1Downloading flash programmer: \n",__func__);
  return EINVAL;
}

int Dload::SetActivePartition()
{
  // Set currently open partition to active
  unsigned char active_prtn[38] = {EHOST_STREAM_DLOAD_REQ,0x2};
  unsigned char rsp[128] = {0};
  int status = 0;
  int bytesRead = sizeof(rsp);
  unsigned short crc;
  active_prtn[34] = 0xAD;
  active_prtn[35] = 0xDE;
  crc = CalcCRC16(&active_prtn[1],35);
  active_prtn[36] = crc & 0xff;
  active_prtn[37] = crc >> 8;
  status = sport->SendSync(active_prtn,sizeof(active_prtn),rsp,&bytesRead);
  if( (status == 0) && (rsp[0] == EHOST_STREAM_DLOAD_RSP) ) {
    return 0;
  }

  return status;
}

int Dload::CreateGPP(uint32_t dwGPP1, uint32_t dwGPP2, uint32_t dwGPP3, uint32_t dwGPP4)
{
  unsigned char stream_dload[38] = {EHOST_STREAM_DLOAD_REQ,0x1};
  unsigned char rsp[128] = {0};
  int bytesRead = sizeof(rsp);
  int status = 0;
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
  if( (status == 0) && (rsp[0] == EHOST_STREAM_DLOAD_RSP) ) {
    // Return actual size of the card
    return 0;
  }
  return status;
}


__uint64_t Dload::GetNumDiskSectors()
{
  unsigned char stream_dload[38] = {EHOST_STREAM_DLOAD_REQ,0x0};
  unsigned char rsp[128] = {0};
  int bytesRead = sizeof(rsp);
  int status = 0;
  unsigned short crc;
  stream_dload[34] = 0xAD;
  stream_dload[35] = 0xDE;
  crc = CalcCRC16(&stream_dload[1],35);
  stream_dload[36] = crc & 0xff;
  stream_dload[37] = crc >> 8;
  status = sport->SendSync(stream_dload,sizeof(stream_dload),rsp,&bytesRead);
  if( (status == 0) && (rsp[0] == EHOST_STREAM_DLOAD_RSP) ) {
    // Return actual size of the card
    return (rsp[5] | rsp[6] << 8 | rsp[7] << 16 | rsp[8] << 24);
  }
  return status;
}

int Dload::ProgramPartitionEntry(PartitionEntry pe)
{
  int status = 0;
  int hInFile = -1;

  if( strcmp(pe.filename,"ZERO") == 0 ) {
    printf("Zeroing out area\n");
  } else {
    // Open the file that we are supposed to dump
    hInFile = emmcdl_creat( pe.filename, O_RDWR);

    if( hInFile == -1 ) {
      status = errno;
    } else {
      status = emmcdl_lseek(hInFile, pe.offset, SEEK_SET);
    }
  }
  
  if( status == 0 ) {
    // Fast copy from input file to Serial port
    printf("\nIn offset: %lu out offset: %lu sectors: %lu\n",pe.offset, pe.start_sector,pe.num_sectors);
    status = FastCopySerial(hInFile,(uint32_t)pe.start_sector,(uint32_t)pe.num_sectors);
  }
  
  if( hInFile != -1 ) emmcdl_close(hInFile);
  return status;
}

int Dload::WipeDiskContents(__uint64_t start_sector, __uint64_t num_sectors)
{
  PartitionEntry pe;
  memset(&pe,0,sizeof(pe));
  strcpy(pe.filename,"ZERO");
  pe.start_sector = start_sector;
  pe.num_sectors = num_sectors;
  return ProgramPartitionEntry(pe);
}

 int Dload::WriteRawProgramFile(char *szXMLFile)
 {
  int status = 0;
  Partition *p;
  p = new Partition(GetNumDiskSectors());

  printf("Parsing partition table: %s\n", szXMLFile);
  status = p->PreLoadImage(szXMLFile);
  if( status != 0 ) {
	printf("Partition table failed to load\n");
    delete p;
    return status;
  }

  PartitionEntry pe;
  char keyName[MAX_STRING_LEN];
  char *key;
  while( p->GetNextXMLKey(keyName,&key) == 0 ) {
	  memset(&pe, 0, sizeof(pe));
    // parse the XML key if we don't understand it then continue
    if( p->ParseXMLKey(key,&pe) != 0 ) continue;
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

    if( status != 0 ) {
      break;
    }
  }

  delete p;
  return status;
 }
