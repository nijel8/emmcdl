/*****************************************************************************
 * partition.cpp
 *
 * This file parses XML and decodes information about raw partition entry
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/partition.cpp#9 $
$DateTime: 2015/04/30 15:10:13 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
10/10/11   pgw     Fix up compiler warning
05/18/11   pgw     Added support for CRC calculations for GPT tweaked partition parsing
03/21/11   pgw     Initial version.
=============================================================================*/

#include "stdio.h"

#include "partition.h"
#include "protocol.h"
#include "sparse.h"

#include "sysdeps.h"
#include <stdlib.h>
#include <ctype.h>
#if 0
// Note currently just replace in place and fill in spaces
char *StringSetValue(char *key, char *keyName, char *value)
{
  char *sptr = strstr(key, keyName);
  if( sptr == NULL) return key;

  sptr = strstr(sptr,"\"");
  if( sptr == NULL) return key;

  // Loop through and replace with actual size;
  for(sptr++;;) {
    char tmp = *value++;
    if( *sptr == '"') break;
    if( tmp == 0 ) {
      *sptr++ = '"';
      for(;*sptr!='"';sptr++) {
        *sptr = ' ';
      }
      *sptr = ' ';
      break;
    }
    *sptr++ = tmp;
  }
  return key;
}

char *StringReplace(char *inp, const char *find, const char *rep)
{
  char tmpstr[MAX_STRING_LEN];
  int max_len = MAX_STRING_LEN;
  char *tptr = tmpstr;
  char *sptr = strstr(inp, find);

  if( sptr != NULL ) {
    // Copy part of string before the value to replace
    strncpy(tptr,inp,(sptr-inp));
    max_len -= (sptr-inp);
    tptr += (sptr-inp);
    sptr += strlen(find);
    // Copy the replace value
    strncpy(tptr,rep,strlen(rep));
    max_len -= strlen(rep);
    tptr += strlen(rep);

    // Copy the rest of the string
    strncpy(tptr,sptr,strlen(sptr)+1);

    // Copy new temp string back into original
    strcpy(inp,tmpstr);
  }

  return inp;
}
#endif
int Partition::Log(const char *str,...)
{
  // For now map the log to dump output to console
  if (bVerbose) {
      va_list ap;
      va_start(ap,str);
      vprintf(str,ap);
      va_end(ap);
  }
  return 0;
}

void Partition::EnableVerbose()
{
  bVerbose = true;
}


int Partition::Reflect(int data, int len)
{
    int ref = 0;

    for(int i=0; i < len; i++) {
        if(data & 0x1) {
            ref |= (1 << ((len - 1) - i));
        }
        data = (data >> 1);
    }

   return ref;
}

