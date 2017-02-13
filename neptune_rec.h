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

#ifndef _NEPTUNE_REC_H_
#define _NEPTUNE_REC_H_

#define MAX_RECORD_SIZE 2053

typedef struct data_rec
{
    unsigned char data[MAX_RECORD_SIZE];    /* One line of record data */
    long        dwSize;                     /* Length of data (bytes read -- strlen) */
    long        dwReturned;                 /* Bytes returned already */
} DATA_REC;

/* ReadRecord - Reads a string from a data source */
extern int ReadRecord(void *pSource, DATA_REC *pRecord);

/* ConvHexByte - Converts ASCII-HEX byte into a character value */
extern unsigned char ConvHexByte(const unsigned char *pData);

/* ReadChar - Returns next character in record buffer and advances pointer */
extern int ReadChar(DATA_REC *pRecord);

/* UnreadChar - Backs the character pointer in the record buffer back by one character */
extern int UnreadChar(DATA_REC *pRecord);

/* ReadHexChar - Reads 3 characters from record and places them in the pHexByte buffer.
                    Designed for reading the 2-ASCII-HEX bytes and 1-Space */
extern int ReadHexChar(DATA_REC *pRecord, unsigned char *pHexByte);

/* GetNextRecord - Reads and verifies next data record
        Note: This function returns with either the
                    record type code (0 - 255) or with:
                    -1 = No more data available from device
                    -2 = Bad Record (too short)
                    -3 = Bad Record (Invalid Checksum)
*/
extern int GetNextRecord(void *pSource, unsigned char *pBuff);

/* ReadString - This function must be implemented by external app to read a string
        from some arbitrary data source, be it a direct socket, device, file, etc. */
extern int ReadString(void *pSource, unsigned char *pBuff, long nBufSize);

#endif  /* _NEPTUNE_REC_H_ */

