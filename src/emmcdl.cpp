/*****************************************************************************
 * emmcdl.cpp
 *
 * This file implements the entry point for the console application.
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/emmcdl.cpp#17 $
$DateTime: 2015/05/07 21:41:17 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
06/28/11   pgw     Call the proper function to wipe the layout rather than manually doing it
05/18/11   pgw     Now supports gpt/crc and changed to use udpated API's
03/21/11   pgw     Initial version.
=============================================================================*/

#include "targetver.h"
#include "emmcdl.h"
#include "partition.h"
#include "diskwriter.h"
#include "dload.h"
#include "sahara.h"
#include "serialport.h"
#include "firehose.h"
#include "ffu.h"
#include "tchar.h"
#include <winerror.h>

using namespace std;

static int m_protocol = FIREHOSE_PROTOCOL;
static int m_chipset = 8974;
static int m_sector_size = 512;
static bool m_emergency = false;
static bool m_verbose = false;
static SerialPort m_port;
static fh_configure_t m_cfg = { 4, "emmc", false, false, true, -1,1024*1024 };

int PrintHelp()
{
  wprintf(L"Usage: emmcdl <option> <value>\n");
  wprintf(L"       Options:\n");
  wprintf(L"       -l                             List available mass storage devices\n");
  wprintf(L"       -info                          List HW information about device attached to COM (eg -p COM8 -info)\n");
  wprintf(L"       -MaxPayloadSizeToTargetInBytes The max bytes in firehose mode (DDR or large IMEM use 16384, default=8192)\n");
  wprintf(L"       -SkipWrite                     Do not write actual data to disk (use this for UFS provisioning)\n");
  wprintf(L"       -SkipStorageInit               Do not initialize storage device (use this for UFS provisioning)\n");
  wprintf(L"       -MemoryName <ufs/emmc>         Memory type default to emmc if none is specified\n");
  wprintf(L"       -SetActivePartition <num>      Set the specified partition active for booting\n");
  wprintf(L"       -disk_sector_size <int>        Dump from start sector to end sector to file\n");
  wprintf(L"       -d <start> <end>               Dump from start sector to end sector to file\n");
  wprintf(L"       -d <PartName>                  Dump entire partition based on partition name\n");
  wprintf(L"       -e <start> <num>               Erase disk from start sector for number of sectors\n");
  wprintf(L"       -e <PartName>                  Erase the entire partition specified\n");
  wprintf(L"       -s <sectors>                   Number of sectors in disk image\n");
  wprintf(L"       -p <port or disk>              Port or disk to program to (eg COM8, for PhysicalDrive1 use 1)\n");
  wprintf(L"       -o <filename>                  Output filename\n");
  wprintf(L"       -x <*.xml>                     Program XML file to output type -o (output) -p (port or disk)\n");
  wprintf(L"       -f <flash programmer>          Flash programmer to load to IMEM eg MPRG8960.hex\n");
  wprintf(L"       -i <singleimage>               Single image to load at offset 0 eg 8960_msimage.mbn\n");
  wprintf(L"       -t                             Run performance tests\n");
  wprintf(L"       -b <prtname> <binfile>         Write <binfile> to GPT <prtname>\n");
  wprintf(L"       -g GPP1 GPP2 GPP3 GPP4         Create GPP partitions with sizes in MB\n");
  wprintf(L"       -gq                            Do not prompt when creating GPP (quiet)\n");
  wprintf(L"       -r                             Reset device\n");
  wprintf(L"       -ffu <*.ffu>                   Download FFU image to device in emergency download need -o and -p\n");
  wprintf(L"       -splitffu <*.ffu> -o <xmlfile> Split FFU into binary chunks and create rawprogram0.xml to output location\n");
  wprintf(L"       -protocol <protocol>           Can be FIREHOSE, STREAMING default is FIREHOSE\n");
  wprintf(L"       -chipset <chipset>             Can be 8960 or 8974 familes\n");
  wprintf(L"       -gpt                           Dump the GPT from the connected device\n");
  wprintf(L"       -raw                           Send and receive RAW data to serial port 0x75 0x25 0x10\n");  
  wprintf(L"       -verbose                       Enable verbose output\n");
  wprintf(L"\n\n\nExamples:");
  wprintf(L" emmcdl -p COM8 -info\n");
  wprintf(L" emmcdl -p COM8 -gpt\n");
  wprintf(L" emmcdl -p COM8 -SkipWrite -SkipStorageInit -MemoryName ufs -f prog_emmc_firehose_8994_lite.mbn -x memory_configure.xml\n");
  wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -x rawprogram0.xml -SetActivePartition 0\n");
  wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -ffu wp8.ffu\n");
  wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -d 0 1000 -o dump_1_1000.bin\n");
  wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -d SVRawDump -o svrawdump.bin\n");
  wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -b SBL1 c:\\temp\\sbl1.mbn\n");
  wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -e 0 100\n");
  wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -e MODEM_FSG\n");
  wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -raw 0x75 0x25 0x10\n");
  return -1;
}

