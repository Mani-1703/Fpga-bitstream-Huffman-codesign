/*
 * sdcard.h
 *
 * Simple SD card and FATFS helper functions for
 * Zynq bare-metal applications using xilffs.
 *
 * Target platform : ZedBoard (Zynq-7000)
 */
#include <xil_types.h>
#include "ff.h"
#include "xil_printf.h"
#include <xstatus.h>
#include "xil_cache.h"

int SD_Init();
int SD_Eject();
FIL* openFile(char *FileName,char mode);
u32 closeFile(FIL* fptr);
int readFile(FIL *fil, u32 DestinationAddress);
int writeFile(FIL* fptr, u32 size, u32 SourceAddress);