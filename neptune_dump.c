/*
 * Neptune_Dump
 *
 * This app reads a neptune data file and dumps a jump summary
 * and/or jump detail for the specified jump, if that jump number
 * is contained in the file.
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

#include <math.h>

#include "neptune_rec.h"

/* Prototype for lround, round, and trunc -- for some reason they aren't included in math.h ?? */
long int lround(double x);
double round(double x);
double trunc(double x);

/* Local Defines */
#define VERSION 100

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif

#define DT_UNKNOWN      0
#define DT_SUMMARY      1
#define DT_DETAIL       2
#define DT_PROFILE_TAB  3
#define DT_PROFILE_CSV  4
#define DT_GNUPLOT      5

#define PT_AIRCRAFT     0
#define PT_FREEFALL     1
#define PT_CANOPY       2

#define MAX_JUMP_RECORDS    200
#define MAX_JUMP_PROFILES   10
#define MAX_PROFILE_DATA    2000

/* Type Definitions */
typedef struct jump_rec
{
    unsigned long nJumpNumber;
    int nJumpType;
    double nExitAltitude;
    double nDeployAltitude;
    double nGroundAltitude;
    double nFreefallStartTime;
    double nCanopyStartTime;
} JUMP_REC;

typedef struct jump_datapt
{
    int nPointType;
    double nTime;
    double nAltitude;
    double nTASpeed;
    double nSASpeed;
} JUMP_DATAPT;

typedef struct jump_prof
{
    unsigned long nJumpNumber;
    int nAircraftPoints;
    int nFreefallPoints;
    int nCanopyPoints;
    int nNumDataPoints;
    JUMP_DATAPT DataPoints[MAX_PROFILE_DATA];
} JUMP_PROF;

/* Constants */
#define NUM_JUMP_TYPES 16
const char *strJumpTypes[NUM_JUMP_TYPES+1] = {
                    "<Unknown>",
                    "Group 1", "Group 2", "Group 3", "Group 4",
                    "4-way", "8-way", "10-way", "16-way",
                    "Freefly", "Big Way", "Tandem", "AFF",
                    "Birdman", "Camera", "Student", "Group 5"
                };

const char *strPointTypes[3] = {
                    "Aircraft", "Freefall", "Canopy"
                };

/* Globals */
unsigned char databuff[MAX_RECORD_SIZE];
JUMP_REC JumpRecords[MAX_JUMP_RECORDS];
int nNumJumpRecords = 0;
JUMP_PROF JumpProfiles[MAX_JUMP_PROFILES];
int nNumJumpProfiles = 0;

/* Prototypes */
int ReadString(void *pSource, unsigned char *pBuff, long nBufSize);
void PrintSummary(FILE *pInFile, int nDumpType, unsigned long nJumpNumber, const char *pSubTypes, const char *pLocation);
void PrintDetail(FILE *pInFile, int nDumpType, unsigned long nJumpNumber, const char *pSubTypes, const char *pLocation);
void PrintProfile(FILE *pInFile, int nDumpType, unsigned long nJumpNumber, const char *pSubTypes, const char *pLocation);
void PrintGnuPlot(FILE *pInFile, int nDumpType, unsigned long nJumpNumber, const char *pSubTypes, const char *pLocation);

/* ========================================================================== */

int ReadString(void *pSource, unsigned char *pBuff, long nBufSize)
{
    FILE *pFile = (FILE*)pSource;

    if (!fgets(pBuff, nBufSize, pFile)) return FALSE;
    return TRUE;
}

/* ========================================================================== */

void PrintSummary(FILE *pInFile, int nDumpType, unsigned long nJumpNumber, const char *pSubTypes, const char *pLocation)
{
    int i;
    int type;
    int done;
    int nNepVersionHi;
    int nNepVersionLo;
    int nNepVersionRev;
    char strNepSerialNo[10];

    done = FALSE;
    while ((!done) &&
            ((type = GetNextRecord(pInFile, databuff)) != -1)) {

        switch (type) {
            case 0:     /* Version Info */
                nNepVersionHi = (ConvHexByte(&databuff[9]) >> 4) & 0x0F;
                nNepVersionLo = ConvHexByte(&databuff[9]) & 0x0F;
                nNepVersionRev = ConvHexByte(&databuff[12]);
                if ((nNepVersionHi == 0) && (nNepVersionLo == 0) && (nNepVersionRev < 14))
                    nNepVersionHi = 2;
                printf("Neptune Software v%u.%u.%u\n",
                            nNepVersionHi, nNepVersionLo, nNepVersionRev);
                for (i=0; i<9; i++) {
                    strNepSerialNo[i] = ConvHexByte(&databuff[i*3+15]);
                    if (strNepSerialNo[i] == 0x20) strNepSerialNo[i] = 0x00;    /* String is right padded with spaces, so whitespace trim */
                }
                strNepSerialNo[9] = 0;
                printf("Neptune Serial No: %s\n", strNepSerialNo);
                printf("\n");
                break;
            case 1:     /* Jump Summary */
                printf("Number Jump Records   = %lu\n",
                            ConvHexByte(&databuff[6]) + ConvHexByte(&databuff[9])*256ul);
                printf("Number Jump Profiles  = %u\n",
                            ConvHexByte(&databuff[12]));
                printf("Total Jumps Made      = %lu\n",
                            ConvHexByte(&databuff[15]) + ConvHexByte(&databuff[18])*256ul);
                printf("Total FreeFall Time   = %lu sec\n",
                            ConvHexByte(&databuff[21]) + ConvHexByte(&databuff[24])*256ul +
                            ConvHexByte(&databuff[27])*65536ul + ConvHexByte(&databuff[30])*16777216ul);
                printf("Last Jump Number      = %lu\n",
                            ConvHexByte(&databuff[33]) + ConvHexByte(&databuff[36])*256ul + 1ul);
                printf("\n");
                break;
            case 2:     /* Jump Record */
                break;
            case 3:     /* End of all data */
                done = TRUE;
                break;
            case 4:     /* Jump Profile Data Stream Type */
                break;
            case 5:     /* Profile Start */
                break;
            case 6:     /* Profile Datapoint */
                break;
            case 7:     /* End of Profile */
                break;
            case -2:    /* Bad Record (Too Short) */
                break;
            case -3:    /* Bad Record (Invalid Checksum) */
                break;
            default:    /* Unknown Record */
                break;
        }
    }
}

