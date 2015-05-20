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
#include "sysdeps.h"

#define SAHARA_VERSION 2.4
#define SAHARA_VERSION_SUPPORTED 2

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
	uint32_t cmd;
	uint32_t len;
	uint32_t version;
	uint32_t version_min;
	uint32_t max_cmd_len;
	uint32_t mode;
	uint32_t res1;	uint32_t res2;	uint32_t res3;	uint32_t res4;	uint32_t res5;	uint32_t res6;
} hello_req_t;

typedef struct {
	uint32_t cmd;
	uint32_t len;
} cmd_hdr_t;

typedef struct {
	uint32_t id;
	uint32_t data_offset;
	uint32_t data_len;
} read_data_t;

typedef struct {
	uint64_t id;
	uint64_t data_offset;
	uint64_t data_len;
} read_data_64_t;

typedef struct {
  uint32_t cmd;
  uint32_t len;
  uint32_t client_cmd;
} execute_cmd_t;

typedef struct {
  uint32_t cmd;
  uint32_t len;
  uint32_t client_cmd;
  uint32_t data_len;
} execute_rsp_t;

typedef struct {
	uint32_t id;
	uint32_t status;
} image_end_t;

typedef struct {
  uint32_t cmd;
  uint32_t len;
  uint32_t status;
} done_t;

typedef struct {
  uint32_t serial;
  uint32_t msm_id;
  unsigned char pk_hash[32];
  uint32_t pbl_sw;
} pbl_info_t;

class Sahara {
public:
  Sahara(SerialPort *port,int hLogFile = 0);
  int DeviceReset(void);
  int LoadFlashProg(char *szFlashPrg);
  int ConnectToDevice(bool bReadHello, int mode);
  int DumpDeviceInfo(pbl_info_t *pbl_info);
  bool CheckDevice(void);

private:
  int ModeSwitch(int mode);
  void HexToByte(const char *hex, unsigned char *bin, int len);
  void Log(const char *str,...);

  uint32_t HexRunAddress(char *filename);
  uint32_t HexDataLength(char *filename);
  int ReadData(int cmd, unsigned char *buf, int len);
  int PblHack(void);

  SerialPort *sport;
  int hLog;
};
