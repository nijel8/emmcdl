/*****************************************************************************
 * sahara.h
 *
 * This file implements the streaming download protocol
 *
 * Copyright (c) 2007-2012
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/sahara.h#5 $
$DateTime: 2015/04/29 17:06:00 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
10/30/12   pgw     Initial version.
=============================================================================*/
#pragma once

#include "serialport.h"
#include <Windows.h>

#define SAHARA_HELLO_REQ      0x1
#define SAHARA_HELLO_RSP      0x2
#define SAHARA_READ_DATA      0x3
#define SAHARA_END_TRANSFER   0x4
#define SAHARA_DONE_REQ       0x5
#define SAHARA_DONE_RSP       0x6
#define SAHARA_RESET_REQ      0x7
#define SAHARA_RESET_RSP      0x8
#define SAHARA_MEMORY_DEBUG   0x9
#define SAHARA_MEMORY_READ    0xA
#define SAHARA_CMD_READY      0xB
#define SAHARA_SWITCH_MODE    0xC
#define SAHARA_EXECUTE_REQ    0xD
#define SAHARA_EXECUTE_RSP    0xE
#define SAHARA_EXECUTE_DATA   0xF
#define SAHARA_64BIT_MEMORY_DEBUG		0x10
#define SAHARA_64BIT_MEMORY_READ		0x11
#define SAHARA_64BIT_MEMORY_READ_DATA	0x12

#define SAHARA_ERROR_SUCCESS  0x0

enum boot_sahara_exec_cmd_id
{
  SAHARA_EXEC_CMD_NOP = 0x00,
  SAHARA_EXEC_CMD_SERIAL_NUM_READ = 0x01,
  SAHARA_EXEC_CMD_MSM_HW_ID_READ = 0x02,
  SAHARA_EXEC_CMD_OEM_PK_HASH_READ = 0x03,
  SAHARA_EXEC_CMD_SWITCH_TO_DMSS_DLOAD = 0x04,
  SAHARA_EXEC_CMD_SWITCH_TO_STREAM_DLOAD = 0x05,
  SAHARA_EXEC_CMD_READ_DEBUG_DATA = 0x06,
  SAHARA_EXEC_CMD_GET_SOFTWARE_VERSION_SBL = 0x07,

  /* place all new commands above this */
  SAHARA_EXEC_CMD_LAST,
  SAHARA_EXEC_CMD_MAX = 0x7FFFFFFF
};

typedef struct {
	DWORD cmd;
	DWORD len;
	DWORD version;
	DWORD version_min;
	DWORD max_cmd_len;
	DWORD mode;
	DWORD res1;	DWORD res2;	DWORD res3;	DWORD res4;	DWORD res5;	DWORD res6;
} hello_req_t;

typedef struct {
	DWORD cmd;
	DWORD len;
} cmd_hdr_t;

typedef struct {
	DWORD id;
	DWORD data_offset;
	DWORD data_len;
} read_data_t;

typedef struct {
	UINT64 id;
	UINT64 data_offset;
	UINT64 data_len;
} read_data_64_t;

typedef struct {
  DWORD cmd;
  DWORD len;
  DWORD client_cmd;
} execute_cmd_t;

typedef struct {
  DWORD cmd;
  DWORD len;
  DWORD client_cmd;
  DWORD data_len;
} execute_rsp_t;

typedef struct {
	DWORD id;
	DWORD status;
} image_end_t;

typedef struct {
  DWORD cmd;
  DWORD len;
  DWORD status;
} done_t;

typedef struct {
  DWORD serial;
  DWORD msm_id;
  BYTE pk_hash[32];
  DWORD pbl_sw;
} pbl_info_t;

class Sahara {
public:
  Sahara(SerialPort *port,HANDLE hLogFile = NULL);
  int DeviceReset(void);
  int LoadFlashProg(TCHAR *szFlashPrg);
  int ConnectToDevice(bool bReadHello, int mode);
  int DumpDeviceInfo(pbl_info_t *pbl_info);
  bool CheckDevice(void);

private:
  int ModeSwitch(int mode);
  void HexToByte(const char *hex, BYTE *bin, int len);
  void Log(char *str,...);
  void Log(TCHAR *str,...);

  DWORD HexRunAddress(TCHAR *filename);
  DWORD HexDataLength(TCHAR *filename);
  int ReadData(int cmd, BYTE *buf, int len);
  int PblHack(void);

  SerialPort *sport;
  bool bSectorAddress;
  HANDLE hLog;
};