void PrintDetail(FILE *pInFile, int nDumpType, unsigned long nJumpNumber, const char *pSubTypes, const char *pLocation)
{
    int i;
    long nAltitude;
    double nAvgSpeed;
    double nSpeed;
    int type;
    int done;
    long datatype;
    unsigned long ulTemp;
    int bFirst;
    int bFindingPoints;
    int nAircraftPoints;
    int nFreefallPoints;
    int nCanopyPoints;

    done = FALSE;
    datatype = PT_AIRCRAFT;
    bFirst = FALSE;
    bFindingPoints = FALSE;
    while ((!done) &&
            ((type = GetNextRecord(pInFile, databuff)) != -1)) {

        switch (type) {
            case 2:     /* Starting new Jump Record */
            case 3:     /* End of all data */
            case 5:     /* Starting new Jump Profile */
            case 7:     /* End of Profile */
                if (bFindingPoints) {
                    printf("Num Aircraft Data Pts = %u\n", nAircraftPoints);
                    printf("Num Freefall Data Pts = %u\n", nFreefallPoints);
                    printf("Num Canopy Data Pts   = %u\n", nCanopyPoints);
                    bFindingPoints = FALSE;
                }
                break;
        }

        switch (type) {
            case 0:     /* Version Info */
                break;
            case 1:     /* Jump Summary */
                break;
            case 2:     /* Jump Record */
                ulTemp = ConvHexByte(&databuff[6]) + ConvHexByte(&databuff[9])*256ul + 1ul;
                if ((nJumpNumber != 0) && (nJumpNumber != ulTemp)) continue;
                if (bFirst) printf("\n");
                bFirst = TRUE;
                printf("Jump Number           : %lu\n", ulTemp);
                printf("Jump Date/Time        = %02u/%02u/%02u  %02u:%02u\n",
                            ConvHexByte(&databuff[21]), ConvHexByte(&databuff[18]), ConvHexByte(&databuff[24]),
                            ConvHexByte(&databuff[15]), ConvHexByte(&databuff[12]));
                printf("Jump Type             = %s\n",
                            ((ConvHexByte(&databuff[27]) < NUM_JUMP_TYPES) ? strJumpTypes[ConvHexByte(&databuff[27])+1] : strJumpTypes[0]));
                printf("Data Version          = %u.%u.%u\n",
                            ((ConvHexByte(&databuff[57])>>4) & 0x0F) + 1,
                            (ConvHexByte(&databuff[57]) & 0x0F),
                            (ConvHexByte(&databuff[60])));
                printf("Data SW Type          = %u\n", ConvHexByte(&databuff[63]));
                nAvgSpeed = 0.0;
                i = 0;
                nSpeed = (round(ConvHexByte(&databuff[30])*22.3694))/10.0;
                if (nSpeed) {
                    nAvgSpeed += nSpeed;
                    i++;
                }
                printf("Max FF Speed (TAS)    = %.1f mph\n", nSpeed);
                nSpeed = (round(ConvHexByte(&databuff[33])*22.3694)/10.0);
                if (nSpeed) {
                    nAvgSpeed += nSpeed;
                    i++;
                }
                printf("12K FF Speed (TAS)    = %.1f mph\n", nSpeed);
                nSpeed = (round(ConvHexByte(&databuff[36])*22.3694)/10.0);
                if (nSpeed) {
                    nAvgSpeed += nSpeed;
                    i++;
                }
                printf(" 9K FF Speed (TAS)    = %.1f mph\n", nSpeed);
                nSpeed = (round(ConvHexByte(&databuff[39])*22.3694)/10.0);
                if (nSpeed) {
                    nAvgSpeed += nSpeed;
                    i++;
                }
                printf(" 6K FF Speed (TAS)    = %.1f mph\n", nSpeed);
                printf(" 3K FF Speed (TAS)    = %.1f mph\n",
                            (round(ConvHexByte(&databuff[42])*22.3694)/10.0));
                if (i) nAvgSpeed = round((nAvgSpeed*10.0)/i)/10.0;
                printf("Avg FF Speed (TAS)    = %.1f mph\n", nAvgSpeed);
                printf("Exit Altitude (AGL)   = %lu ft\n",
                            (unsigned long)lround((ConvHexByte(&databuff[45]) + ConvHexByte(&databuff[48])*256ul)*3.28084));
                printf("Deploy Altitude (AGL) = %lu ft\n",
                            (unsigned long)lround((ConvHexByte(&databuff[51]) + ConvHexByte(&databuff[54])*256ul)*3.28084));
                printf("Freefall Time         = %lu sec\n",
                            ConvHexByte(&databuff[66]) + ConvHexByte(&databuff[69])*256ul);
                break;
            case 3:     /* End of all data */
                done = TRUE;
                break;
            case 4:     /* Jump Profile Data Stream Type */
                switch (ConvHexByte(&databuff[6])) {
                    case 5:
                        datatype = PT_FREEFALL;
                        break;
                    case 6:
                    case 7:
                        datatype = PT_CANOPY;
                        break;
                }
                break;
            case 5:     /* Profile Start */
                ulTemp = ConvHexByte(&databuff[6]) + ConvHexByte(&databuff[9])*256ul + 1ul;
                if ((nJumpNumber != 0) && (nJumpNumber != ulTemp)) continue;
                nAltitude = (ConvHexByte(&databuff[12]) + ConvHexByte(&databuff[15])*256ul);
                if (nAltitude > 32767l) nAltitude = nAltitude - 65534l;     /* Why is this 65534 in paralog and not 65536 ?? */
                printf("Ground Altitude (MSL) = %ld ft\n", lround(nAltitude*3.28084));
                //Type5 Exit Altitude -- Redundant
                //printf("Exit Altitude (AGL)   = %lu ft\n",
                //            (unsigned long)lround((ConvHexByte(&databuff[18]) + ConvHexByte(&databuff[21])*256ul)*3.28084));
                printf("Freefall Start Time   = %.2f sec\n",
                            (ConvHexByte(&databuff[24]) + ConvHexByte(&databuff[27])*256ul)*0.25);
                printf("Canopy Start Time     = %.2f sec\n",
                            (ConvHexByte(&databuff[30]) + ConvHexByte(&databuff[33])*256ul)*0.25);

                datatype = PT_AIRCRAFT;
                nAircraftPoints = 0;
                nFreefallPoints = 0;
                nCanopyPoints = 0;
                bFindingPoints = TRUE;
                break;
            case 6:     /* Profile Datapoint */
                if (bFindingPoints) {
                    switch (datatype) {
                        case PT_AIRCRAFT:
                            nAircraftPoints++;
                            break;
                        case PT_FREEFALL:
                            nFreefallPoints++;
                            break;
                        case PT_CANOPY:
                            nCanopyPoints++;
                            break;
                    }
                }
                break;
            case 7:     /* End of Profile */
                break;
            case -2:    /* Bad Record (Too Short) */
                break;
            case -3:    /* Bad Record (Invalid Checksum) */
                break;
            default:    /* Unknown Record */
                break;
        }
    }

    printf("\n");
}

