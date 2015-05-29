/*****************************************************************************
 * emmcdl.h
 *
 * This file has all definitions for command line parameters and structures
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/emmcdl.h#3 $
$DateTime: 2014/06/11 17:27:14 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
11/28/11   pgw     Initial version.
=============================================================================*/
#pragma once

#define STREAMING_PROTOCOL  1
#define FIREHOSE_PROTOCOL   2

enum emmc_cmd_e {
  EMMC_CMD_NONE,
  EMMC_CMD_LIST,
  EMMC_CMD_DUMP,
  EMMC_CMD_DUMP_LOG,
  EMMC_CMD_ERASE,
  EMMC_CMD_WRITE,
  EMMC_CMD_WIPE,
  EMMC_CMD_GPP,
  EMMC_CMD_TEST,
  EMMC_CMD_WRITE_GPT,
  EMMC_CMD_RESET,
  EMMC_CMD_FFU,
  EMMC_CMD_LOAD_MRPG,
  EMMC_CMD_GPT,
  EMMC_CMD_SPLIT_FFU,
  EMMC_CMD_RAW,
  EMMC_CMD_LOAD_FFU,
  EMMC_CMD_INFO
};
