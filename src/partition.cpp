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
#include "tchar.h"
#include "partition.h"
#include "protocol.h"
#include "sparse.h"
#include "winerror.h"
#include <stdlib.h>

// Note currently just replace in place and fill in spaces
TCHAR *StringSetValue(TCHAR *key, TCHAR *keyName, TCHAR *value)
{
  TCHAR *sptr = wcsstr(key, keyName);
  if( sptr == NULL) return key;

  sptr = wcsstr(sptr,L"\"");
  if( sptr == NULL) return key;

  // Loop through and replace with actual size;
  for(sptr++;;) {
    TCHAR tmp = *value++;
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

TCHAR *StringReplace(TCHAR *inp, TCHAR *find, TCHAR *rep)
{
  TCHAR tmpstr[MAX_STRING_LEN];
  int max_len = MAX_STRING_LEN;
  TCHAR *tptr = tmpstr;;
  TCHAR *sptr = wcsstr(inp, find);

  if( sptr != NULL ) {
    // Copy part of string before the value to replace
    wcsncpy_s(tptr,max_len,inp,(sptr-inp));
    max_len -= (sptr-inp);
    tptr += (sptr-inp);
    sptr += wcslen(find);
    // Copy the replace value
    wcsncpy_s(tptr,max_len,rep,wcslen(rep));
    max_len -= wcslen(rep);
    tptr += wcslen(rep);

    // Copy the rest of the string
    wcsncpy_s(tptr,max_len,sptr,wcslen(sptr));

    // Copy new temp string back into original
    wcscpy_s(inp,MAX_STRING_LEN,tmpstr);
  }

  return inp;
}

int Partition::Log(char *str,...)
{
  // For now map the log to dump output to console
  va_list ap;
  va_start(ap,str);
  vprintf(str,ap);
  va_end(ap);
  return ERROR_SUCCESS;
}

int Partition::Log(TCHAR *str,...)
{
  // For now map the log to dump output to console
  va_list ap;
  va_start(ap,str);
  vwprintf(str,ap);
  va_end(ap);
  return ERROR_SUCCESS;
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

unsigned int Partition::CalcCRC32(BYTE *buffer, int len)
{
   int k = 8;                   // length of unit (i.e. byte)
   int MSB = 0;
   int gx = 0x04C11DB7;         // IEEE 32bit polynomial
   int regs = 0xFFFFFFFF;       // init to all ones
   int regsMask = 0xFFFFFFFF;   // ensure only 32 bit answer
   int regsMSB = 0;

   for( int i=0; i < len; i++) {
     BYTE DataByte = buffer[i];
     DataByte = (BYTE)Reflect(DataByte,8);
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

int Partition::ParseXMLString(TCHAR *line, TCHAR *key, TCHAR *value)
{
  TCHAR *eptr;
  TCHAR *sptr = wcsstr(line,key);
  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr = wcschr(sptr, L'"');
  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr++;
  eptr = wcschr(sptr, L'"');
  if( eptr == NULL) return ERROR_INVALID_DATA;
  // Copy the value between the quotes to output string
  wcsncpy_s(value,MAX_PATH,sptr,eptr-sptr);
  return ERROR_SUCCESS;
}

int Partition::ParseXMLEvaluate(TCHAR *expr, uint64 &value, PartitionEntry *pe)
{
  // Parse simple expression understands -+/*, NUM_DISK_SECTORS,CRC32(offset:length)
  TCHAR disk_sectors[64];
  TCHAR *sptr, *sptr1, *sptr2;
  _i64tow_s(d_sectors,disk_sectors,64,10);
  expr = StringReplace(expr,_T("NUM_DISK_SECTORS"),disk_sectors);
  value = _wtoi64(expr);

  sptr = wcsstr(expr,L"CRC32");
  if( sptr != NULL ) {
    TCHAR tmp[MAX_STRING_LEN];
    sptr1 = wcsstr(sptr,L"(") + 1;
    if( sptr1 == NULL ) return ERROR_INVALID_PARAMETER;
    sptr2 = wcsstr(sptr1,L",");
    if( sptr2 == NULL ) return ERROR_INVALID_PARAMETER;
    wcsncpy_s(tmp,MAX_STRING_LEN,sptr1,(sptr2-sptr1));
    ParseXMLEvaluate(tmp, pe->crc_start, pe);
    sptr1 = sptr2 + 1;
    sptr2 = wcsstr(sptr1,L")");
    if( sptr2 == NULL ) return ERROR_INVALID_PARAMETER;
    wcsncpy_s(tmp,MAX_STRING_LEN,sptr1,(sptr2-sptr1));
    ParseXMLEvaluate(tmp, pe->crc_size,pe);
    // Revome the CRC part set value 0
    memset(sptr,' ',(sptr2-sptr+1)*2);
    value = 0;
  }

  sptr = wcsstr(expr,L"*");
  if( sptr != NULL ) {
    // Found * do multiplication
    TCHAR val1[64];
    TCHAR val2[64];
    wcsncpy_s(val1,64,expr,(sptr-expr));
    wcscpy_s(val2,64,sptr+1);
    value = _wtoi64(val1) * _wtoi64(val2);
  }

  sptr = wcsstr(expr,L"/");
  if( sptr != NULL ) {
    // Found / do division
    TCHAR val1[64];
    TCHAR val2[64];
    wcsncpy_s(val1,64,expr,(sptr-expr));
    wcscpy_s(val2,64,sptr+1);
    value = _wtoi64(val1) / _wtoi64(val2);
  }

  sptr = wcsstr(expr,L"-");
  if( sptr != NULL ) {
    // Found - do subtraction
    TCHAR val1[32];
    TCHAR val2[32];
    wcsncpy_s(val1,32,expr,(sptr-expr));
    wcscpy_s(val2,32,sptr+1);
    value = _wtoi64(val1) - _wtoi64(val2);
  }
  
  sptr = wcsstr(expr,L"+");
  if( sptr != NULL ) {
    // Found + do addition
    TCHAR val1[32];
    TCHAR val2[32];
    wcsncpy_s(val1,32,expr,(sptr-expr));
    wcscpy_s(val2,32,sptr+1);
    value = _wtoi64(val1) + _wtoi64(val2);
  }

  return ERROR_SUCCESS;
}

int Partition::ParseXMLInt64(TCHAR *line, TCHAR *key, uint64 &value, PartitionEntry *pe)
{
  TCHAR tmp[MAX_STRING_LEN];
  TCHAR *eptr;
  TCHAR *sptr = wcsstr(line,key);
  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr = wcschr(sptr,L'"');
  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr++;
  eptr = wcschr(sptr, L'"');
  if( eptr == NULL) return ERROR_INVALID_DATA;
  wcsncpy_s(tmp,MAX_STRING_LEN,sptr,eptr-sptr);
  
  ParseXMLEvaluate(tmp,value, pe);

  return ERROR_SUCCESS;
}

int Partition::ParseXMLOptions()
{

  return ERROR_SUCCESS;
}

int Partition::ParsePathList()
{
  return ERROR_SUCCESS;
}

bool Partition::CheckEmptyLine(TCHAR *str)
{
  int keylen = wcslen(str);
  while (iswspace(*str) && (keylen > 0))
  {
    str++;
    keylen--;
  }

  return (keylen == 0);
}

int Partition::ParseXMLKey(TCHAR *key, PartitionEntry *pe)
{
  // If line is whitepsace return CMD_NOP
  if (CheckEmptyLine(key)) {
    pe->eCmd = CMD_NOP;
    return ERROR_SUCCESS;
  }

  // First of all check if this is a program, patch or zeroout line
  pe->eCmd = CMD_INVALID;
  if (wcsstr(key, L"<data>") != NULL || wcsstr(key, L"</data>") != NULL
    || wcsstr(key, L"</patches>") != NULL || wcsstr(key, L"</data>") != NULL) {
    pe->eCmd = CMD_NOP;
    return ERROR_SUCCESS;
  } else if (wcsstr(key, L"program") != NULL) {
    pe->eCmd = CMD_PROGRAM;
  } else if( wcsstr(key,L"patch") != NULL ) {
    pe->eCmd = CMD_PATCH;
  } else if( wcsstr(key,L"options") != NULL ) {
    pe->eCmd = CMD_OPTION;
  } else if( wcsstr(key,L"search_path") != NULL ) {
    pe->eCmd = CMD_PATH;
  } else if (wcsstr(key, L"read") != NULL) {
    pe->eCmd = CMD_READ;
  } else {
    return ERROR_INVALID_DATA;
  }

  // Set options and search path
  if( pe->eCmd == CMD_OPTION ) {
    ParseXMLOptions();
  } else if( pe->eCmd == CMD_PATH ) {
    ParsePathList();
  }

  // All commands need start_sector, physical_partition_number and num_partition_sectors
  if( ParseXMLInt64(key,L"start_sector", pe->start_sector, pe) != ERROR_SUCCESS ) {
    Log(L"start_sector missing in XML line:%s\n",key);
    return ERROR_INVALID_DATA;
  } else {
    Log("start_sector: %I64d ",  pe->start_sector);
  }
	
  uint64 partNum;
  if( ParseXMLInt64(key,L"physical_partition_number", partNum, pe) != ERROR_SUCCESS ) {
    Log("physical_partition_number missing in XML line\n");
    return ERROR_INVALID_DATA;
  } else {
    pe->physical_partition_number = (UINT8)partNum;
    Log("physical_partition_number: %i ", pe->physical_partition_number);
  }

  if( ParseXMLInt64(key,L"num_partition_sectors", pe->num_sectors, pe) == ERROR_SUCCESS ) {
    Log("num_partition_sectors: %I64d ", pe->num_sectors);
    // If zero then write out all sectors for size of file
  } else {
    pe->num_sectors = (uint64)-1;
  }

  if( pe->eCmd == CMD_PATCH || pe->eCmd == CMD_PROGRAM || pe->eCmd == CMD_READ) {
    // Both program and patch need a filename to be valid
    if( ParseXMLString(key,L"filename", pe->filename) != ERROR_SUCCESS ) {
      Log("filename missing in XML line\n");
		  return ERROR_INVALID_DATA;
    } else {
      // Only program if filename is specified
      if( wcslen(pe->filename) == 0 ) {
        pe->eCmd = CMD_NOP;
        return ERROR_SUCCESS;
      }
      Log(L"filename: %s ", pe->filename);
    }

    // File sector offset is optional for both these otherwise use default
    if( ParseXMLInt64(key,L"file_sector_offset", pe->offset, pe) == ERROR_SUCCESS ) {
      Log("file_sector_offset: %I64d ", pe->offset);
    } else {
      pe->offset = (uint64)-1;
    }
	
    // The following entries should only be used in patching
    if( pe->eCmd == CMD_PATCH ) {
      // Get the value parameter
      if( ParseXMLInt64(key,L"value", pe->patch_value, pe) == ERROR_SUCCESS ) {
	      Log("value: %I64d ", pe->patch_value);
	    } else {
        Log("value missing in patch command\n");
        return ERROR_INVALID_DATA;
      }
      Log("crc_size %i\n", (int)pe->crc_size);

      // get byte offset for patch value to be written
      if( ParseXMLInt64(key,L"byte_offset", pe->patch_offset, pe) == ERROR_SUCCESS ) {
	      Log("patch_offset: %I64d ", pe->patch_offset);
	    } else {
        Log("byte_offset missing in patch command\n");
        return ERROR_INVALID_DATA;
      }

      // Get the size of the patch in bytes
      if( ParseXMLInt64(key,L"size_in_bytes", pe->patch_size, pe) == ERROR_SUCCESS ) {
	     Log("patch_size: %I64d ", pe->patch_size);
	    } else {
        Log("size_in_bytes missing in patch command\n");
        return ERROR_INVALID_DATA;
      }

    } // end of CMD_PATCH
  } // end of CMD_PROGRAM || CMD_PATCH
 
  return ERROR_SUCCESS;
}



int Partition::ProgramPartitionEntry(Protocol *proto, PartitionEntry pe, TCHAR *key)
{
  UNREFERENCED_PARAMETER(key);
  HANDLE hRead = INVALID_HANDLE_VALUE;
  bool bSparse = false;
  int status = ERROR_SUCCESS;

  if (proto == NULL) {
    wprintf(L"Can't write to disk no protocol passed in.\n");
    return ERROR_INVALID_PARAMETER;
  }

  if (wcscmp(pe.filename, L"ZERO") == 0) {
    wprintf(L"Zeroing out area\n");
  }
  else {
    // First check if the file is a sparse image then program via sparse
    SparseImage sparse;
    status = sparse.PreLoadImage(pe.filename);
    if (status == ERROR_SUCCESS) {
      bSparse = true;
      status = sparse.ProgramImage(proto, pe.start_sector*proto->GetDiskSectorSize());
    }
    else
    {
      wprintf(L"\nSparse image not detected -- loading binary\n");
      // Open the file that we are supposed to dump
      status = ERROR_SUCCESS;
      hRead = CreateFile(pe.filename,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
      if (hRead == INVALID_HANDLE_VALUE) {
        status = GetLastError();
      }
      else {
        // Update the number of sectors based on real file size, rounded to next sector offset
        DWORD dwUpperFileSize = 0;
        DWORD dwLowerFileSize = GetFileSize(hRead, &dwUpperFileSize);
        __int64 dwTotalSize = dwLowerFileSize + ((__int64)dwUpperFileSize << 32);
        dwTotalSize = (dwTotalSize + proto->GetDiskSectorSize() - 1) & (__int64)~(proto->GetDiskSectorSize() - 1);
        dwTotalSize = dwTotalSize / proto->GetDiskSectorSize();
        if (dwTotalSize <= (__int64)pe.num_sectors) {
          pe.num_sectors = dwTotalSize;
        }
        else {
          wprintf(L"\nFileSize is > partition size, truncating file\n");
        }
        status = ERROR_SUCCESS;
      }
    }
  }

  if (status == ERROR_SUCCESS && !bSparse) {
    // Fast copy from input file to output disk
    wprintf(L"In offset: %I64d out offset: %I64d sectors: %I64d\n", pe.offset, pe.start_sector, pe.num_sectors);
    status = proto->FastCopy(hRead, pe.offset, proto->GetDiskHandle(),  pe.start_sector, pe.num_sectors,pe.physical_partition_number);
    CloseHandle(hRead);
  }
  return status;
}

int Partition::ProgramImage(Protocol *proto)
{
  int status = ERROR_SUCCESS;

  PartitionEntry pe;
  TCHAR keyName[MAX_STRING_LEN];
  TCHAR *key;
  while (GetNextXMLKey(keyName, &key) == ERROR_SUCCESS) {
    // parse the XML key if we don't understand it then continue
    if (ParseXMLKey(key, &pe) != ERROR_SUCCESS) {
      // If we don't understand the command just try sending it otherwise ignore command
      if (pe.eCmd == CMD_INVALID) {
        status = proto->ProgramRawCommand(key);
      }
    }
    else if (pe.eCmd == CMD_PROGRAM) {
      status = ProgramPartitionEntry(proto, pe, key);
    }
    else if (pe.eCmd == CMD_PATCH) {
      // Only patch disk entries
      if (wcscmp(pe.filename, L"DISK") == 0) {
        status = proto->ProgramPatchEntry(pe, key);
      }
    }
    else if (pe.eCmd == CMD_READ) {
      status = proto->DumpDiskContents(pe.start_sector, pe.num_sectors, pe.filename, pe.physical_partition_number, NULL);
    }
    else if (pe.eCmd == CMD_ZEROOUT) {
      status = proto->WipeDiskContents(pe.start_sector, pe.num_sectors, NULL);
    }

    if (status != ERROR_SUCCESS) {
      break;
    }
  }

  CloseXML();
  return status;
}

int Partition::PreLoadImage(TCHAR *fname)
{
  HANDLE hXML;
  int status = ERROR_SUCCESS;
  xmlStart = NULL;

  // Open the XML file and read into RAM
  hXML = CreateFile( fname,
                      GENERIC_READ,
                      FILE_SHARE_READ,
                      NULL,
                      OPEN_EXISTING,
                      0,
                      NULL);
  
  if( hXML == INVALID_HANDLE_VALUE ) {
    return GetLastError();
  }

  DWORD xmlSize = GetFileSize(hXML,NULL);
  size_t sizeOut;

  // Make sure filesize is valid
  if( xmlSize == INVALID_FILE_SIZE ) {
    CloseHandle(hXML);
    return INVALID_FILE_SIZE;
  }

  char *xmlTmp = (char *)malloc(xmlSize);
  xmlStart = (TCHAR *)malloc((xmlSize+2)*sizeof(TCHAR));
  xmlEnd = xmlStart + xmlSize;
  keyStart = xmlStart;

  if( xmlTmp == NULL || xmlStart == NULL ) {
    status = ERROR_OUTOFMEMORY;
  }


  if( status == ERROR_SUCCESS ) {
    if(!ReadFile(hXML,xmlTmp,xmlSize,&xmlSize,NULL) ) {
      status = ERROR_FILE_INVALID;
    }
  }

  CloseHandle(hXML);

  // If successful then prep the buffer
  if( status == ERROR_SUCCESS ) {

    mbstowcs_s(&sizeOut, xmlStart,xmlSize+2, xmlTmp,xmlSize);

    // skip over xml header should be <?xml....?>
    for(keyStart=xmlStart;keyStart < xmlEnd;) {
      if((*keyStart++ == '<') && (*keyStart++ == '?') ) break;
    }

    for(;keyStart < xmlEnd;) {
      if((*keyStart++ == '?') && (*keyStart++ == '>') ) break;   
    }

    // skip the data header for now as hack until we support embedded content
    for(;keyStart < xmlEnd;) {
      if(*keyStart++ == '>' ) break;   
    }

  }

  // Copied this over to wchar buffer free temp buffer 
  if(xmlTmp != NULL) free(xmlTmp);
  
  return status;
}

int Partition::CloseXML(void)
{
  // Free the RAM buffer for storing the XML data
  if( xmlStart != NULL ) {
    delete[] xmlStart;
    xmlStart = NULL;
    xmlEnd = NULL;
    keyStart = NULL;
    keyEnd = NULL;
  }

  return ERROR_SUCCESS;
}

int Partition::GetNextXMLKey(TCHAR *keyName, TCHAR **key)
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
      for(keyEnd = keyStart; !iswalnum(*keyEnd); keyEnd++);
      for(;keyEnd < xmlEnd; keyEnd++) {
        if( *keyEnd == '>' && *(keyEnd-1) == '/') {
          // TODO:FIXME handle multiple embedded parameters
          break;
        } else if( bRecordKey ) {
          if( iswalnum(*keyEnd) ) *keyName++ = *keyEnd;
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
    return ERROR_SUCCESS;
  }
  return ERROR_END_OF_MEDIA;
}
