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

#include "xmlparser.h"
#include "stdio.h"
#include <stdlib.h>

XMLParser::XMLParser()
{
  xmlStart = NULL;
  xmlEnd = NULL;
  keyStart = NULL;
  keyEnd = NULL;
  xmlFilename = NULL;
}

XMLParser::~XMLParser()
{
  if( xmlStart ) {
	  free(xmlStart);
	  xmlStart = NULL;
	  xmlEnd = NULL;
	  keyStart = NULL;
	  keyEnd = NULL;
	  xmlFilename = NULL;

  }
}

int XMLParser::LoadXML(char *fname)
{
	  int hXML;
	  int status = 0;
	  xmlStart = NULL;

	  // Open the XML file and read into RAM
	  hXML = emmcdl_open( fname,O_RDONLY);

	  if( hXML < 0 ) {
	    return errno;
	  }

	  struct stat      my_stat;
	  int ret = fstat(hXML, &my_stat);
	  // Make sure filesize is valid
	  if(ret)
	          return ret;

	  uint32_t xmlSize = my_stat.st_size;

	  // Make sure filesize is valid
	  if( xmlSize < 0 ) {
	    emmcdl_close(hXML);
	    return EINVAL;
	  }

	  char *xmlTmp = (char *)malloc(xmlSize);
	  xmlStart = (char *)malloc((xmlSize+2)*sizeof(char));
	  xmlEnd = xmlStart + xmlSize;
	  keyStart = xmlStart;

	  if( xmlTmp == NULL || xmlStart == NULL ) {
	    status = ENOMEM;
	  }


	  if( status == 0 ) {
	    if((xmlSize = emmcdl_read(hXML,xmlTmp,xmlSize)) < 0 ) {
	      status = EINVAL;
	    }
	  }

	  emmcdl_close(hXML);
	  xmlFilename = fname;

	  // If successful then prep the buffer
	  if( status == 0 ) {

	    strncpy(xmlStart,xmlTmp,xmlSize);
	    xmlStart[xmlSize] = 0;

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


	  if(xmlTmp != NULL) free(xmlTmp);

	  return status;
}

// Note currently just replace in place and fill in spaces
char *XMLParser::StringSetValue(char *key, const char *keyName, char *value) const
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

char *XMLParser::StringReplace(char *inp, const char *find, const char *rep) const
{
  char tmpstr[MAX_STRING_LEN];
  int max_len = MAX_STRING_LEN;
  char *tptr = tmpstr;;
  char *sptr = strstr(inp, find);

  if( sptr != NULL ) {
    // Copy part of string before the value to replace
    strncpy(tptr,inp,(sptr-inp));
    max_len -= (sptr-inp);
    tptr += (sptr-inp);
    sptr += strlen(find);
    // Copy the replace value
    strncpy(tptr,(const char*)rep,strlen(rep));
    max_len -= strlen(rep);
    tptr += strlen(rep);

    // Copy the rest of the string
    strncpy(tptr, (const char*)sptr,strlen(sptr)+1);

    // Copy new temp string back into original
    strcpy(inp,(const char*)tmpstr);
  }

  return inp;
}

int XMLParser::ParseXMLEvaluate(char *expr, __uint64_t &value) const
{
  // Parse simple expression understands -+/*, NUM_DISK_SECTORS,CRC32(offset:length)
  char *sptr, *sptr1, *sptr2;
  expr = StringReplace(expr,"NUM_DISK_SECTORS","1");
  value = atoll(expr);

  sptr = strstr(expr,"CRC32");
  if( sptr != NULL ) {
    char tmp[MAX_STRING_LEN];
    __uint64_t crc;
    sptr1 = strstr(sptr,"(") + 1;
    if( sptr1 == NULL ) {
      return EINVAL;
    }
    sptr2 = strstr(sptr1,",");
    if( sptr2 == NULL ) {
      return EINVAL;
    }
    strncpy(tmp,(const char*)sptr1,(sptr2-sptr1));
    tmp[sptr2-sptr1] = 0;
    ParseXMLEvaluate(tmp, crc);
    sptr1 = sptr2 + 1;
    sptr2 = strstr(sptr1,")");
    if( sptr2 == NULL ) {
      return EINVAL;
    }
    strncpy(tmp,(const char*)sptr1,(sptr2-sptr1));
    tmp[sptr2-sptr1] = 0;
    ParseXMLEvaluate(tmp, crc);
    // Revome the CRC part set value 0
    memset(sptr,' ',(sptr2-sptr+1));
    value = 0;
  }

  sptr = strstr(expr,"*");
  if( sptr != NULL ) {
    // Found * do multiplication
    char val1[64];
    char val2[64];
    strncpy(val1, (const char*)expr,(sptr-expr));
    val1[sptr-expr] = 0;
    strcpy(val2,sptr+1);
    value = atoll(val1) * atoll(val2);
  }

  sptr = strstr(expr,"/");
  if( sptr != NULL ) {
    // Found / do division
    char val1[64];
    char val2[64];
    strncpy(val1, (const char*)expr,(sptr-expr));
    val1[sptr-expr] = 0;
    strcpy(val2, (const char*)sptr+1);

    // Prevent division by 0
    if( atoll(val2) > 0 ) {
      value = atoll(val1) / atoll(val2);
    } else {
      value = 0;
    }
  }

  sptr = strstr(expr,"-");
  if( sptr != NULL ) {
    // Found - do subtraction
    char val1[32];
    char val2[32];
    strncpy(val1, (const char*)expr,(sptr-expr));
    val1[sptr-expr] = 0;
    strcpy(val2,(const char*)sptr+1);
    value = atoll(val1) - atoll(val2);
  }
  
  sptr = strstr(expr,"+");
  if( sptr != NULL ) {
    // Found + do addition
    char val1[32];
    char val2[32];
    strncpy(val1,(const char*)expr,(sptr-expr));
    val1[sptr-expr] = 0;
    strcpy(val2,(const char*)sptr+1);
    value = atoll(val1) + atoll(val2);
  }

  return 0;
}


int XMLParser::ParseXMLString(char *line, const char *key, char *value) const
{
  // Check to make sure none of the parameters are null
  if( line == NULL || key == NULL || value == NULL ) {
    return EINVAL;
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
  strncpy(value,(const char*)sptr,eptr-sptr);
  value[eptr-sptr] = 0;

  return 0;
}

int XMLParser::ParseXMLInteger(char *line,const char *key, __uint64_t *value) const
{
  // Check to make sure none of the parameters are null
  if( line == NULL || key == NULL || value == NULL ) {
    return EINVAL;
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
  char tmpVal[MAX_STRING_LEN];
  strncpy(tmpVal, (const char*)sptr,MIN(MAX_STRING_LEN-1,eptr-sptr));
  tmpVal[MIN(MAX_STRING_LEN-1,eptr-sptr)] = 0;
  return  ParseXMLEvaluate(tmpVal,*value);
}