void StringToByte(char **szSerialData, unsigned char *data, int len)
{
  for(int i=0; i < len; i++) {
   char *hex = szSerialData[i];
   if( wcsncmp(hex,L"0x",2) == 0 ) {
     unsigned char val1 = (unsigned char)(hex[2] - '0');
     unsigned char val2 = (unsigned char)(hex[3] - '0');
     if( val1 > 9 ) val1 = val1 - 7;
     if( val2 > 9 ) val2 = val2 - 7;
     data[i] = (val1 << 4) + val2;
   } else {
      data[i] = (unsigned char)_wtoi(szSerialData[i]);
   }
  }
}

int RawSerialSend(int dnum, char **szSerialData, int len)
{
  int status = ERROR_SUCCESS;
  unsigned char data[256];

  // Make sure the number of bytes of data we are trying to send is valid
  if( len < 1 || len > sizeof(data) ) {
    return ERROR_INVALID_PARAMETER;
  }

  m_port.Open(dnum);
  StringToByte(szSerialData,data,len);
  status = m_port.Write(data,len);
  return status;
}


int LoadFlashProg(char *mprgFile)
{
  int status = ERROR_SUCCESS;
  // This is PBL so depends on the chip type

  if( m_chipset == 8974 ) {
    Sahara sh(&m_port);
    if( status != ERROR_SUCCESS ) return status;
    status = sh.ConnectToDevice(true,0);
    if( status != ERROR_SUCCESS ) return status;
    wprintf(L"Downloading flash programmer: %s\n",mprgFile);
    status = sh.LoadFlashProg(mprgFile);
    if( status != ERROR_SUCCESS ) return status;
  } else {
    Dload dl(&m_port);
    if( status != ERROR_SUCCESS ) return status;
    status = dl.IsDeviceInDload();
    if( status != ERROR_SUCCESS ) return status;
    wprintf(L"Downloading flash programmer: %s\n",mprgFile);
    status = dl.LoadFlashProg(mprgFile);
    if( status != ERROR_SUCCESS ) return status;
  }
  return status;
}

int EraseDisk(__uint64_t start, __uint64_t num, int dnum, char *szPartName)
{
  int status = ERROR_SUCCESS;

  if (m_emergency) {
	  Firehose fh(&m_port);
    fh.SetDiskSectorSize(m_sector_size);
    if (m_verbose) fh.EnableVerbose();
	  status = fh.ConnectToFlashProg(&m_cfg);
	  if (status != ERROR_SUCCESS) return status;
	  wprintf(L"Connected to flash programmer, starting download\n");
	  fh.WipeDiskContents(start, num, szPartName);
  } else {
    DiskWriter dw;
    // Initialize and print disk list
    dw.InitDiskList(false);
  
    status = dw.OpenDevice(dnum);
    if( status == ERROR_SUCCESS ) {
      wprintf(L"Successfully opened volume\n");
      wprintf(L"Erase at start_sector %I64d: num_sectors: %I64d\n",start, num);
      status = dw.WipeDiskContents( start,num, szPartName );
    }
    dw.CloseDevice();
  }
  return status;
}

