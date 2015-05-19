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


#include <windows.h>
#include <stdio.h>

#define MAX_STRING_LEN   512
typedef unsigned __int64 uint64;

class XMLParser {
public:
  XMLParser();
  ~XMLParser();
  int LoadXML(TCHAR * fname);
  int ParseXMLString(char *line, char *key, char *value);
  int ParseXMLInteger(char *line, char *key, uint64 *value);
  char *StringReplace(char *inp, char *find, char *rep);
  char *StringSetValue(char *key, char *keyName, char *value);

private:
  char *xmlStart;
  char *xmlEnd;
  char *keyStart;
  char *keyEnd;

  int ParseXMLEvaluate(char *expr, uint64 &value);

};
