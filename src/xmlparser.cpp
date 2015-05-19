/*****************************************************************************
 * xmlparser.cpp
 *
 * This file parses XML contents
 *
 * Copyright (c) 2007-2013
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/xmlparser.cpp#2 $
$DateTime: 2014/08/05 11:44:01 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
05/13/13   pgw     Initial version.
=============================================================================*/

#include "stdio.h"
#include "tchar.h"
#include "xmlparser.h"
#include "winerror.h"
#include <stdlib.h>

XMLParser::XMLParser()
{
  xmlStart = NULL;
  xmlEnd = NULL;
  keyStart = NULL;
  keyEnd = NULL;
}

XMLParser::~XMLParser()
{
  if( xmlStart ) free(xmlStart);
}

int XMLParser::LoadXML(TCHAR *fname)
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

  // Make sure filesize is valid
  if( xmlSize == INVALID_FILE_SIZE ) {
    CloseHandle(hXML);
    return INVALID_FILE_SIZE;
  }

  xmlStart = (char *)malloc(xmlSize+2);
  xmlEnd = xmlStart + xmlSize;
  keyStart = xmlStart;

  if( xmlStart == NULL ) {
    status = ERROR_OUTOFMEMORY;
  }


  if( status == ERROR_SUCCESS ) {
    if(!ReadFile(hXML,xmlStart,xmlSize,&xmlSize,NULL) ) {
      status = ERROR_FILE_INVALID;
    }
  }

  CloseHandle(hXML);

  return status;
}

// Note currently just replace in place and fill in spaces
char *XMLParser::StringSetValue(char *key, char *keyName, char *value)
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

char *XMLParser::StringReplace(char *inp, char *find, char *rep)
{
  char tmpstr[MAX_STRING_LEN];
  int max_len = MAX_STRING_LEN;
  char *tptr = tmpstr;;
  char *sptr = strstr(inp, find);

  if( sptr != NULL ) {
    // Copy part of string before the value to replace
    strncpy_s(tptr,max_len,inp,(sptr-inp));
    max_len -= (sptr-inp);
    tptr += (sptr-inp);
    sptr += strlen(find);
    // Copy the replace value
    strncpy_s(tptr,max_len,rep,strlen(rep));
    max_len -= strlen(rep);
    tptr += strlen(rep);

    // Copy the rest of the string
    strncpy_s(tptr,max_len,sptr,strlen(sptr));

    // Copy new temp string back into original
    strcpy_s(inp,MAX_STRING_LEN,tmpstr);
  }

  return inp;
}

int XMLParser::ParseXMLEvaluate(char *expr, uint64 &value)
{
  // Parse simple expression understands -+/*, NUM_DISK_SECTORS,CRC32(offset:length)
  char *sptr, *sptr1, *sptr2;
  expr = StringReplace(expr,"NUM_DISK_SECTORS","1");
  value = _atoi64(expr);

  sptr = strstr(expr,"CRC32");
  if( sptr != NULL ) {
    char tmp[MAX_STRING_LEN];
    uint64 crc;
    sptr1 = strstr(sptr,"(") + 1;
    if( sptr1 == NULL ) {
      return ERROR_INVALID_PARAMETER;
    }
    sptr2 = strstr(sptr1,",");
    if( sptr2 == NULL ) {
      return ERROR_INVALID_PARAMETER;
    }
    strncpy_s(tmp,MAX_STRING_LEN,sptr1,(sptr2-sptr1));
    ParseXMLEvaluate(tmp, crc);
    sptr1 = sptr2 + 1;
    sptr2 = strstr(sptr1,")");
    if( sptr2 == NULL ) {
      return ERROR_INVALID_PARAMETER;
    }
    strncpy_s(tmp,MAX_STRING_LEN,sptr1,(sptr2-sptr1));
    ParseXMLEvaluate(tmp, crc);
    // Revome the CRC part set value 0
    memset(sptr,' ',(sptr2-sptr+1)*2);
    value = 0;
  }

  sptr = strstr(expr,"*");
  if( sptr != NULL ) {
    // Found * do multiplication
    char val1[64];
    char val2[64];
    strncpy_s(val1,64,expr,(sptr-expr));
    strcpy_s(val2,64,sptr+1);
    value = _atoi64(val1) * _atoi64(val2);
  }

  sptr = strstr(expr,"/");
  if( sptr != NULL ) {
    // Found / do division
    char val1[64];
    char val2[64];
    strncpy_s(val1,64,expr,(sptr-expr));
    strcpy_s(val2,64,sptr+1);

    // Prevent division by 0
    if( _atoi64(val2) > 0 ) {
      value = _atoi64(val1) / _atoi64(val2);
    } else {
      value = 0;
    }
  }

  sptr = strstr(expr,"-");
  if( sptr != NULL ) {
    // Found - do subtraction
    char val1[32];
    char val2[32];
    strncpy_s(val1,32,expr,(sptr-expr));
    strcpy_s(val2,32,sptr+1);
    value = _atoi64(val1) - _atoi64(val2);
  }
  
  sptr = strstr(expr,"+");
  if( sptr != NULL ) {
    // Found + do addition
    char val1[32];
    char val2[32];
    strncpy_s(val1,32,expr,(sptr-expr));
    strcpy_s(val2,32,sptr+1);
    value = _atoi64(val1) + _atoi64(val2);
  }

  return ERROR_SUCCESS;
}


int XMLParser::ParseXMLString(char *line, char *key, char *value)
{
  // Check to make sure none of the parameters are null
  if( line == NULL || key == NULL || value == NULL ) {
    return ERROR_INVALID_PARAMETER;
  }

  char *eptr;
  char *sptr = strstr(line,key);
  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr = strchr(sptr, '"');
  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr++;
  eptr = strchr(sptr, '"');
  if( eptr == NULL) return ERROR_INVALID_DATA;
  // Copy the value between the quotes to output string
  strncpy_s(value,MAX_PATH,sptr,eptr-sptr);

  return ERROR_SUCCESS;
}

int XMLParser::ParseXMLInteger(char *line, char *key, uint64 *value)
{
  // Check to make sure none of the parameters are null
  if( line == NULL || key == NULL || value == NULL ) {
    return ERROR_INVALID_PARAMETER;
  }

  char *eptr;
  char *sptr = strstr(line,key);
  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr = strchr(sptr, '"');
  if( sptr == NULL ) return ERROR_INVALID_DATA;
  sptr++;
  eptr = strchr(sptr, '"');
  if( eptr == NULL) return ERROR_INVALID_DATA;
  // Copy the value between the quotes to output string
  char tmpVal[MAX_STRING_LEN] = {0};
  strncpy_s(tmpVal,sizeof(tmpVal),sptr,min(MAX_STRING_LEN-1,eptr-sptr));

  return  ParseXMLEvaluate(tmpVal,*value);
}