int DumpDeviceInfo(void)
{
  int status = ERROR_SUCCESS;
  Sahara sh(&m_port);
  if (m_protocol == FIREHOSE_PROTOCOL) {
    pbl_info_t pbl_info;
    status = sh.DumpDeviceInfo(&pbl_info);
    if (status == ERROR_SUCCESS) {
      wprintf(L"SerialNumber: 0x%08x\n", pbl_info.serial);
      wprintf(L"MSM_HW_ID: 0x%08x\n", pbl_info.msm_id);
      wprintf(L"OEM_PK_HASH: 0x");
      for (int i = 0; i < sizeof(pbl_info.pk_hash); i++) {
        wprintf(L"%02x", pbl_info.pk_hash[i]);
      }
      wprintf(L"\nSBL SW Version: 0x%08x\n", pbl_info.pbl_sw);
    }
  }
  else {
    wprintf(L"Only devices with Sahara support this information\n");
    status = ERROR_INVALID_PARAMETER;
  }

  return status;
}

// Wipe out existing MBR, Primary GPT and secondary GPT
int WipeDisk(int dnum)
{
  DiskWriter dw;
  int status;

  // Initialize and print disk list
  dw.InitDiskList();
  
  status = dw.OpenDevice(dnum);
  if( status == ERROR_SUCCESS ) {
    wprintf(L"Successfully opened volume\n");
    wprintf(L"Wipping GPT and MBR\n");
    status = dw.WipeLayout();
  }
  dw.CloseDevice();
  return status;
}

int CreateGPP(uint32_t dwGPP1, uint32_t dwGPP2, uint32_t dwGPP3, uint32_t dwGPP4)
{
  int status = ERROR_SUCCESS;

  if( m_protocol == STREAMING_PROTOCOL ) { 
    Dload dl(&m_port);
  
    // Wait for device to re-enumerate with flashprg
    status = dl.ConnectToFlashProg(4);
    if( status != ERROR_SUCCESS ) return status;
    status = dl.OpenPartition(PRTN_EMMCUSER);
    if( status != ERROR_SUCCESS ) return status;
    wprintf(L"Connected to flash programmer, creating GPP\n");
    status = dl.CreateGPP(dwGPP1,dwGPP2,dwGPP3,dwGPP4);
  
  } else if(m_protocol == FIREHOSE_PROTOCOL) {
    Firehose fh(&m_port);
    fh.SetDiskSectorSize(m_sector_size);
    if (m_verbose) fh.EnableVerbose();
    status = fh.ConnectToFlashProg(&m_cfg);
    if( status != ERROR_SUCCESS ) return status;
    wprintf(L"Connected to flash programmer, creating GPP\n");
    status = fh.CreateGPP(dwGPP1/2,dwGPP2/2,dwGPP3/2,dwGPP4/2);
    status = fh.SetActivePartition(1);
  }    
  
  return status;
}

int ReadGPT(int dnum)
{
  int status;
  
  if( m_emergency ) {
    Firehose fh(&m_port);
    fh.SetDiskSectorSize(m_sector_size);
    if(m_verbose) fh.EnableVerbose();
    status = fh.ConnectToFlashProg(&m_cfg);
    if( status != ERROR_SUCCESS ) return status;
    wprintf(L"Connected to flash programmer, starting download\n");
    fh.ReadGPT(true);
  } else {
    DiskWriter dw;
    dw.InitDiskList();
    status = dw.OpenDevice(dnum);
  
    if( status == ERROR_SUCCESS ) {
      status = dw.ReadGPT(true);
    }

    dw.CloseDevice();
  }
  return status;
}

int WriteGPT(int dnum, char *szPartName, char *szBinFile)
{
  int status;

  if (m_emergency) {
    Firehose fh(&m_port);
    fh.SetDiskSectorSize(m_sector_size);
    if (m_verbose) fh.EnableVerbose();
    status = fh.ConnectToFlashProg(&m_cfg);
    if (status != ERROR_SUCCESS) return status;
    wprintf(L"Connected to flash programmer, starting download\n");
    status = fh.WriteGPT(szPartName, szBinFile);
  }
  else {
    DiskWriter dw;
    dw.InitDiskList();
    status = dw.OpenDevice(dnum);
    if (status == ERROR_SUCCESS) {
      status = dw.WriteGPT(szPartName, szBinFile);
    }
    dw.CloseDevice();
  }
  return status;
}

