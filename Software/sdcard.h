/*
 * sdcard.c / sdcard.h
 *
 * Original Author: Prof. Vipin K. Menon
 * Source:
 * https://github.com/vipinkmenon/sdReadWrite/tree/master/rgb2grayWithSDCard
 *
 * This file is used for SD card read/write support (FAT32)
 * and has been adapted for use in this project.
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