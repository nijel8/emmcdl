/*****************************************************************************
 * dload.h
 *
 * This file implements the streaming download protocol
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/dload.h#2 $
$DateTime: 2014/08/05 11:44:01 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
11/08/11   pgw     Initial version.
=============================================================================*/
#pragma once

#include "serialport.h"
#include "partition.h"
#include <stdio.h>
#include "sysdeps.h"

#define PACKET_TIMEOUT  1000

#define FEATURE_SECTOR_ADDRESSES   0x00000010

// Packets that are used in dload mode
#define CMD_WRITE  0x01   /* Write a block of data to memory (received)    */
#define CMD_ACK    0x02   /* Acknowledge receiving a packet  (transmitted) */
#define CMD_NAK    0x03   /* Acknowledge a bad packet        (transmitted) */
#define CMD_ERASE  0x04   /* Erase a block of memory         (received)    */
#define CMD_GO     0x05   /* Begin execution at an address   (received)    */
#define CMD_NOP    0x06   /* No operation, for debug         (received)    */
#define CMD_PREQ   0x07   /* Request implementation info     (received)    */
#define CMD_PARAMS 0x08   /* Provide implementation info     (transmitted) */
#define CMD_DUMP   0x09   /* Debug: dump a block of memory   (received)    */
#define CMD_RESET  0x0A   /* Reset the phone                 (received)    */
#define CMD_UNLOCK 0x0B   /* Unlock access to secured ops    (received)    */
#define CMD_VERREQ 0x0C   /* Request software version info   (received)    */
#define CMD_VERRSP 0x0D   /* Provide software version info   (transmitted) */
#define CMD_PWROFF 0x0E   /* Turn phone power off            (received)    */
#define CMD_WRITE_32BIT 0x0F  /* Write a block of data to 32-bit memory */
#define CMD_MEM_DEBUG_QUERY 0x10 /* Memory debug query       (received)    */
#define CMD_MEM_DEBUG_INFO  0x11 /* Memory debug info        (transmitted) */
#define CMD_MEM_READ_REQ    0x12 /* Memory read request      (received)    */
#define CMD_MEM_READ_RESP   0x13 /* Memory read response     (transmitted) */
#define CMD_MEM_UNFRAMED_READ_REQ  0x14 /* Memory unframed read request   (received)      */
#define CMD_MEM_UNFRAMED_READ_RESP 0x15 /* Memory unframed read response  (transmitted)   */
#define CMD_SERIAL_NUM_READ   0x16 /* Return Serial number from efuse read   */
#define CMD_MSM_HW_ID_READ    0x17 /* Return msm_hw_id from efuse read   */
#define CMD_OEM_HASH_KEY_READ 0x18 /* Return oem_pk_hash from efuse read   */
#define CMD_QPST_COOKIE_READ_REQ 0x19 /* QPST cookie read request      (received)    */
#define CMD_QPST_COOKIE_READ_RESP 0x1A /* QPST cookie read response     (transmitted) */      
#define CMD_DLOAD_SWITCH 0x3A

// Partition types
#define PRTN_PBL          1 // don't change value
#define PRTN_QCSBLDHDR    2 // don't change value
#define PRTN_QCSBL        3 // don't change value
#define PRTN_OEMSBL       4 // don't change value
#define PRTN_MODEM        5 // don't change value
#define PRTN_APPS         6 // don't change value
#define PRTN_OTPBL        7 // don't change value
#define PRTN_FOTAUI       8	// don't change value
#define PRTN_CEFS         9 // Write CEFS to the modem f/s
#define PRTN_APPSBL      10 // don't change value
#define PRTN_CEFSAPPS    11 // Write CEFS to the apps f/s
#define PRTN_WINMOBILE   12 // Microsoft WinMobile image
#define PRTN_QDSP6       13 // QDSP6 executable
#define PRTN_USER        14 // User defined partition data
#define PRTN_DBL         15 // SB 2.0 Device boot loader
#define PRTN_OSBL        16 // SB 2.0 OS boot loader
#define PRTN_FSBL        17 // SB 2.0 Failsafe boot loader
#define PRTN_QDSP6_2     18 // dsp2.mbn
#define PRTN_RAWAPPSEFS  19 // Raw (includes ECC + spare data) 
#define PRTN_ROFS1       20 // 0x14 - Symbian
#define PRTN_ROFS2       21 // 0x15 - Symbian
#define PRTN_ROFS3       22 // 0x16 - Symbian
#define PRTN_MBR         32 // mbr.bin for NOR (boot) + MoviNand (AMSS+APPS)
#define PRTN_EMMCUSER    33 // For programming eMMC chip (singleimage.mbn)
#define PRTN_EMMCBOOT0   34 // BOOT and GPP partitions
#define PRTN_EMMCBOOT1   35
#define PRTN_EMMCRPMB    36
#define PRTN_EMMCGPP1    37
#define PRTN_EMMCGPP2    38
#define PRTN_EMMCGPP3    39
#define PRTN_EMMCGPP4    40

