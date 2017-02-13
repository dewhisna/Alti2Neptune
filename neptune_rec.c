/*
 * Neptune_Rec
 *
 * This module reads and parses neptune data -- either directly
 * from the device or our special .nep files.
 *
 * Written May 12, 2004 by Donna Whisnant
 * Copyright(C)2004 by Donna Whisnant
 *
 * GNU General Public License Usage
 * This file may be used under the terms of the GNU General Public License
 * version 2.0 as published by the Free Software Foundation and appearing
 * in the file gpl-2.0.txt included in the packaging of this file. Please
 * review the following information to ensure the GNU General Public License
 * version 2.0 requirements will be met:
 * http://www.gnu.org/copyleft/gpl.html.
 *
 * Other Usage
 * Alternatively, this file may be used in accordance with the terms and
 * conditions contained in a signed written agreement between you and
 * Donna Whisnant.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "neptune_rec.h"

/* Local Defines */
#define VERSION 100

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif

/* ========================================================================== */

int ReadRecord(void *pSource, DATA_REC *pRecord)
{
    pRecord->dwSize = 0;
    pRecord->dwReturned = 0;
    if (!ReadString(pSource, pRecord->data, MAX_RECORD_SIZE)) return FALSE;
    pRecord->dwSize = strlen(pRecord->data);

    return TRUE;
}

unsigned char ConvHexByte(const unsigned char *pData)
{
    char hexbyte[3];
    hexbyte[0] = pData[0];
    hexbyte[1] = pData[1];
    hexbyte[2] = 0;
    return (unsigned char)strtoul(hexbyte, NULL, 16);
}

int ReadChar(DATA_REC *pRecord)
{
    if (pRecord->dwReturned >= pRecord->dwSize) return -1;
    return pRecord->data[pRecord->dwReturned++];
}

int UnreadChar(DATA_REC *pRecord)
{
    if (pRecord->dwReturned > 0) {
        pRecord->dwReturned--;
        return TRUE;
    }
    return FALSE;
}

int ReadHexChar(DATA_REC *pRecord, unsigned char *pHexByte)
{
    int c;

    c = ReadChar(pRecord);
    if (c == -1) return -1;
    pHexByte[0] = (unsigned char)c;

    c = ReadChar(pRecord);
    if (c == -1) return -1;
    pHexByte[1] = (unsigned char)c;

    c = ReadChar(pRecord);
    if (c == -1) {
        pHexByte[2] = 0;
    } else {
        /* Each Hex Pair has a space after it we need to read here */
        pHexByte[2] = (unsigned char)c;
        pHexByte[3] = 0;
    }

    return ConvHexByte(pHexByte);
}

/* ========================================================================== */

int GetNextRecord(void *pSource, unsigned char *pBuff)
{
    /* Note: This function returns with either the
                record type code (0 - 255) or with:
                -1 = No more data available from device
                -2 = Bad Record (too short)
                -3 = Bad Record (Invalid Checksum)
    */
    DATA_REC myRecord;
    int type;
    int reclen;
    int checksum;
    unsigned char hexbyte[4];
    int i;
    int byteval;
    int iscomment;

    while (1) {
        pBuff[0] = 0;
        if (!ReadRecord(pSource, &myRecord)) return -1;

        /* Search and remove left whitespace and remove comment lines */
        iscomment = FALSE;
        while ((i = ReadChar(&myRecord)) != -1) {
            if (!isspace(i)) {
                if (i == '!') {
                    iscomment = TRUE;
                } else {
                    UnreadChar(&myRecord);
                }
                break;
            }
        }
        if (iscomment) continue;    /* If this was just a comment line, get next record */

        /* Get Record Length */
        reclen = ReadHexChar(&myRecord, hexbyte);
        if (reclen < 0) {
            type = -2;
            break;
        }
        strcat(pBuff, hexbyte);

        /* Get Record Type */
        type = ReadHexChar(&myRecord, hexbyte);
        if (type < 0) {
            type = -2;
            break;
        }
        strcat(pBuff, hexbyte);
        checksum = type;

        /* Read Data Bytes */
        for (i=0; ((i<(reclen-1)) && (i<MAX_RECORD_SIZE)); i++) {
            byteval = ReadHexChar(&myRecord, hexbyte);
            if (byteval < 0) {
                type = -2;
                break;
            }
            strcat(pBuff, hexbyte);
            checksum += byteval;
        }
        if (type < 0) break;

        /* Read and check the checksum */
        byteval = ReadHexChar(&myRecord, hexbyte);
        if (byteval == -1) {
            type = -2;
            break;
        }
        strcat(pBuff, hexbyte);
        if (byteval != (checksum & 0xFF)) {
            type = -3;
            break;
        }

        break;      /* Always break out of a valid record read */
    }

    switch (type) {
        case -2:
            fprintf(stderr, "\n%s  <<< Invalid Record (Too Short)\n", pBuff);
            break;
        case -3:
            fprintf(stderr, "\n%s  <<< Bad Record Checksum\n", pBuff);
            break;
    }

    return type;
}

