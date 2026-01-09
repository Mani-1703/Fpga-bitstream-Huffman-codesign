/*
 * sdcard.c
 *
 * Implementation of SD card file I/O helpers
 * using the xilffs (FatFs) library.
 *
 * This module provides a lightweight abstraction
 * over FATFS for use in hardware–software
 * co-designed applications.
 */
#include "sdCard.h"
#include <stdlib.h> // for malloc and free

static FATFS fatfs;

int SD_Init()
{
    FRESULT rc;
    TCHAR *Path = "0:/";
    rc = f_mount(&fatfs, Path, 0);
    if (rc) {
        xil_printf(" ERROR : f_mount returned %d\r\n", rc);
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int SD_Eject()
{
    FRESULT rc;
    TCHAR *Path = "0:/";
    rc = f_mount(NULL, Path, 0); // Correct unmount
    if (rc) {
        xil_printf(" ERROR : f_mount returned %d\r\n", rc);
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int readFile(FIL *fil, u32 DestinationAddress)
{
    FRESULT rc;
    UINT br;
    u32 file_size;

    file_size = f_size(fil);

    rc = f_lseek(fil, 0);
    if (rc) {
        xil_printf(" ERROR : f_lseek returned %d\r\n", rc);
        return XST_FAILURE;
    }

    rc = f_read(fil, (void *)DestinationAddress, file_size, &br);
    if (rc) {
        xil_printf(" ERROR : f_read returned %d\r\n", rc);
        return XST_FAILURE;
    }

    Xil_DCacheFlush();
    return file_size;
}

u32 closeFile(FIL *fptr)
{
    FRESULT rc;
    rc = f_close(fptr);
    if (rc) {
        xil_printf(" ERROR : f_close returned %d\r\n", rc);
        return XST_FAILURE;
    }
    free(fptr); // Free the dynamically allocated FIL
    return XST_SUCCESS;
}

FIL *openFile(char *FileName, char mode)
{
    FIL *fil = (FIL *)malloc(sizeof(FIL)); // dynamically allocate a new FIL
    FRESULT rc;

    if (mode == 'r') {
        rc = f_open(fil, FileName, FA_READ);
    } else if (mode == 'w') {
        rc = f_open(fil, (char *)FileName, FA_CREATE_NEW | FA_WRITE);
        if (rc != FR_OK) {
            rc = f_unlink(FileName); // delete existing
            rc = f_open(fil, (char *)FileName, FA_CREATE_NEW | FA_WRITE);
        }
    } else if (mode == 'a') {
        rc = f_open(fil, (char *)FileName, FA_OPEN_ALWAYS | FA_WRITE);
        if (rc != FR_OK) {
            rc = f_open(fil, (char *)FileName, FA_CREATE_NEW | FA_WRITE);
        } else {
            rc = f_lseek(fil, f_size(fil));
        }
    }

    if (rc) {
        xil_printf(" ERROR : f_open returned %d\r\n", rc);
        free(fil);
        return NULL;
    }

    return fil;
}

int writeFile(FIL *fptr, u32 size, u32 SourceAddress)
{
    UINT btw;
    FRESULT rc;

    rc = f_write(fptr, (const void *)SourceAddress, size, &btw);
    if (rc) {
        xil_printf(" ERROR : f_write returned %d\r\n", rc);
        return XST_FAILURE;
    }

    return btw;
}
