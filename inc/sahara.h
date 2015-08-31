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

#define SAHARA_VERSION 2
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

#define SAHARA_MODE_IMAGE_TX_PENDING  0x0
#define SAHARA_MODE_IMAGE_TX_COMPLETE 0x1
#define SAHARA_MODE_MEMORY_DEBUG      0x2
#define SAHARA_MODE_COMMAND           0x3

/* Status codes for Sahara */
enum boot_sahara_status
{
  /* Success */
  SAHARA_STATUS_SUCCESS =                     0x00,

  /* Invalid command received in current state */
  SAHARA_NAK_INVALID_CMD =                    0x01,

  /* Protocol mismatch between host and target */
  SAHARA_NAK_PROTOCOL_MISMATCH =              0x02,

  /* Invalid target protocol version */
  SAHARA_NAK_INVALID_TARGET_PROTOCOL =        0x03,

  /* Invalid host protocol version */
  SAHARA_NAK_INVALID_HOST_PROTOCOL =          0x04,

  /* Invalid packet size received */
  SAHARA_NAK_INVALID_PACKET_SIZE =            0x05,

  /* Unexpected image ID received */
  SAHARA_NAK_UNEXPECTED_IMAGE_ID =            0x06,

  /* Invalid image header size received */
  SAHARA_NAK_INVALID_HEADER_SIZE =            0x07,

  /* Invalid image data size received */
  SAHARA_NAK_INVALID_DATA_SIZE =              0x08,

  /* Invalid image type received */
  SAHARA_NAK_INVALID_IMAGE_TYPE =             0x09,

  /* Invalid tranmission length */
  SAHARA_NAK_INVALID_TX_LENGTH =              0x0A,

  /* Invalid reception length */
  SAHARA_NAK_INVALID_RX_LENGTH =              0x0B,

  /* General transmission or reception error */
  SAHARA_NAK_GENERAL_TX_RX_ERROR =            0x0C,

  /* Error while transmitting READ_DATA packet */
  SAHARA_NAK_READ_DATA_ERROR =                0x0D,

  /* Cannot receive specified number of program headers */
  SAHARA_NAK_UNSUPPORTED_NUM_PHDRS =          0x0E,

  /* Invalid data length received for program headers */
  SAHARA_NAK_INVALID_PDHR_SIZE =              0x0F,

  /* Multiple shared segments found in ELF image */
  SAHARA_NAK_MULTIPLE_SHARED_SEG =            0x10,

  /* Uninitialized program header location */
  SAHARA_NAK_UNINIT_PHDR_LOC =                0x11,

  /* Invalid destination address */
  SAHARA_NAK_INVALID_DEST_ADDR =              0x12,

  /* Invalid data size receieved in image header */
  SAHARA_NAK_INVALID_IMG_HDR_DATA_SIZE =      0x13,

  /* Invalid ELF header received */
  SAHARA_NAK_INVALID_ELF_HDR =                0x14,

  /* Unknown host error received in HELLO_RESP */
  SAHARA_NAK_UNKNOWN_HOST_ERROR =             0x15,

  /* Timeout while receiving data */
  SAHARA_NAK_TIMEOUT_RX =                     0x16,

  /* Timeout while transmitting data */
  SAHARA_NAK_TIMEOUT_TX =                     0x17,

  /* Invalid mode received from host */
  SAHARA_NAK_INVALID_HOST_MODE =              0x18,

  /* Invalid memory read access */
  SAHARA_NAK_INVALID_MEMORY_READ =            0x19,

  /* Host cannot handle read data size requested */
  SAHARA_NAK_INVALID_DATA_SIZE_REQUEST =      0x1A,

  /* Memory debug not supported */
  SAHARA_NAK_MEMORY_DEBUG_NOT_SUPPORTED =     0x1B,

  /* Invalid mode switch */
  SAHARA_NAK_INVALID_MODE_SWITCH =            0x1C,

  /* Failed to execute command */
  SAHARA_NAK_CMD_EXEC_FAILURE =               0x1D,

  /* Invalid parameter passed to command execution */
  SAHARA_NAK_EXEC_CMD_INVALID_PARAM =         0x1E,

  /* Unsupported client command received */
  SAHARA_NAK_EXEC_CMD_UNSUPPORTED =           0x1F,

  /* Invalid client command received for data response */
  SAHARA_NAK_EXEC_DATA_INVALID_CLIENT_CMD =   0x20,

  /* Failed to authenticate hash table */
  SAHARA_NAK_HASH_TABLE_AUTH_FAILURE =        0x21,

  /* Failed to verify hash for a given segment of ELF image */
  SAHARA_NAK_HASH_VERIFICATION_FAILURE =      0x22,

  /* Failed to find hash table in ELF image */
  SAHARA_NAK_HASH_TABLE_NOT_FOUND =           0x23,

  /* Target failed to initialize */
  SAHARA_NAK_TARGET_INIT_FAILURE =            0x24,

  /* Failed to authenticate generic image */
  SAHARA_NAK_IMAGE_AUTH_FAILURE  =            0x25,

  /* Invalid ELF hash table size.  Too bit or small. */
  SAHARA_NAK_INVALID_IMG_HASH_TABLE_SIZE =    0x26,

  /* Place all new error codes above this */
  SAHARA_NAK_LAST_CODE,

  SAHARA_NAK_MAX_CODE = 0x7FFFFFFF /* To ensure 32-bits wide */
};

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
  int CheckDevice(void);

private:
  int ModeSwitch(int mode, bool rsp = true);
  void HexToByte(const char *hex, unsigned char *bin, int len);
  void Log(const char *str,...);

  uint32_t HexRunAddress(char *filename);
  uint32_t HexDataLength(char *filename);
  int ReadData(int cmd, unsigned char *buf, int len);
  int PblHack(void);

  SerialPort *sport;
  int hLog;
};
