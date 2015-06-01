/*****************************************************************************
 * xmlparser.h
 *
 * This file is used to parse xml files and contents
 *
 * Copyright (c) 2007-2013
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/xmlparser.h#2 $
$DateTime: 2014/08/05 11:44:01 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
05/13/13   pgw     Initial version.
=============================================================================*/

#pragma once


#include "sysdeps.h"
#include <stdio.h>

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX_STRING_LEN   512
#define ERROR_INVALID_DATA  -10

class XMLParser {
public:
  XMLParser();
  ~XMLParser();
  int LoadXML(char * fname);
  int ParseXMLString(char *line, const char *key, char *value) const;
  int ParseXMLInteger(char *line, const char *key, __uint64_t *value) const;
  char *StringReplace(char *inp, const char *find, const char *rep) const;
  char *StringSetValue(char *key, const char *keyName, char *value) const;

protected:
  char *xmlStart;
  char *xmlEnd;
  char *keyStart;
  char *keyEnd;
  const char *xmlFilename;


  int ParseXMLEvaluate(char *expr, __uint64_t &value) const;

};