void ReadJumpData(FILE *pInFile, unsigned long nJumpNumber)
{
    int i,j,k;
    double speedInterval;
    double nmAltitude;
    long nAltitude;
    int type;
    int done;
    long datatype;
    unsigned long nCurrentJump;
    int bFindingPoints;
    int ndxJumpRecord;
    int ndxJumpProfile;

    done = FALSE;
    datatype = PT_AIRCRAFT;
    bFindingPoints = FALSE;
    nNumJumpRecords = 0;
    nNumJumpProfiles = 0;

    while ((!done) &&
            ((type = GetNextRecord(pInFile, databuff)) != -1)) {

        switch (type) {
            case 2:     /* Starting new Jump Record */
            case 3:     /* End of all data */
            case 5:     /* Starting new Jump Profile */
            case 7:     /* End of Profile */
                bFindingPoints = FALSE;
                break;
        }

        switch (type) {
            case 0:     /* Version Info */
                break;
            case 1:     /* Jump Summary */
                break;
            case 2:     /* Jump Record */
                nCurrentJump = ConvHexByte(&databuff[6]) + ConvHexByte(&databuff[9])*256ul + 1ul;
                if ((nJumpNumber != 0) && (nJumpNumber != nCurrentJump)) continue;

                ndxJumpRecord = -1;
                for (i=0; i<nNumJumpRecords; i++) {
                    if (JumpRecords[i].nJumpNumber == nCurrentJump) {
                        ndxJumpRecord = i;
                        break;
                    }
                }
                if (ndxJumpRecord == -1) {
                    if (nNumJumpRecords >= MAX_JUMP_RECORDS) break;
                    ndxJumpRecord = nNumJumpRecords++;
                    JumpRecords[ndxJumpRecord].nJumpNumber = nCurrentJump;
                    JumpRecords[ndxJumpRecord].nJumpType = 0;
                    JumpRecords[ndxJumpRecord].nExitAltitude = 0.0;
                    JumpRecords[ndxJumpRecord].nDeployAltitude = 0.0;
                    JumpRecords[ndxJumpRecord].nGroundAltitude = 0.0;
                    JumpRecords[ndxJumpRecord].nFreefallStartTime = 0.0;
                    JumpRecords[ndxJumpRecord].nCanopyStartTime = 0.0;
                }

                if (ConvHexByte(&databuff[27]) < NUM_JUMP_TYPES) {
                    JumpRecords[ndxJumpRecord].nJumpType = ConvHexByte(&databuff[27])+1;
                } else {
                    JumpRecords[ndxJumpRecord].nJumpType = 0;
                }
                JumpRecords[ndxJumpRecord].nExitAltitude = (ConvHexByte(&databuff[45]) + ConvHexByte(&databuff[48])*256ul)*3.28084;
                JumpRecords[ndxJumpRecord].nDeployAltitude = (ConvHexByte(&databuff[51]) + ConvHexByte(&databuff[54])*256ul)*3.28084;

                break;
            case 3:     /* End of all data */
                done = TRUE;
                break;
            case 4:     /* Jump Profile Data Stream Type */
                switch (ConvHexByte(&databuff[6])) {
                    case 5:
                        datatype = PT_FREEFALL;
                        break;
                    case 6:
                    case 7:
                        datatype = PT_CANOPY;
                        break;
                }
                break;
            case 5:     /* Profile Start */
                nCurrentJump = ConvHexByte(&databuff[6]) + ConvHexByte(&databuff[9])*256ul + 1ul;
                if ((nJumpNumber != 0) && (nJumpNumber != nCurrentJump)) continue;

                ndxJumpRecord = -1;
                for (i=0; i<nNumJumpRecords; i++) {
                    if (JumpRecords[i].nJumpNumber == nCurrentJump) {
                        ndxJumpRecord = i;
                        break;
                    }
                }
                if (ndxJumpRecord == -1) {
                    if (nNumJumpRecords >= MAX_JUMP_RECORDS) break;
                    ndxJumpRecord = nNumJumpRecords++;
                    JumpRecords[ndxJumpRecord].nJumpNumber = nCurrentJump;
                    JumpRecords[ndxJumpRecord].nJumpType = 0;
                    JumpRecords[ndxJumpRecord].nExitAltitude = 0.0;
                    JumpRecords[ndxJumpRecord].nDeployAltitude = 0.0;
                    JumpRecords[ndxJumpRecord].nGroundAltitude = 0.0;
                    JumpRecords[ndxJumpRecord].nFreefallStartTime = 0.0;
                    JumpRecords[ndxJumpRecord].nCanopyStartTime = 0.0;
                }

                nAltitude = (ConvHexByte(&databuff[12]) + ConvHexByte(&databuff[15])*256ul);
                if (nAltitude > 32767l) nAltitude = nAltitude - 65534l;     /* Why is this 65534 in paralog and not 65536 ?? */

                JumpRecords[ndxJumpRecord].nGroundAltitude = nAltitude*3.28084;
                JumpRecords[ndxJumpRecord].nFreefallStartTime = (ConvHexByte(&databuff[24]) + ConvHexByte(&databuff[27])*256ul)*0.25;
                JumpRecords[ndxJumpRecord].nCanopyStartTime = (ConvHexByte(&databuff[30]) + ConvHexByte(&databuff[33])*256ul)*0.25;

                ndxJumpProfile = -1;
                for (i=0; i<nNumJumpProfiles; i++) {
                    if (JumpProfiles[i].nJumpNumber == nCurrentJump) {
                        ndxJumpProfile = i;
                        break;
                    }
                }
                if (ndxJumpProfile == -1) {
                    if (nNumJumpProfiles >= MAX_JUMP_PROFILES) break;
                    ndxJumpProfile = nNumJumpProfiles++;
                    JumpProfiles[ndxJumpProfile].nJumpNumber = nCurrentJump;
                    JumpProfiles[ndxJumpProfile].nAircraftPoints = 0;
                    JumpProfiles[ndxJumpProfile].nFreefallPoints = 0;
                    JumpProfiles[ndxJumpProfile].nCanopyPoints = 0;
                    JumpProfiles[ndxJumpProfile].nNumDataPoints = 0;
                }

                datatype = PT_AIRCRAFT;
                bFindingPoints = TRUE;
                break;
            case 6:     /* Profile Datapoint */
                if (bFindingPoints) {
                    if (JumpProfiles[ndxJumpProfile].nNumDataPoints >= MAX_PROFILE_DATA) {
                        bFindingPoints = FALSE;
                        break;
                    }
                    switch (datatype) {
                        case PT_AIRCRAFT:
                            JumpProfiles[ndxJumpProfile].nAircraftPoints++;
                            break;
                        case PT_FREEFALL:
                            JumpProfiles[ndxJumpProfile].nFreefallPoints++;
                            break;
                        case PT_CANOPY:
                            JumpProfiles[ndxJumpProfile].nCanopyPoints++;
                            break;
                    }
                    JumpProfiles[ndxJumpProfile].DataPoints[JumpProfiles[ndxJumpProfile].nNumDataPoints].nPointType = datatype;
                    JumpProfiles[ndxJumpProfile].DataPoints[JumpProfiles[ndxJumpProfile].nNumDataPoints].nTime =
                                (ConvHexByte(&databuff[12]) + ConvHexByte(&databuff[15])*256ul)*0.25;
                    JumpProfiles[ndxJumpProfile].DataPoints[JumpProfiles[ndxJumpProfile].nNumDataPoints].nAltitude =
                                (ConvHexByte(&databuff[6]) + ConvHexByte(&databuff[9])*256ul)*3.28084;
                    JumpProfiles[ndxJumpProfile].DataPoints[JumpProfiles[ndxJumpProfile].nNumDataPoints].nTASpeed = 0.0;
                    JumpProfiles[ndxJumpProfile].DataPoints[JumpProfiles[ndxJumpProfile].nNumDataPoints].nSASpeed = 0.0;

                    JumpProfiles[ndxJumpProfile].nNumDataPoints++;
                }
                break;
            case 7:     /* End of Profile */
                break;
            case -2:    /* Bad Record (Too Short) */
                break;
            case -3:    /* Bad Record (Invalid Checksum) */
                break;
            default:    /* Unknown Record */
                break;
        }
    }

    speedInterval = 6.0;

    for (ndxJumpProfile=0; ndxJumpProfile < nNumJumpProfiles; ndxJumpProfile++) {
        for (k=0; ((k < JumpProfiles[ndxJumpProfile].nNumDataPoints) &&
                    ((JumpProfiles[ndxJumpProfile].DataPoints[k].nTime - JumpProfiles[ndxJumpProfile].DataPoints[0].nTime) <
                        (speedInterval / 2.0))); k++) {
            JumpProfiles[ndxJumpProfile].DataPoints[k].nTASpeed = 0.0;
            JumpProfiles[ndxJumpProfile].DataPoints[k].nSASpeed = 0.0;
        }

        j = 0;
        i = 0;

        for(; ((k < JumpProfiles[ndxJumpProfile].nNumDataPoints) &&
                (j < JumpProfiles[ndxJumpProfile].nNumDataPoints)); k++) {
            while ((i < JumpProfiles[ndxJumpProfile].nNumDataPoints) &&
                    ((JumpProfiles[ndxJumpProfile].DataPoints[k].nTime - JumpProfiles[ndxJumpProfile].DataPoints[i].nTime) >
                        (speedInterval / 2.0)))
                i++;
            for (; ((j < JumpProfiles[ndxJumpProfile].nNumDataPoints) &&
                    ((JumpProfiles[ndxJumpProfile].DataPoints[j].nTime - JumpProfiles[ndxJumpProfile].DataPoints[i].nTime) <
                        speedInterval)); j++);
            if ((i < JumpProfiles[ndxJumpProfile].nNumDataPoints) &&
                (j < JumpProfiles[ndxJumpProfile].nNumDataPoints)) {
                JumpProfiles[ndxJumpProfile].DataPoints[k].nTASpeed =
                        ((JumpProfiles[ndxJumpProfile].DataPoints[i].nAltitude/3.28084) -
                            (JumpProfiles[ndxJumpProfile].DataPoints[j].nAltitude/3.28084)) /
                        (JumpProfiles[ndxJumpProfile].DataPoints[j].nTime - JumpProfiles[ndxJumpProfile].DataPoints[i].nTime);
                nmAltitude = JumpProfiles[ndxJumpProfile].DataPoints[k].nAltitude/3.28084;
                JumpProfiles[ndxJumpProfile].DataPoints[k].nSASpeed =
                        JumpProfiles[ndxJumpProfile].DataPoints[k].nTASpeed /
                        (1.0 + 0.00004 * nmAltitude + 0.000000001 * nmAltitude * nmAltitude);
                JumpProfiles[ndxJumpProfile].DataPoints[k].nTASpeed *= 2.236936;
                JumpProfiles[ndxJumpProfile].DataPoints[k].nSASpeed *= 2.236936;
            }
        }

        for (; (k < JumpProfiles[ndxJumpProfile].nNumDataPoints); k++) {
            JumpProfiles[ndxJumpProfile].DataPoints[k].nTASpeed = 0.0;
            JumpProfiles[ndxJumpProfile].DataPoints[k].nSASpeed = 0.0;
        }
    }
}

