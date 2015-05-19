/*****************************************************************************
 * partition.h
 *
 * This file defines the interface for the Partition class
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/partition.h#5 $
$DateTime: 2015/04/01 17:01:45 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
03/21/11   pgw     Initial version.
=============================================================================*/

#pragma once


#include <windows.h>
#include "xmlparser.h"
#include <stdio.h>

#define MAX_LIST_SIZE   100
#define MAX_PATH_LEN    256
#define SECTOR_SIZE	    512

typedef unsigned __int64 uint64;
class Protocol;

enum cmdEnum {
  CMD_INVALID = 0,
  CMD_PATCH = 1,
  CMD_ZEROOUT = 2,
  CMD_PROGRAM = 3,
  CMD_OPTION = 4,
  CMD_PATH = 5,
  CMD_READ = 6,
  CMD_ERASE = 7,
  CMD_NOP = 8
};

typedef struct {
  char    signature[8];
  __int32 revision;
  __int32 header_size;
  __int32 crc_header;
  __int32 reserved;
  uint64  current_lba;
  uint64  backup_lba;
  uint64  first_lba;
  uint64  last_lba;
  char    disk_guid[16];
  uint64  partition_lba;
  __int32 num_entries;
  __int32 entry_size;
  __int32 crc_partition;
  char    reserved2[420];
} gpt_header_t;

typedef struct {
  char    type_guid[16];
  char    unique_guid[16];
  uint64  first_lba;
  uint64  last_lba;
  uint64  attributes;
  TCHAR   part_name[36];
} gpt_entry_t;

typedef struct {
  cmdEnum eCmd;
  uint64 start_sector;
  uint64 offset;
  uint64 num_sectors;
  UINT8 physical_partition_number;
  uint64 patch_value;
  uint64 patch_offset;
  uint64 patch_size;
  uint64 crc_start;
  uint64 crc_size;
  TCHAR  filename[MAX_PATH];
} PartitionEntry;

TCHAR *StringReplace(TCHAR *inp, TCHAR *find, TCHAR *rep);
TCHAR *StringSetValue(TCHAR *key, TCHAR *keyName, TCHAR *value);


class Partition {
public:
  int num_entries;

  Partition(uint64 ds=0) {num_entries = 0; cur_action = 0; d_sectors = ds;};
  ~Partition() {};
  int PreLoadImage(TCHAR * fname);
  int ProgramImage(Protocol *proto);
  int ProgramPartitionEntry(Protocol *proto, PartitionEntry pe, TCHAR *key);

  int CloseXML();
  int GetNextXMLKey(TCHAR *keyName, TCHAR **key);
  unsigned int CalcCRC32(BYTE *buffer, int len);
  int ParseXMLKey(TCHAR *key, PartitionEntry *pe);

private:
  TCHAR *xmlStart;
  TCHAR *xmlEnd;
  TCHAR *keyStart;
  TCHAR *keyEnd;

  int cur_action;
  uint64 d_sectors;

  int Reflect(int data, int len);
  int ParseXMLOptions();
  int ParsePathList();
  int ParseXMLString(TCHAR *line, TCHAR *key, TCHAR *value);
  int ParseXMLInt64(TCHAR *line, TCHAR *key, uint64 &value, PartitionEntry *pe);
  int ParseXMLEvaluate(TCHAR *expr, uint64 &value, PartitionEntry *pe);
  bool CheckEmptyLine(TCHAR *str);
  int Log(char *str,...);
  int Log(TCHAR *str,...);

  HANDLE hLog;
};