unsigned int Partition::CalcCRC32(unsigned char *buffer, int len)
{
   int k = 8;                   // length of unit (i.e. byte)
   int MSB = 0;
   int gx = 0x04C11DB7;         // IEEE 32bit polynomial
   int regs = 0xFFFFFFFF;       // init to all ones
   int regsMask = 0xFFFFFFFF;   // ensure only 32 bit answer
   int regsMSB = 0;

   for( int i=0; i < len; i++) {
     unsigned char DataByte = buffer[i];
     DataByte = (unsigned char)Reflect(DataByte,8);
     for(int j=0; j < k; j++) {
       MSB = DataByte >> (k-1);  // get MSB
       MSB &= 1;                 // ensure just 1 bit
       regsMSB = (regs>>31) & 1; // MSB of regs
       regs = regs<<1;           // shift regs for CRC-CCITT
       if(regsMSB ^ MSB) {       // MSB is a 1
         regs = regs ^ gx;       // XOR with generator poly
       }
       regs = regs & regsMask;   // Mask off excess upper bits
       DataByte <<= 1;           // get to next bit
     }
   }

   regs = regs & regsMask;       // Mask off excess upper bits
   return Reflect(regs,32) ^ 0xFFFFFFFF;
}
#if 0
int Partition::ParseXMLString(char *line, const char *key, char *value)
{
  char *eptr;
  char *sptr = strstr(line,key);
  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr = strchr(sptr, '"');
  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr++;
  eptr = strchr(sptr, '"');
  if( eptr == NULL) return ERROR_INVALID_DATA;
  // Copy the value between the quotes to output string
  strncpy(value,sptr,eptr-sptr);
  value[eptr-sptr] = 0;
  return 0;
}
#endif
int Partition::ParseXMLEvaluate(char *expr, __uint64_t &value, PartitionEntry *pe) const
{
  // Parse simple expression understands -+/*, NUM_DISK_SECTORS,CRC32(offset:length)
  char disk_sectors[64];
  char *sptr, *sptr1, *sptr2;
  sprintf(disk_sectors, "%lu", d_sectors);
  expr = StringReplace(expr,"NUM_DISK_SECTORS",disk_sectors);
  value = atoll(expr);

  sptr = strstr(expr,"CRC32");
  if( sptr != NULL ) {
    char tmp[MAX_STRING_LEN];
    sptr1 = strstr(sptr,"(") + 1;
    if( sptr1 == NULL ) return EINVAL;
    sptr2 = strstr(sptr1,",");
    if( sptr2 == NULL ) return EINVAL;
    strncpy(tmp,sptr1,(sptr2-sptr1));
    tmp[sptr2-sptr1] = 0;
    ParseXMLEvaluate(tmp, pe->crc_start, pe);
    sptr1 = sptr2 + 1;
    sptr2 = strstr(sptr1,")");
    if( sptr2 == NULL ) return EINVAL;
    strncpy(tmp,sptr1,(sptr2-sptr1));
    tmp[sptr2-sptr1] = 0;
    ParseXMLEvaluate(tmp, pe->crc_size,pe);
    // Revome the CRC part set value 0
    memset(sptr,' ',(sptr2-sptr+1));
    value = 0;
  }

  sptr = strstr(expr,"*");
  if( sptr != NULL ) {
    // Found * do multiplication
    char val1[64];
    char val2[64];
    strncpy(val1,expr,(sptr-expr));
    val1[sptr-expr] = 0;
    strcpy(val2,sptr+1);
    value = atoll(val1) * atoll(val2);
  }

  sptr = strstr(expr,"/");
  if( sptr != NULL ) {
    // Found / do division
    char val1[64];
    char val2[64];
    strncpy(val1,expr,(sptr-expr));
    val1[sptr-expr] = 0;
    strcpy(val2,sptr+1);
    value = atoll(val1) / atoll(val2);
  }

  sptr = strstr(expr,"-");
  if( sptr != NULL ) {
    // Found - do subtraction
    char val1[32];
    char val2[32];
    strncpy(val1,expr,(sptr-expr));
    val1[sptr-expr] = 0;
    strcpy(val2,sptr+1);
    value = atoll(val1) - atoll(val2);
  }
  
  sptr = strstr(expr,"+");
  if( sptr != NULL ) {
    // Found + do addition
    char val1[32];
    char val2[32];
    strncpy(val1,expr,(sptr-expr));
    val1[sptr-expr] = 0;
    strcpy(val2,sptr+1);
    value = atoll(val1) + atoll(val2);
  }

  return 0;
}

int Partition::ParseXMLInt64(char *line, const char *key, __uint64_t &value, PartitionEntry *pe) const
{
  char tmp[MAX_STRING_LEN];
  char *eptr;
  char *sptr = strstr(line,key);

  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr = strchr(sptr,'"');

  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr++;
  eptr = strchr(sptr, '"');

  if( eptr == NULL) return ERROR_INVALID_DATA;

  strncpy(tmp,sptr,eptr-sptr);

  tmp[eptr-sptr] = 0;

  ParseXMLEvaluate(tmp,value, pe);

  return 0;
}

int Partition::ParseXMLOptions()
{

  return 0;
}

int Partition::ParsePathList()
{
  return 0;
}

bool Partition::CheckEmptyLine(char *str) const
{
  int keylen = strlen(str);
  while (isspace(*str) && (keylen > 0))
  {
    str++;
    keylen--;
  }

  return (keylen == 0);
}