int ResetDevice()
{
  Dload dl(&m_port);
  int status = ERROR_SUCCESS;
  if( status != ERROR_SUCCESS ) return status;
  status = dl.DeviceReset();
  return status;
}

int FFUProgram(char *szFFUFile)
{
  FFUImage ffu;
  int status = ERROR_SUCCESS;
  Firehose fh(&m_port);
  fh.SetDiskSectorSize(m_sector_size);
  status = fh.ConnectToFlashProg(&m_cfg);
  if (status != ERROR_SUCCESS) return status;
  wprintf(L"Trying to open FFU\n");
  status = ffu.PreLoadImage(szFFUFile);
  if (status != ERROR_SUCCESS) return status;
  wprintf(L"Valid FFU found trying to program image\n");
  status = ffu.ProgramImage(&fh, 0);
  ffu.CloseFFUFile();
  return status;
}

int FFULoad(char *szFFUFile, char *szPartName, char *szOutputFile)
{
  int status = ERROR_SUCCESS;
  wprintf(_T("Loading FFU\n"));
  if( (szFFUFile != NULL) && (szOutputFile != NULL)) {
    FFUImage ffu;
    ffu.SetDiskSectorSize(m_sector_size);
    status = ffu.PreLoadImage(szFFUFile);
    if( status == ERROR_SUCCESS ) 
      status = ffu.SplitFFUBin(szPartName,szOutputFile);
    status = ffu.CloseFFUFile();
  } else {
    return PrintHelp();
  }
  return status;
}

int FFURawProgram(char *szFFUFile, char *szOutputFile)
{
  int status = ERROR_SUCCESS;
  wprintf(_T("Creating rawprogram and files\n"));
  if( (szFFUFile != NULL) && (szOutputFile != NULL)) {
    FFUImage ffu;
    ffu.SetDiskSectorSize(m_sector_size);
    status = ffu.FFUToRawProgram(szFFUFile,szOutputFile);
    ffu.CloseFFUFile();
  } else {
    return PrintHelp();
  }
  return status;
}


int EDownloadProgram(char *szSingleImage, char **szXMLFile)
{
  int status = ERROR_SUCCESS;
  Dload dl(&m_port);
  Firehose fh(&m_port);
  unsigned char prtn=0;

  if( szSingleImage != NULL ) {
    // Wait for device to re-enumerate with flashprg
    status = dl.ConnectToFlashProg(2);
    if( status != ERROR_SUCCESS ) return status;
    wprintf(L"Connected to flash programmer, starting download\n");
    dl.OpenPartition(PRTN_EMMCUSER);
    status = dl.LoadImage(szSingleImage);
    dl.ClosePartition();
  } else if( szXMLFile[0] != NULL ) {
    // Wait for device to re-enumerate with flashprg
    if( m_protocol == STREAMING_PROTOCOL ) {
      status = dl.ConnectToFlashProg(4);
      if( status != ERROR_SUCCESS ) return status;
      wprintf(L"Connected to flash programmer, starting download\n");
      
      // Download all XML files to device 
      for(int i=0; szXMLFile[i] != NULL; i++) {
        // Use new method to download XML to serial port
        char szPatchFile[MAX_STRING_LEN];
        wcsncpy_s(szPatchFile,szXMLFile[i],sizeof(szPatchFile));
        StringReplace(szPatchFile,L"rawprogram",L"patch");
        char *sptr = wcsstr(szXMLFile[i],L".xml");
        if( sptr == NULL ) return ERROR_INVALID_PARAMETER;
        prtn = (unsigned char)((*--sptr) - '0' + PRTN_EMMCUSER);
        wprintf(L"Opening partition %i\n",prtn);
        dl.OpenPartition(prtn);
        //Sleep(1);
        status = dl.WriteRawProgramFile(szPatchFile);
        if( status != ERROR_SUCCESS ) return status;
        status = dl.WriteRawProgramFile(szXMLFile[i]);
      }
      wprintf(L"Setting Active partition to %i\n",(prtn - PRTN_EMMCUSER));  
      dl.SetActivePartition();
      dl.DeviceReset();
      dl.ClosePartition();
    } else if(m_protocol == FIREHOSE_PROTOCOL) {
      fh.SetDiskSectorSize(m_sector_size);
      if(m_verbose) fh.EnableVerbose();
      status = fh.ConnectToFlashProg(&m_cfg);
      if( status != ERROR_SUCCESS ) return status;
      wprintf(L"Connected to flash programmer, starting download\n");

      // Download all XML files to device
      for (int i = 0; szXMLFile[i] != NULL; i++) {
        Partition rawprg(0);
        status = rawprg.PreLoadImage(szXMLFile[i]);
        if (status != ERROR_SUCCESS) return status;
        status = rawprg.ProgramImage(&fh);

        // Only try to do patch if filename has rawprogram in it
        char *sptr = wcsstr(szXMLFile[i], L"rawprogram");
        if (sptr != NULL && status == ERROR_SUCCESS) {
          Partition patch(0);
          int pstatus = ERROR_SUCCESS;
          // Check if patch file exist
          char szPatchFile[MAX_STRING_LEN];
          wcsncpy_s(szPatchFile, szXMLFile[i], sizeof(szPatchFile));
          StringReplace(szPatchFile, L"rawprogram", L"patch");
          pstatus = patch.PreLoadImage(szPatchFile);
          if( pstatus == ERROR_SUCCESS ) patch.ProgramImage(&fh);
        }
      }

      // If we want to set active partition then do that here
      if (m_cfg.ActivePartition >= 0) {
        status = fh.SetActivePartition(m_cfg.ActivePartition);
      }
    }
  }

  return status;
}


