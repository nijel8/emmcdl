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


#include "sysdeps.h"

#include "xmlparser.h"
#include <stdio.h>

#define MAX_PATH 512
#define MAX_LIST_SIZE   100
#define MAX_PATH_LEN    256
#define SECTOR_SIZE	    512

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
  CMD_NOP = 8,
  CMD_PEEK= 9
};

typedef struct {
  char    signature[8];
  int32_t revision;
  int32_t header_size;
  int32_t crc_header;
  int32_t reserved;
  __uint64_t current_lba;
  __uint64_t backup_lba;
  __uint64_t first_lba;
  __uint64_t last_lba;
  char    disk_guid[16];
  __uint64_t partition_lba;
  int32_t num_entries;
  int32_t entry_size;
  int32_t crc_partition;
  char    reserved2[420];
} gpt_header_t;

typedef struct {
  char    type_guid[16];
  char    unique_guid[16];
  __uint64_t first_lba;
  __uint64_t last_lba;
  __uint64_t attributes;
  char   part_name[36];
} gpt_entry_t;

typedef struct {
  cmdEnum eCmd;
  __uint64_t start_sector;
  __uint64_t offset;
  __uint64_t num_sectors;
  uint8_t physical_partition_number;
  __uint64_t patch_value;
  __uint64_t patch_offset;
  __uint64_t patch_size;
  __uint64_t crc_start;
  __uint64_t crc_size;
  char  filename[MAX_PATH];
} PartitionEntry;

//char *StringReplace(char *inp, const char *find, const char *rep);
//char *StringSetValue(char *key, char *keyName, char *value);


class Partition:public XMLParser {
public:
  int num_entries;

  Partition(__uint64_t ds=0)
  {
	  num_entries = 0; cur_action = 0; d_sectors = ds;
  };
  ~Partition() {};
  int PreLoadImage(char * fname);
  int ProgramImage(Protocol *proto);
  int ProgramPartitionEntry(Protocol *proto, PartitionEntry pe, char *key);

  int CloseXML();
  int GetNextXMLKey(char *keyName, char **key);
  unsigned int CalcCRC32(unsigned char *buffer, int len);
  int ParseXMLKey(char *key, PartitionEntry *pe);

private:
  int cur_action;
  __uint64_t d_sectors;

  int Reflect(int data, int len);
  int ParseXMLOptions();
  int ParsePathList();
  //int ParseXMLString(char *line, const char *key, char *value);
  int ParseXMLInt64(char *line, const char *key, __uint64_t &value, PartitionEntry *pe) const;
  int ParseXMLEvaluate(char *expr, __uint64_t &value, PartitionEntry *pe) const;
  bool CheckEmptyLine(char *str) const;
  int Log(const char *str,...);

};