int Partition::ParseXMLKey(char *key, PartitionEntry *pe)
{
  // If line is whitepsace return CMD_NOP
  if (CheckEmptyLine(key)) {
    pe->eCmd = CMD_NOP;
    return 0;
  }

  // First of all check if this is a program, patch or zeroout line
  pe->eCmd = CMD_INVALID;
  if (strstr(key, "<data>") != NULL || strstr(key, "</data>") != NULL
    || strstr(key, "</patches>") != NULL || strstr(key, "</data>") != NULL) {
    pe->eCmd = CMD_NOP;
    return 0;
  } else if (strstr(key, "simlock") != NULL) {
    pe->eCmd = CMD_SIMLOCK;
  } else if (strstr(key, "program") != NULL) {
    pe->eCmd = CMD_PROGRAM;
  } else if( strstr(key,"patch") != NULL ) {
    pe->eCmd = CMD_PATCH;
  } else if( strstr(key,"options") != NULL ) {
    pe->eCmd = CMD_OPTION;
  } else if( strstr(key,"search_path") != NULL ) {
    pe->eCmd = CMD_PATH;
  } else if (strstr(key, "read") != NULL) {
    pe->eCmd = CMD_READ;
  } else if (strstr(key, "peek") != NULL) {
        pe->eCmd = CMD_PEEK;
        return 0;
  } else {
    Log("%s\n", key);
    //return ERROR_INVALID_DATA;
  }
  Log("\n");
  // Set options and search path
  if( pe->eCmd == CMD_OPTION ) {
    ParseXMLOptions();
  } else if( pe->eCmd == CMD_PATH ) {
    ParsePathList();
  }

  // All commands need start_sector, physical_partition_number and num_partition_sectors
  if( ParseXMLInt64(key,"start_sector", pe->start_sector, pe) != 0 ) {
    Log("start_sector missing in XML line:%s\n",key);
    return ERROR_INVALID_DATA;
  } else {
    Log("start_sector: %d ",  pe->start_sector);
  }
	
  __uint64_t partNum;
  if( ParseXMLInt64(key,"physical_partition_number", partNum, pe) != 0 ) {
    Log("physical_partition_number missing in XML line\n");
    return ERROR_INVALID_DATA;
  } else {
    pe->physical_partition_number = (uint8_t)partNum;
    Log("physical_partition_number: %i ", pe->physical_partition_number);
  }

  if( ParseXMLInt64(key,"num_partition_sectors", pe->num_sectors, pe) == 0 ) {
    Log("num_partition_sectors: %d ", pe->num_sectors);
    // If zero then write out all sectors for size of file
  } else {
    pe->num_sectors = (__uint64_t )-1;
  }

  if( pe->eCmd == CMD_PATCH || pe->eCmd == CMD_PROGRAM || pe->eCmd == CMD_READ || pe->eCmd == CMD_SIMLOCK) {
    // Both program and patch need a filename to be valid
    if( ParseXMLString(key,"filename", pe->filename) != 0 ) {
      Log("filename missing in XML line\n");
		  return ERROR_INVALID_DATA;
    } else {
      // Only program if filename is specified
      if( strlen(pe->filename) == 0 ) {
        pe->eCmd = CMD_NOP;
        return 0;
      }
      Log("filename: %s ", pe->filename);
    }


    // File sector offset is optional for both these otherwise use default
    if( ParseXMLInt64(key,"file_sector_offset", pe->offset, pe) == 0 ) {
			Log("file_sector_offset: %d ", pe->offset);
    } else {
      pe->offset = (__uint64_t )-1;
    }
	
    // The following entries should only be used in patching
    if( pe->eCmd == CMD_PATCH ) {
      // Get the value parameter
	  pe->crc_start = (uint64_t) -1;
      pe->crc_size = (uint64_t) -1;
      if( ParseXMLInt64(key,"value", pe->patch_value, pe) == 0 ) {
	      Log("value: %d ", pe->patch_value);
	    } else {
        Log("value missing in patch command\n");
        return ERROR_INVALID_DATA;
      }
      Log("crc_size: %i ", (int)pe->crc_size);

      // get byte offset for patch value to be written
      if( ParseXMLInt64(key,"byte_offset", pe->patch_offset, pe) == 0 ) {
	      Log("patch_offset: %d ", pe->patch_offset);
	    } else {
        Log("byte_offset missing in patch command\n");
        return ERROR_INVALID_DATA;
      }

      // Get the size of the patch in bytes
      if( ParseXMLInt64(key,"size_in_bytes", pe->patch_size, pe) == 0 ) {
	     Log("patch_size: %d ", pe->patch_size);
	    } else {
        Log("size_in_bytes missing in patch command\n");
        return ERROR_INVALID_DATA;
      }

    } // end of CMD_PATCH
  } // end of CMD_PROGRAM || CMD_PATCH
 
  return 0;
}