void PrintProfile(FILE *pInFile, int nDumpType, unsigned long nJumpNumber, const char *pSubTypes, const char *pLocation)
{
    int i,j;

    ReadJumpData(pInFile, nJumpNumber);

    if ((!pSubTypes) || (strpbrk(pSubTypes, "h") == NULL)) {
        switch (nDumpType) {
            case DT_PROFILE_TAB:
                if ((!pSubTypes) || (strpbrk(pSubTypes, "s") == NULL)) {
                    printf("Jump\tPoint\tType\tTime\tAltitude\tTASpeed\tSASpeed\n");
                } else {
                    printf("Jump    Point   Type            Time            Altitude        TASpeed         SASpeed\n");
                }
                break;
            case DT_PROFILE_CSV:
                printf("Jump,Point,Type,Time,Altitude,TASpeed,SASpeed\n");
                break;
        }
    }

    for (i=0; i<nNumJumpProfiles; i++) {
        for (j=0; j<JumpProfiles[i].nNumDataPoints; j++) {
            switch (nDumpType) {
                case DT_PROFILE_TAB:
                    if ((!pSubTypes) || (strpbrk(pSubTypes, "s") == NULL)) {
                        printf("%lu\t%d\t%s\t%f\t%f\t%f\t%f\n",
                                JumpProfiles[i].nJumpNumber,
                                j+1,
                                strPointTypes[JumpProfiles[i].DataPoints[j].nPointType],
                                JumpProfiles[i].DataPoints[j].nTime,
                                JumpProfiles[i].DataPoints[j].nAltitude,
                                JumpProfiles[i].DataPoints[j].nTASpeed,
                                JumpProfiles[i].DataPoints[j].nSASpeed);
                    } else {
                        printf("%-7lu %-7d %-15s %-15f %-15f %-15f %-15f\n",
                                JumpProfiles[i].nJumpNumber,
                                j+1,
                                strPointTypes[JumpProfiles[i].DataPoints[j].nPointType],
                                JumpProfiles[i].DataPoints[j].nTime,
                                JumpProfiles[i].DataPoints[j].nAltitude,
                                JumpProfiles[i].DataPoints[j].nTASpeed,
                                JumpProfiles[i].DataPoints[j].nSASpeed);
                    }
                    break;
                case DT_PROFILE_CSV:
                    printf("%lu,%d,%s,%f,%f,%f,%f\n",
                            JumpProfiles[i].nJumpNumber,
                            j+1,
                            strPointTypes[JumpProfiles[i].DataPoints[j].nPointType],
                            JumpProfiles[i].DataPoints[j].nTime,
                            JumpProfiles[i].DataPoints[j].nAltitude,
                            JumpProfiles[i].DataPoints[j].nTASpeed,
                            JumpProfiles[i].DataPoints[j].nSASpeed);
                    break;
            }
        }
    }
}