int RawDiskProgram(char **pFile, char *oFile, __uint64_t dnum)
{
  DiskWriter dw;
  int status = ERROR_SUCCESS;

  // Depending if we want to write to disk or file get appropriate handle
  if( oFile != NULL ) {
    status = dw.OpenDiskFile(oFile,dnum);
  } else {
    int disk = dnum & 0xFFFFFFFF;
    // Initialize and print disk list
    dw.InitDiskList();
    status = dw.OpenDevice(disk);
  }
  if( status == ERROR_SUCCESS ) {
    wprintf(L"Successfully opened disk\n");
    for(int i=0; pFile[i] != NULL; i++) {
      Partition p(dw.GetNumDiskSectors());
      status = p.PreLoadImage(pFile[i]);
      if (status != ERROR_SUCCESS) return status;
      status = p.ProgramImage(&dw);
    }
  }

  dw.CloseDevice();
  return status;
}

int RawDiskTest(int dnum, __uint64_t offset)
{
  DiskWriter dw;
  int status = ERROR_SUCCESS;
  offset = 0x2000000;

  // Initialize and print disk list
  dw.InitDiskList();
  status = dw.OpenDevice(dnum);
  if( status == ERROR_SUCCESS ) {
    wprintf(L"Successfully opened volume\n");
    //status = dw.CorruptionTest(offset);
    status = dw.DiskTest(offset);
  } else {
    wprintf(L"Failed to open volume\n");
  }

  dw.CloseDevice();
  return status;
}

int RawDiskDump(__uint64_t start, __uint64_t num, char *oFile, int dnum, char *szPartName)
{
  DiskWriter dw;
  int status = ERROR_SUCCESS;

  // Get extra info from the user via command line
  wprintf(L"Dumping at start sector: %I64d for sectors: %I64d to file: %s\n",start, num, oFile);
  if( m_emergency ) {
    Firehose fh(&m_port);
    fh.SetDiskSectorSize(m_sector_size);
    if(m_verbose) fh.EnableVerbose();
    if( status != ERROR_SUCCESS ) return status;
    status = fh.ConnectToFlashProg(&m_cfg);
    if( status != ERROR_SUCCESS ) return status;
    wprintf(L"Connected to flash programmer, starting dump\n");
    status = fh.DumpDiskContents(start,num,oFile,0,szPartName);
  } else {
    // Initialize and print disk list
    dw.InitDiskList();
    status = dw.OpenDevice(dnum);
    if( status == ERROR_SUCCESS ) {
      wprintf(L"Successfully opened volume\n");
      status = dw.DumpDiskContents(start,num,oFile,0,szPartName);
    }
    dw.CloseDevice();
  }
  return status;
}

int DiskList()
{
  DiskWriter dw;
  dw.InitDiskList();
  
  return ERROR_SUCCESS;
}