int Partition::ProgramPartitionEntry(Protocol *proto, PartitionEntry pe, char *key)
{
  int hRead = -1;
  bool bSparse = false;
  int status = 0;

  if (proto == NULL) {
    printf("Can't write to disk no protocol passed in.\n");
    return EINVAL;
  }

  if (strcmp(pe.filename, "ZERO") == 0) {
    printf("Zeroing out area\n");
  }
  else {
    // First check if the file is a sparse image then program via sparse
    SparseImage sparse;
    char imgfname[MAX_PATH];
    const char* ptr = xmlFilename ? rindex(xmlFilename,'/'): NULL;
    if (ptr != NULL) {
       if (imgDir != NULL) {
           sprintf(imgfname, "%s/%s", imgDir, pe.filename);
       } else {
           strncpy(imgfname, xmlFilename, ptr - xmlFilename);
           sprintf(&imgfname[ptr - xmlFilename], "/%s", pe.filename);
       }
    } else {
       if (imgDir != NULL) {
           sprintf(imgfname, "%s/%s", imgDir, pe.filename);
       } else {
           strcpy(imgfname, pe.filename);
       }
    }

    printf("\nSparse image:%s\n", imgfname);
    status = sparse.PreLoadImage(imgfname);
    if (status == 0) {
      bSparse = true;
      printf("detected \n-- loading sparse...\n");
      status = sparse.ProgramImage(proto, pe.start_sector*proto->GetDiskSectorSize());
    }
    else
    {
      printf("not detected \n-- loading binary...\n");
      // Open the file that we are supposed to dump
      status = 0;
      hRead = emmcdl_open(imgfname,O_RDONLY);
      if (hRead < 0) {
        status = errno;
      }
      else {
        // Update the number of sectors based on real file size, rounded to next sector offset

        struct stat      my_stat;
        int ret = fstat(hRead, &my_stat);
        // Make sure filesize is valid
        if(ret)
                return ret;
        int64_t dwTotalSize = my_stat.st_size;
        dwTotalSize = (dwTotalSize + proto->GetDiskSectorSize() - 1) & (int64_t)~(proto->GetDiskSectorSize() - 1);
        dwTotalSize = dwTotalSize / proto->GetDiskSectorSize();
        if (dwTotalSize <= (int64_t)pe.num_sectors) {
          pe.num_sectors = dwTotalSize;
        }
        else {
          printf("\nFileSize is > partition size, truncating file\n");
        }
        status = 0;
      }
    }
  }

  if (status == 0 && !bSparse) {
    // Fast copy from input file to output disk
    Log("In offset: %lu out offset: %lu sectors: %lu\n", pe.offset, pe.start_sector, pe.num_sectors);
    status = proto->FastCopy(hRead, pe.offset, proto->GetDiskHandle(),  pe.start_sector, pe.num_sectors,pe.physical_partition_number);
  }
  if (hRead > 0)
    emmcdl_close(hRead);
  return status;
}

int Partition::ProgramImage(Protocol *proto)
{
  int status = 0;

  PartitionEntry pe;
  char keyName[MAX_STRING_LEN];
  char *key;
  while (GetNextXMLKey(keyName, &key) == 0) {
    // parse the XML key if we don't understand it then continue
    if (ParseXMLKey(key, &pe) != 0) {
      // If we don't understand the command just try sending it otherwise ignore command
      if (pe.eCmd == CMD_INVALID) {
        status = proto->ProgramRawCommand(key);
      }
    }
    else if (pe.eCmd == CMD_PROGRAM) {
      status = ProgramPartitionEntry(proto, pe, key);
    }
    else if (pe.eCmd == CMD_SIMLOCK) {
      status = SimlockPartitionEntry(proto, pe, key);
    }
    else if (pe.eCmd == CMD_PATCH) {
      // Only patch disk entries
      if (strcmp(pe.filename, "DISK") == 0) {
        status = proto->ProgramPatchEntry(pe, key);
      }
    }
    else if (pe.eCmd == CMD_READ) {
      status = proto->DumpDiskContents(pe.start_sector, pe.num_sectors, pe.filename, pe.physical_partition_number, NULL);
    }
    else if (pe.eCmd == CMD_ZEROOUT) {
      status = proto->WipeDiskContents(pe.start_sector, pe.num_sectors, NULL);
    }

    if (status != 0) {
      break;
    }
  }

  return status;
}