void PrintGnuPlot(FILE *pInFile, int nDumpType, unsigned long nJumpNumber, const char *pSubTypes, const char *pLocation)
{
    int i,j;
    double nMaxAltitude;
    double nExitAltitude;
    double nDeployAltitude;
    double nFreefallStartTime;
    double nCanopyStartTime;
    int bSingleJump;
    int nFlags;
    int bAltPlot;
    int bTASPlot;
    int bSASPlot;
    int bFirst;

    ReadJumpData(pInFile, nJumpNumber);
    if (nNumJumpProfiles == 0) return;  /* Exit if nothing to do */

    bSingleJump = FALSE;
    if ((nNumJumpRecords == 1) &&
        (nNumJumpProfiles == 1)) bSingleJump = TRUE;

    bAltPlot = FALSE;
    bTASPlot = FALSE;
    bSASPlot = FALSE;
    if (pSubTypes) {
        if (strpbrk(pSubTypes, "a")) bAltPlot = TRUE;
        if (strpbrk(pSubTypes, "t")) bTASPlot = TRUE;
        if (strpbrk(pSubTypes, "s")) bSASPlot = TRUE;
    } else {
        bAltPlot = TRUE;
        bTASPlot = TRUE;
        bSASPlot = TRUE;
    }

    nMaxAltitude = 0.0;
    nExitAltitude = 0.0;
    nDeployAltitude = 0.0;
    nFreefallStartTime = 0.0;
    nCanopyStartTime = 0.0;
    for (i=0; i<nNumJumpProfiles; i++) {
        if (bSingleJump) {
            nFreefallStartTime = JumpRecords[0].nFreefallStartTime;
            nCanopyStartTime = JumpRecords[0].nCanopyStartTime;
            nFlags = 0;
            for (j=0; j<JumpProfiles[0].nNumDataPoints; j++) {
                if ((nFlags == 0) &&
                    (JumpProfiles[0].DataPoints[j].nTime >= nFreefallStartTime)) {
                    nExitAltitude = JumpProfiles[0].DataPoints[j].nAltitude;
                    nFlags = 1;
                }
                if ((nFlags == 1) &&
                    (JumpProfiles[0].DataPoints[j].nTime >= nCanopyStartTime)) {
                    nDeployAltitude = JumpProfiles[0].DataPoints[j].nAltitude;
                    nFlags = 2;
                }
            }
        }

        for (j=0; j<JumpProfiles[i].nNumDataPoints; j++) {
            if (JumpProfiles[i].DataPoints[j].nAltitude > nMaxAltitude)
                nMaxAltitude = JumpProfiles[i].DataPoints[j].nAltitude;
        }
    }
    nMaxAltitude = (trunc(nMaxAltitude/1000.0) + 1.0) * 1000.0;

    if ((pSubTypes) && (strpbrk(pSubTypes, "r") == NULL))
        printf("reset\n");

    if (bSingleJump) {
        if (pLocation) {
            printf("set title \"Jump %lu - %s (%s)\"\n",
                        JumpRecords[0].nJumpNumber,
                        pLocation,
                        strJumpTypes[JumpRecords[0].nJumpType]);
        } else {
            printf("set title \"Jump %lu (%s)\"\n",
                        JumpRecords[0].nJumpNumber,
                        strJumpTypes[JumpRecords[0].nJumpType]);
        }
    } else {
        if (pLocation) {
            printf("set title \"%s\"\n", pLocation);
        }
    }

    printf("set xtics 0.0,25.0\n");
    printf("set xlabel \"Time (sec)\"\n");

    if (bAltPlot) {
        if ((bTASPlot) || (bSASPlot)) {
            printf("set ytics nomirror 0.0,1000.0\n");
            printf("set ylabel \"Altitude (ft)\"\n");
            printf("set y2tics autofreq\n");
            printf("set y2label \"Speed (mph)\"\n");
        } else {
            printf("set ytics 0.0,1000.0\n");
            printf("set ylabel \"Altitude (ft)\"\n");
        }
    } else {
        printf("set ytics autofreq\n");
        printf("set ylabel \"Speed (mph)\"\n");
    }

    if ((bSingleJump) && (bAltPlot)) {
        printf("set style line 1 lt 8 pt 3\n");
        printf("set label 1 \"Exit\" at %f,%f\n",
                    nFreefallStartTime + 5.0, nExitAltitude);
        printf("set label 2 \"Deploy\" at %f,%f\n",
                    nCanopyStartTime + 5.0, nDeployAltitude);
    }

    /* Print Plot Commands */
    bFirst = TRUE;
    for (i=0; i<nNumJumpProfiles; i++) {
        if (bAltPlot) {
            printf("%s '-' title \"Altitude\" with lines",
                    (bFirst ? "plot" : ", "));
            bFirst = FALSE;
        }

        if (bTASPlot) {
            printf("%s '-' axes x1y2 title \"TASpeed\" with lines%s",
                    (bFirst ? "plot" : ", "),
                    ((bSingleJump && !bSASPlot) ? " 3" : ""));
            bFirst = FALSE;
        }

        if (bSASPlot) {
            printf("%s '-' axes x1y2 title \"SASpeed\" with lines%s",
                    (bFirst ? "plot" : ", "),
                    ((bSingleJump && !bTASPlot) ? " 3" : ""));
            bFirst = FALSE;
        }

        if ((bSingleJump) && (bAltPlot)) {
            printf("%s '-' notitle with points ls 1",
                    (bFirst ? "plot" : ", "));
            bFirst = FALSE;
        }
    }
    printf("\n");

    /* Print Plot Data */
    for (i=0; i<nNumJumpProfiles; i++) {
        if (bAltPlot) {
            for (j=0; j<JumpProfiles[i].nNumDataPoints; j++) {
                printf("%f %f\n", JumpProfiles[i].DataPoints[j].nTime,
                                JumpProfiles[i].DataPoints[j].nAltitude);
            }
            printf("e\n");
        }

        if (bTASPlot) {
            for (j=0; j<JumpProfiles[i].nNumDataPoints; j++) {
                printf("%f %f\n", JumpProfiles[i].DataPoints[j].nTime,
                                JumpProfiles[i].DataPoints[j].nTASpeed);
            }
            printf("e\n");
        }

        if (bSASPlot) {
            for (j=0; j<JumpProfiles[i].nNumDataPoints; j++) {
                printf("%f %f\n", JumpProfiles[i].DataPoints[j].nTime,
                                JumpProfiles[i].DataPoints[j].nSASpeed);
            }
            printf("e\n");
        }

        if ((bSingleJump) && (bAltPlot)) {
            printf("%f %f\n", nFreefallStartTime, nExitAltitude);
            printf("%f %f\n", nCanopyStartTime, nDeployAltitude);
            printf("e\n");
        }
    }

    if ((pSubTypes) && (strpbrk(pSubTypes, "p")))
        printf("pause -1 \"Hit return to continue\"\n");
}