int __cdecl _tmain(int argc, _char* argv[])
{
  int dnum = -1;
  int status = 0;
  bool bEmergdl = FALSE;
  char *szOutputFile = NULL;
  char *szXMLFile[8] = {NULL};
  char **szSerialData = {NULL};
  uint32_t dwXMLCount = 0;
  char *szFFUImage = NULL;
  char *szFlashProg = NULL;
  char *szSingleImage = NULL;
  char *szPartName = NULL;
  emmc_cmd_e cmd = EMMC_CMD_NONE;
  __uint64_t uiStartSector = 0;
  __uint64_t uiNumSectors = 0;
  __uint64_t uiOffset = 0x40000000;
  uint32_t dwGPP1=0,dwGPP2=0,dwGPP3=0,dwGPP4=0;
  bool bGppQuiet = FALSE;

  // Print out the version first thing so we know this
  wprintf(_T("Version %i.%i\n"), VERSION_MAJOR, VERSION_MINOR);

  if( argc < 2) {
    return PrintHelp();
  }

  // Loop through all our input arguments 
  for(int i=1; i < argc; i++) {
    // Do a list inline
    if( _wcsicmp(argv[i], L"-l") == 0 ) {
      DiskWriter dw;
      dw.InitDiskList(false);
    }
    if (_wcsicmp(argv[i], L"-lv") == 0) {
      DiskWriter dw;
      dw.InitDiskList(true);
    }
    if (_wcsicmp(argv[i], L"-r") == 0) {
      cmd = EMMC_CMD_RESET;
    }
    if (_wcsicmp(argv[i], L"-o") == 0) {
      // Update command with output filename
      szOutputFile = argv[++i];
    }
    if (_wcsicmp(argv[i], L"-disk_sector_size") == 0) {
      // Update the global disk sector size
      m_sector_size = _wtoi(argv[++i]);
    }
    if (_wcsicmp(argv[i], L"-d") == 0) {
      // Dump from start for number of sectors
      cmd = EMMC_CMD_DUMP;
      // If the next param is alpha then pass in as partition name other wise use sectors
      if (iswdigit(argv[i+1][0])) {
        uiStartSector = _wtoi(argv[++i]);
        uiNumSectors = _wtoi(argv[++i]);
      } else {
        szPartName = argv[++i];
      }
    }
    if (_wcsicmp(argv[i], L"-e") == 0) {
      cmd = EMMC_CMD_ERASE;
      // If the next param is alpha then pass in as partition name other wise use sectors
      if (iswdigit(argv[i + 1][0])) {
        uiStartSector = _wtoi(argv[++i]);
        uiNumSectors = _wtoi(argv[++i]);
      }
      else {
        szPartName = argv[++i];
      }
    }
    if (_wcsicmp(argv[i], L"-w") == 0) {
      cmd = EMMC_CMD_WIPE;
    }
    if (_wcsicmp(argv[i], L"-x") == 0) {
      cmd = EMMC_CMD_WRITE;
      szXMLFile[dwXMLCount++] = argv[++i];
    }
    if (_wcsicmp(argv[i], L"-p") == 0) {
      // Everyone wants to use format COM8 so detect this and accept this as well
      if( _wcsnicmp(argv[i+1], L"COM",3) == 0 ) {
        dnum = _wtoi((argv[++i]+3));
      } else {
        dnum = _wtoi(argv[++i]);
      }
    }
    if (_wcsicmp(argv[i], L"-s") == 0) {
      uiNumSectors = _wtoi(argv[++i]);
    }
    if (_wcsicmp(argv[i], L"-f") == 0) {
      szFlashProg = argv[++i];
      bEmergdl = true;
    }
    if (_wcsicmp(argv[i], L"-i") == 0) {
      cmd = EMMC_CMD_WRITE;
      szSingleImage = argv[++i];
      bEmergdl = true;
    }
    if (_wcsicmp(argv[i], L"-t") == 0) {
      cmd = EMMC_CMD_TEST;
      if( i < argc ) {
        uiOffset = (__uint64_t )(_wtoi(argv[++i])) * 512;
      }
    }
    if (_wcsicmp(argv[i], L"-g") == 0) {
      if( (i + 4) < argc ) {
        cmd = EMMC_CMD_GPP;
        dwGPP1 = _wtoi(argv[++i]);
        dwGPP2 = _wtoi(argv[++i]);
        dwGPP3 = _wtoi(argv[++i]);
        dwGPP4 = _wtoi(argv[++i]);
      } else {
        PrintHelp();
      }
    }
    if (_wcsicmp(argv[i], L"-gq") == 0) {
      bGppQuiet = TRUE;
	}
    if (_wcsicmp(argv[i], L"-b") == 0) {
      if( (i+2) < argc ) {
        cmd = EMMC_CMD_WRITE_GPT;
        szPartName = argv[++i];
        szSingleImage = argv[++i];
      } else {
	      PrintHelp();
      }
    }

    if (_wcsicmp(argv[i], L"-gpt") == 0) {
      cmd = EMMC_CMD_GPT;
    }

    if (_wcsicmp(argv[i], L"-info") == 0) {
      cmd = EMMC_CMD_INFO;
    }

    if (_wcsicmp(argv[i], L"-ffu") == 0) {
      if( (i+1) < argc ) {
        szFFUImage = argv[++i];
        cmd = EMMC_CMD_LOAD_FFU;
      } else {
        PrintHelp();
      }
    }

    if (_wcsicmp(argv[i], L"-dumpffu") == 0) {
      if( (i+1) < argc ) {
        szFFUImage = argv[++i];
        szPartName = argv[++i];
        cmd = EMMC_CMD_FFU;
      } else {
        PrintHelp();
      }
    }

    if (_wcsicmp(argv[i], L"-raw") == 0) {
      if( (i+1) < argc ) {
        szSerialData = &argv[i+1];
        cmd = EMMC_CMD_RAW;
      } else {
        PrintHelp();
      }
	  break;
    }

    if (_wcsicmp(argv[i], L"-splitffu") == 0) {
      if( (i+1) < argc ) {
        szFFUImage = argv[++i];
        cmd = EMMC_CMD_SPLIT_FFU;
      } else {
        PrintHelp();
      }
    }

    if (_wcsicmp(argv[i], L"-protocol") == 0) {
      if( (i+1) < argc ) {
        if( wcscmp(argv[i+1], L"STREAMING") == 0 ) {
          m_protocol = STREAMING_PROTOCOL;
        } else if( wcscmp(argv[i+1], L"FIREHOSE") == 0 ) {
          m_protocol = FIREHOSE_PROTOCOL;
        }
      } else {
        PrintHelp();
      }
    }

    if (_wcsicmp(argv[i], L"-chipset") == 0) {
      if( (i+1) < argc ) {
        if (_wcsicmp(argv[i + 1], L"8960") == 0) {
          m_chipset = 8960;
        } else if( _wcsicmp(argv[i+1], L"8974") == 0 ) {
          m_chipset = 8974;
        }
      } else {
        PrintHelp();
      }
    }

    if (_wcsicmp(argv[i], L"-MaxPayloadSizeToTargetInBytes") == 0) {
      if ((i + 1) < argc) {
        m_cfg.MaxPayloadSizeToTargetInBytes = _wtoi(argv[++i]);
      }
      else {
        PrintHelp();
      }
    }

    if (_wcsicmp(argv[i], L"-SkipWrite") == 0) {
      m_cfg.SkipWrite = true;
    }

    if (_wcsicmp(argv[i], L"-SkipStorageInit") == 0) {
      m_cfg.SkipStorageInit = true;
    }

    if (_wcsicmp(argv[i], L"-SetActivePartition") == 0) {
      if ((i + 1) < argc) {
        m_cfg.ActivePartition = _wtoi(argv[++i]);
      } else {
        PrintHelp();
      }
    }

    if (_wcsicmp(argv[i], L"-MemoryName") == 0) {
      if ((i + 1) < argc) {
        i++;
        if (_wcsicmp(argv[i], L"emmc") == 0) {
          strcpy_s(m_cfg.MemoryName,sizeof(m_cfg.MemoryName),"emmc");
        }
        else if (_wcsicmp(argv[i], L"ufs") == 0) {
          strcpy_s(m_cfg.MemoryName,sizeof(m_cfg.MemoryName), "ufs");
        }
      }
      else {
        PrintHelp();
      }
    }
  }
  
  if( szFlashProg != NULL ) {
     m_port.Open(dnum);
     status = LoadFlashProg(szFlashProg);
     if (status == ERROR_SUCCESS) {
       wprintf(_T("Waiting for flash programmer to boot\n"));
       Sleep(2000);
     }
     else {
       wprintf(_T("\n!!!!!!!! WARNING: Flash programmer failed to load trying to continue !!!!!!!!!\n\n"));
     }
     m_emergency = true;
  }

  // If there is a special command execute it
  switch(cmd) {
  case EMMC_CMD_DUMP:
    if( szOutputFile && (dnum >= 0)) {
      wprintf(_T("Dumping data to file %s\n"),szOutputFile);
      status = RawDiskDump(uiStartSector, uiNumSectors, szOutputFile, dnum, szPartName);
    } else {
      return PrintHelp();
    }
    break;
  case EMMC_CMD_ERASE:
    wprintf(_T("Erasing Disk\n"));
    status = EraseDisk(uiStartSector,uiNumSectors,dnum, szPartName);
    break;
  case EMMC_CMD_SPLIT_FFU:
    status = FFURawProgram(szFFUImage,szOutputFile);
    break;
  case EMMC_CMD_FFU:
    status = FFULoad(szFFUImage,szPartName,szOutputFile);
    break;
  case EMMC_CMD_LOAD_FFU:
    status = FFUProgram(szFFUImage);
    break;
  case EMMC_CMD_WRITE:
    if( (szXMLFile[0]!= NULL) && (szSingleImage != NULL)) {
      return PrintHelp();
    }
    if( m_emergency ) {
      status = EDownloadProgram(szSingleImage, szXMLFile);
    } else {
      wprintf(_T("Programming image\n"));
      status = RawDiskProgram(szXMLFile, szOutputFile, dnum);
    }
    break;
  case EMMC_CMD_WIPE:
    wprintf(_T("Wipping Disk\n"));
    if( dnum > 0 ) {
      status = WipeDisk(dnum);
    }
    break;
  case EMMC_CMD_RAW:
    wprintf(_T("Sending RAW data to COM%i\n"),dnum);
    status = RawSerialSend(dnum, szSerialData,argc-4);
    break;
  case EMMC_CMD_TEST:
    wprintf(_T("Running performance tests disk %i\n"),dnum);
    status = RawDiskTest(dnum,uiOffset);
    break;
  case EMMC_CMD_GPP:
    wprintf(_T("Create GPP1=%iMB, GPP2=%iMB, GPP3=%iMB, GPP4=%iMB\n"),(int)dwGPP1,(int)dwGPP2,(int)dwGPP3,(int)dwGPP4);
	if(!bGppQuiet) {
      wprintf(_T("Are you sure? (y/n)"));
      if( getchar() != 'y') {
        wprintf(_T("\nGood choice back to safety\n"));
		break;
	  }
	}
    wprintf(_T("Sending command to create GPP\n"));
    status = CreateGPP(dwGPP1*1024*2,dwGPP2*1024*2,dwGPP3*1024*2,dwGPP4*1024*2);
    if( status == ERROR_SUCCESS ) {
      wprintf(_T("Power cycle device to complete operation\n"));
    }
	break;
  case EMMC_CMD_WRITE_GPT:
    if( (szSingleImage != NULL) && (szPartName != NULL) && (dnum >=0) ) {
      status = WriteGPT(dnum, szPartName, szSingleImage);
    }
    break;
  case EMMC_CMD_RESET:
	  status = ResetDevice();
	  break;
  case EMMC_CMD_LOAD_MRPG:
    break;
  case EMMC_CMD_GPT:
    // Read and dump GPT information from given disk
    status = ReadGPT(dnum);
    break;
  case EMMC_CMD_INFO:
    m_port.Open(dnum); 
    status = DumpDeviceInfo();
    break;
  case EMMC_CMD_NONE:
    break;
  }
 
  // Print error information
  LPVOID lpMsgBuf;

  FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        status,
        MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

  // Display the error message and exit the process
  wprintf(_T("Status: %i %s\n"),status, (char*)lpMsgBuf);
  return status;
}