int Partition::SimlockPartitionEntry(Protocol *proto, PartitionEntry pe, char *key)
{
  int hRead = -1;
  int status = 0;

  if (proto == NULL) {
    printf("Can't write to disk no protocol passed in.\n");
    return EINVAL;
  }

  char imgfname[MAX_PATH];
  const char* ptr = rindex(xmlFilename,'/');
  if (ptr != NULL) {
    if (imgDir != NULL) {
      sprintf(imgfname, "%s/%s", imgDir, pe.filename);
    } else {
      strncpy(imgfname, xmlFilename, ptr - xmlFilename);
      sprintf(&imgfname[ptr - xmlFilename], "/%s", pe.filename);
    }
  } else {
    if (imgDir != NULL) {
      sprintf(imgfname, "%s/%s", imgDir, pe.filename);
    } else {
      strcpy(imgfname, pe.filename);
    }
  }

  printf("\n-- loading binary...%s\n", imgfname);
  // Open the file that we are supposed to dump
  struct stat      my_stat;
  status = 0;
  hRead = emmcdl_open(imgfname,O_RDONLY);
  if (hRead < 0) {
    printf("-- skip flash simlock\n");
    return 0;//status = errno;
  }
  else {
    // Update the number of sectors based on real file size, rounded to next sector offset

    int ret = fstat(hRead, &my_stat);
    // Make sure filesize is valid
    if(ret)
      return ret;
    int64_t dwTotalSize = my_stat.st_size;
    dwTotalSize = (dwTotalSize + proto->GetDiskSectorSize() - 1) & (int64_t)~(proto->GetDiskSectorSize() - 1);
    dwTotalSize = dwTotalSize / proto->GetDiskSectorSize();
    if (dwTotalSize <= (int64_t)pe.num_sectors) {
      pe.num_sectors = dwTotalSize;
    }
    else {
      printf("\nFileSize is > partition size, truncating file\n");
    }
    status = 0;
  }

  if (status == 0 ) {
    // Fast copy from input file to output disk
    Log("In offset: %lu out offset: %lu sectors: %lu\n", pe.offset, pe.start_sector, pe.num_sectors);
    unsigned char *bpDataBuf = (unsigned char *)malloc(my_stat.st_size);
    if (!bpDataBuf) return errno;
    status = emmcdl_read(hRead, bpDataBuf, my_stat.st_size);
    uint32_t dwBytesOut = pe.num_sectors;
    status = proto->WriteSimlockData(bpDataBuf, pe.start_sector, my_stat.st_size, &dwBytesOut,pe.physical_partition_number);
    emmcdl_close(hRead);
    free(bpDataBuf);
  }
  return status;
}

int Partition::PreLoadImage(char *fname, const char *imgdir)
{
	return LoadXML(fname, imgdir);
}


int Partition::GetNextXMLKey(char *keyName, char **key)
{
  bool bRecordKey = true;
  // Find < and make sure it's not a comment
  for(;(keyStart < xmlEnd) && bRecordKey; keyStart++) {
    // Check for starting of a comment
    if( *keyStart == '<' && *(keyStart+1) == '!' && *(keyStart+2) == '-' && *(keyStart+3)== '-' ) {
      // This is a comment search for the closing -->
      for(;keyStart < xmlEnd;keyStart++) {
        if( *keyStart == '-' && *(keyStart+1) == '-' && *(keyStart+2) == '>') break;
      }
    } else if( *keyStart == '<' ) {
      // suck in the keyname up to MAX_STRING_LEN
      // go past until we hit alpha numeric value
      for(keyEnd = keyStart; !isalnum(*keyEnd); keyEnd++);
      for(;keyEnd < xmlEnd; keyEnd++) {
        if( *keyEnd == '>' && *(keyEnd-1) == '/') {
          // TODO:FIXME handle multiple embedded parameters
          break;
        } else if( bRecordKey ) {
          if( isalnum(*keyEnd) ) *keyName++ = *keyEnd;
          else bRecordKey = false;
          *keyName = 0;
        }
      }
    }
  }

  // Terminate this string for searches on it
  if( keyStart < xmlEnd ) {
    *keyEnd = 0;
    *key = keyStart-1;
    return 0;
  }
  return EINVAL;
}