/* ========================================================================== */

int main(int argc, char *argv[])
{
    FILE *pInFile;
    char *pInFilename;
    char *pLocation;
    unsigned long nJumpNumber;
    int nDumpType;
    char *pSubTypes;
    int i;
    int bNeedHelp;

    /* Check Arguments */
    bNeedHelp = FALSE;
    if ((argc < 4) || (argc > 6)) bNeedHelp = TRUE;

    if (argc >= 3) {
        nJumpNumber = strtoul(argv[1], NULL, 0);
        nDumpType = DT_UNKNOWN;
        if (strcmp(argv[2], "s") == 0) nDumpType = DT_SUMMARY;
        if (strcmp(argv[2], "d") == 0) nDumpType = DT_DETAIL;
        if (strcmp(argv[2], "t") == 0) nDumpType = DT_PROFILE_TAB;
        if (strcmp(argv[2], "c") == 0) nDumpType = DT_PROFILE_CSV;
        if (strcmp(argv[2], "p") == 0) nDumpType = DT_GNUPLOT;
        if (nDumpType == DT_UNKNOWN) bNeedHelp = TRUE;
    }

    pSubTypes = NULL;
    pLocation = NULL;
    if (argc >= 5) {
        pSubTypes = argv[3];
        if (argc > 5) {
            pLocation = argv[4];
            pInFilename = argv[5];
        } else {
            pInFilename = argv[4];
            pLocation = NULL;
        }
        switch (nDumpType) {
            case DT_GNUPLOT:
                for (i=0; i<strlen(pSubTypes); i++) {
                    if (strpbrk(&pSubTypes[i], "atsrp") == NULL) bNeedHelp = TRUE;
                }
                break;
            case DT_PROFILE_TAB:
                for (i=0; i<strlen(pSubTypes); i++) {
                    if (strpbrk(&pSubTypes[i], "sh") == NULL) bNeedHelp = TRUE;
                }
                break;
            case DT_PROFILE_CSV:
                for (i=0; i<strlen(pSubTypes); i++) {
                    if (strpbrk(&pSubTypes[i], "h") == NULL) bNeedHelp = TRUE;
                }
                break;
            default:
                bNeedHelp = TRUE;
                break;
        }
    }

    if (argc == 4) {
        pInFilename = argv[3];
    }

    if ((pSubTypes) && (strlen(pSubTypes) == 0))
        pSubTypes = NULL;

    if (bNeedHelp) {
        fprintf(stderr, "Neptune Dump V%d.%02d\n", VERSION/100, VERSION%100);
        fprintf(stderr, "Usage: neptune_dump <jump-num> <dump-type> [<sub-types>] [<Location>] <input-file>\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "       Where:\n");
        fprintf(stderr, "           <jump-num>   = Jump number to dump\n");
        fprintf(stderr, "               A jump number of '0' will dump all jumps in the file and\n");
        fprintf(stderr, "               has no meaning for some report types, like summary.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "           <dump-type>  = Type and Format of data to dump\n");
        fprintf(stderr, "               Supported formats are:\n");
        fprintf(stderr, "                   s    = Jump Summary Information (human readable)\n");
        fprintf(stderr, "                   d    = Jump Detail Information (human readable)\n");
        fprintf(stderr, "                   t    = Profile Data (Tabular Format)\n");
        fprintf(stderr, "                   c    = Profile Data (CSV Format)\n");
        fprintf(stderr, "                   p    = GnuPlot Commands (Can be piped to GnuPlot)\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "           <sub-types>  = Subformats for various dump types:\n");
        fprintf(stderr, "               For Type = t (can be zero or more of the following):\n");
        fprintf(stderr, "                   s    = Use spaces instead of tabs\n");
        fprintf(stderr, "                   h    = No Headers\n");
        fprintf(stderr, "               For Type = c (can be zero or more of the following):\n");
        fprintf(stderr, "                   h    = No Headers\n");
        fprintf(stderr, "               For Type = p (can be zero or more of the following):\n");
        fprintf(stderr, "                   a    = Altitude Plot\n");
        fprintf(stderr, "                   t    = TAS Speed Plot\n");
        fprintf(stderr, "                   s    = SAS Speed Plot\n");
        fprintf(stderr, "                   r    = Remove reset command from plot output\n");
        fprintf(stderr, "                   p    = Add pause command to plot output\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "           <Location>   = Optionally specifies the location of the\n");
        fprintf(stderr, "                           jump and is added to jump detail and plots.\n");
        fprintf(stderr, "                           If <Location> is used, <sub-types> MUST be\n");
        fprintf(stderr, "                           specified as well, but may be \"\".\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "           <input-file> = Neptune data file to read generated from\n");
        fprintf(stderr, "                           using neptune_read\n");
        fprintf(stderr, "\n");
        return -1;
    }

    /* Open Input File */
    pInFile = fopen(pInFilename, "rb");
    if (!pInFile) {
        fprintf(stderr, "Failed to open \"%s\" for reading!\n\n", pInFilename);
        return -2;
    }

    /* Check magic tag */
    if (!ReadString(pInFile, databuff, sizeof(databuff))) {
        fprintf(stderr, "Couldn't read input file \"%s\"!\n\n", pInFilename);
        fclose(pInFile);
        return -2;
    }
    for (i=strlen(databuff)-1; i>=0; i--) {
        if (isspace(databuff[i])) {
            databuff[i] = 0;
        } else {
            break;
        }
    }
    if (strcmp(databuff, "#NEPTUNE") != 0) {
        fprintf(stderr, "The input file \"%s\" doesn't appear to be a Neptune Data File!\n\n", pInFilename);
        fclose(pInFile);
        return -3;
    }

    /* Print Specified Report Type */
    switch (nDumpType) {
        case DT_SUMMARY:
            PrintSummary(pInFile, nDumpType, nJumpNumber, pSubTypes, pLocation);
            break;
        case DT_DETAIL:
            PrintDetail(pInFile, nDumpType, nJumpNumber, pSubTypes, pLocation);
            break;
        case DT_PROFILE_TAB:
        case DT_PROFILE_CSV:
            PrintProfile(pInFile, nDumpType, nJumpNumber, pSubTypes, pLocation);
            break;
        case DT_GNUPLOT:
            PrintGnuPlot(pInFile, nDumpType, nJumpNumber, pSubTypes, pLocation);
            break;
    }

    /* Close everything */
    fclose(pInFile);

    return 0;
}