// Packets that are used in emergency download mode
#define EHOST_HELLO_REQ  0x01   // Hello request
#define EHOST_HELLO_RSP  0x02   // Hello response
#define EHOST_SIMPLE_READ_REQ   0x03   // Read some number of bytes from phone memory
#define EHOST_SIMPLE_READ_RSP   0x04   // Response to simple read
#define EHOST_SIMPLE_WRITE_REQ  0x05   // Write data without erase
#define EHOST_SIMPLE_WRITE_RSP  0x06   // Response to simple write
#define EHOST_STREAM_WRITE_REQ  0x07   // Streaming write command
#define EHOST_STREAM_WRITE_RSP  0x08   // Response to stream write
#define EHOST_NOP_REQ           0x09   // NOP request
#define EHOST_NOP_RSP           0x0A   // NOP response
#define EHOST_RESET_REQ         0x0B   // Reset target
#define EHSOT_RESET_ACK         0x0C   // Response to reset
#define EHOST_ERROR             0x0D   // Target Error
#define EHOST_LOG               0x0E   // Target informational message
#define EHOST_UNLOCK_REQ        0x0F   // Unlock Target
#define EHOST_UNLOCK_RSP        0x10   // Target unlocked
#define EHOST_POWER_OFF_REQ     0x11   // Power off target
#define EHOST_POWER_OFF_RSP     0x12   // Power off target response
#define EHOST_OPEN_REQ          0x13   // Open for writing image
#define EHOST_OPEN_RSP          0x14   // Response to open for writing image
#define EHOST_CLOSE_REQ         0x15   // Close and flush last partial write to Flash
#define EHOST_CLOSE_RSP         0x16   // Response to close and flush last partial write to Flash
#define EHOST_SECURITY_REQ      0x17   // Send Security mode to use for programming images
#define EHOST_SECURITY_RSP      0x18   // Response to Send Security mode
#define EHOST_PARTITION_REQ     0x19   // Send partition table to use for programming images
#define EHOST_PARTITION_RSP     0x1A   // Response to send partition table
#define EHOST_OPEN_MULTI_REQ    0x1B   // Open for writing image (Multi-image mode only)
#define EHOST_OPEN_MULTI_RSP    0x1C   // Response to open for writing image
#define EHOST_ERASE_FLASH_REQ   0x1D   // Erase entire Flash device
#define EHOST_ERASE_FLASH_RSP   0x1E   // Response to erase Flash
#define EHOST_GET_ECC_REQ       0x1F   // Read current ECC generation status
#define EHOST_GET_ECC_RSP       0x20   // Response to Get ECC state with current status
#define EHOST_SET_ECC_REQ       0x21   // Enable/disable hardware ECC generation
#define EHOST_SET_ECC_RSP       0x22   // Response to set ECC
#define EHOST_STREAM_DLOAD_REQ  0x23   // New streaming download request
#define EHOST_STREAM_DLOAD_RSP  0x24   // New streaming download response
#define EHOST_UNFRAMED_REQ      0x30   // Unframed streaming write command
#define EHOST_UNFRAMED_RSP      0x31   // Unframed streaming write response


class Dload {
public:
  Dload(SerialPort *port);
  int ConnectToFlashProg(unsigned char ver); //ver=2 for byte addressing and ver=3 for sector addressing
  int DeviceReset(void);
  int LoadPartition(char *filename);
  int LoadImage(char *szSingleImage);
  int CreateGPP(uint32_t dwGPP1, uint32_t dwGPP2, uint32_t dwGPP3, uint32_t dwGPP4);
  int SetActivePartition();
  int OpenPartition(int partition);
  int ClosePartition();
  int FastCopySerial(int hInFile, uint32_t sector, uint32_t sectors);
  int LoadFlashProg(char *szFlashPrg);
  int WriteRawProgramFile(char *szXMLFile);
  int GetDloadParams(unsigned char *rsp, int len);
  int IsDeviceInDload(void);
  int WipeDiskContents(__uint64_t start_sector, __uint64_t num_sectors);

private:
  void HexToByte(const char *hex, unsigned char *bin, int len);
  uint32_t HexRunAddress(char *filename);
  uint32_t HexDataLength(char *filename);
  __uint64_t GetNumDiskSectors();
  int ProgramPartitionEntry(PartitionEntry pe);

  SerialPort *sport;
  bool bSectorAddress;
};
