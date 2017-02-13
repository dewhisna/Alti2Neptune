/*
 * Neptune_Read
 *
 * This app connects to the Neptune Altimeter (by Alti-2) using
 * an IrComm IrDA port and reads all data and stores it to a file.
 * That data file can then be parsed and processed to extract
 * jump information.
 *
 * Written May 8, 2004 by Donna Whisnant
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

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <linux/types.h>
#include <linux/irda.h>

#include <sys/ioctl.h>
#include <sys/termios.h>
#include <fcntl.h>

#include <math.h>

#include "neptune_rec.h"

/* Defines */
//#define EXTRA_DEBUG 1
#define VERSION 100
#define RX_BUFFER_SIZE 1024
#define MAX_TX_FRAME 64

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif

/* Memory allocation for discovery */
#define DISC_MAX_DEVICES 10
#define DISC_BUF_LEN	sizeof(struct irda_device_list) + \
			sizeof(struct irda_device_info) * DISC_MAX_DEVICES

/* Custom Types */
typedef struct comm_port
{
    int         sock;               /* Socket Handle for direct IrCOMM emulation mode or -1 for driver mode */
    struct irda_device_info info;   /* IrDA Device Info from enumeration */
    __u8        n_lsap_sel;         /* IrDA:TinyTP LSAP Selector for Neptune */

    int         desc;               /* File Descriptor for driver mode or -1 for IrCOMM emulation mode */

    long        rx_bufsize;         /* size of the receive buffer */
    unsigned char rxbuf[RX_BUFFER_SIZE];   /* receive buffer */
    long        dwRead;
    long        dwReturned;
} COMM_PORT;

/* Comm Prototypes */
void InitPort(COMM_PORT *port);
int OpenPort(COMM_PORT *port, const char *pDevice);
int ConnectPort(COMM_PORT *port);
int ClosePort(COMM_PORT *port);
int Discover(COMM_PORT *port, const char* pDevice);
int PutChar(COMM_PORT *port, const char c);
int SendString(COMM_PORT *port, const char *pString);
int CheckForData(COMM_PORT *port);
int ReadString(void *pSource, unsigned char *pBuff, long nBufSize);
#ifdef EXTRA_DEBUG
int DumpParamTuple(unsigned char *pParam);
#endif

/* Misc Prototypes */
void SleepFine(long nSeconds, long nuSeconds);

/* ========================================================================== */

void InitPort(COMM_PORT *port)
{
    int i;

    /* Initialize our port struct: */
    for (i=0; i<sizeof(COMM_PORT); i++)
        ((char*)port)[i] = 0;
    port->sock = -1;
    port->desc = -1;
    port->n_lsap_sel = LSAP_ANY;
    port->rx_bufsize = RX_BUFFER_SIZE;
    port->dwRead = 0;
    port->dwReturned = 0;
}

int OpenPort(COMM_PORT *port, const char *pDevice)
{
    struct termios terminfo;

    /* close port if one is open */
    ClosePort(port);

    if (pDevice) {
        /* Here if using real kernel module IrCOMM device driver */

        port->desc = open(pDevice, O_RDWR | O_NOCTTY | O_NONBLOCK);
        if  (port->desc < 0) {
            perror("Opening Device ");
            return FALSE;
        }

        tcgetattr(port->desc, &terminfo);
        terminfo.c_iflag = 0;   /* IGNBRK | IGNPAR; */
        terminfo.c_oflag = 0;
        terminfo.c_cflag = CS8|CREAD|CLOCAL;
        terminfo.c_lflag = 0;
        terminfo.c_cc[4] = 0;
        terminfo.c_cc[5] = 5;

        cfsetospeed(&terminfo, B9600);
        cfsetispeed(&terminfo, B9600);

        if (tcsetattr(port->desc, TCSANOW, &terminfo) != 0) {
            perror( "Configuring Device " );
            return FALSE;
        }

        if (fcntl(port->desc, F_SETFL, O_NONBLOCK) < 0) {
            perror("Device Setup ");
            return FALSE;
        }

        if (tcflush(port->desc, TCIOFLUSH ) == -1) {
            perror( "Flushing Device " );
            return FALSE;
        }
    } else {
        /* Here if we're emulating the IrCOMM layer and talking direct to TinyTP socket */

        port->sock = socket(AF_IRDA, SOCK_STREAM /* SOCK_SEQPACKET */, 0);
        if (port->sock < 0) {
            perror("Creating IrDA socket (no IrDA stack?) ");
            return FALSE;
        }
    }

    return TRUE;
}

int ConnectPort(COMM_PORT *port)
{
    struct sockaddr_irda peer;
    const unsigned char pServiceSelectMessage[] =
            { 0x03,
                0x00, 0x01, 0x04                        /* Select 9-Wire Cooked */
            };
    const unsigned char pConnectSettingsMessage[] =
            { 0x0F,
                0x10, 0x04, 0x00, 0x00, 0x25, 0x80,     /* Baud = 9600 */
                0x11, 0x01, 0x03,                       /* N, 8, 1 */
                0x12, 0x01, 0x00,                       /* No Flow Control */
                0x20, 0x01, 0xC0                        /* DTR=RTS=on, no delta */
            };

    if ((port->desc < 0) &&
        (port->sock < 0)) return FALSE;

    printf("Start Neptune Transmitting");
    fflush(stdout);

    if (port->sock < 0) {
        /* Here if using real kernel module IrCOMM device driver */

        while (!PutChar(port, ' ')) {
            SleepFine(1, 0);
            printf(".");
            fflush(stdout);
        }
        printf("Connected\n");
    } else {
        /* Here if we're emulating the IrCOMM layer and talking direct to TinyTP socket */

        /* Search IrDA for Neptune Device: */
        while (!Discover(port, "Neptune")) {
            SleepFine(1, 0);
            printf(".");
            fflush(stdout);
        }

        peer.sir_family = AF_IRDA;
        peer.sir_lsap_sel = port->n_lsap_sel;
        peer.sir_addr = port->info.daddr;
        strncpy(peer.sir_name, "NeptuneRead:IrDA:TinyTP", 25);

        if (connect(port->sock, (struct sockaddr*) &peer, sizeof(struct sockaddr_irda))) {
            perror("Connect to IrDA socket ");
            return FALSE;
        }
        printf("Connected\n");
        fflush(stdout);

        if (send(port->sock, pServiceSelectMessage, sizeof(pServiceSelectMessage), 0) == -1)
            perror("Error Sending Service Type Select ");

        if (send(port->sock, pConnectSettingsMessage, sizeof(pConnectSettingsMessage), 0) == -1)
            perror("Error Sending Connect Settings ");

        printf("Sending Wakeup");
        fflush(stdout);
        while (!PutChar(port, ' ')) {
            SleepFine(1, 0);
            printf(".");
            fflush(stdout);
        }
        printf("\n");
    }

    fflush(stdout);
    SleepFine(5, 0);

    return TRUE;
}

int ClosePort(COMM_PORT *port)
{
    if (port->desc >= 0) {
        close(port->desc);
        port->desc = -1;
    }
    if (port->sock >= 0) {
        close(port->sock);
        port->sock = -1;
    }

    return TRUE;
}

int Discover(COMM_PORT *port, const char* pDevice)
{
    struct irda_device_list *list;      /* List of device */
    unsigned char buf[DISC_BUF_LEN];    /* Actual memory allocation */
    int len;
    int i;
//    int j;
//    struct irda_ias_set ias_query;
//    unsigned char *pParam;
    

    /* Set the list to point to the correct place */
    list = (struct irda_device_list *) buf;
    len = DISC_BUF_LEN;

    /* Ask for the discovery log */
    if (getsockopt(port->sock, SOL_IRLMP, IRLMP_ENUMDEVICES, buf, &len)) {
        /* Discovery log empty (normal case) or error */
        return FALSE;
    }
    /* Is there any addresses ? */
    if (list->len <= 0)
        /* Discovery log empty (exceptional case) */
        return FALSE;

    /* Dump list found: */
#ifdef EXTRA_DEBUG
    fprintf(stderr, "\nDiscovered %ld devices:\n", list->len);
    for (i=0; i < list->len; i++) {
        fprintf(stderr, "    \"%s\"%s\n", list->dev[i].info,
                ((strcmp(list->dev[i].info, pDevice) == 0) ? " -- Found" : ""));
    }
    fflush(stderr);
#endif

    /* Go through the list */
    for (i=0; i < list->len; i++) {
        if (strcmp(list->dev[i].info, pDevice) == 0) {
            /* Copy device info for our device: */
            memcpy(&port->info, &list->dev[i], sizeof(struct irda_device_info));
            port->n_lsap_sel = LSAP_ANY;

//
// TODO -- Figure out why the following doesn't work:
//              Returns an LSEL of 0x04 instead of 0x37 and
//              the parameters query flat out fails:
//
//            /* Query the LSAP Selector number for the TinyTP (standard 3-Wire Cooked and 9-Wire Cooked modes) */
//            len = sizeof(ias_query);
//            ias_query.daddr = list->dev[i].daddr;
//            strcpy(ias_query.irda_class_name, "IrCOMM");
//            strcpy(ias_query.irda_attrib_name, "IrDA:TinyTP:LsapSel");
//            if ((getsockopt(port->sock, SOL_IRLMP, IRLMP_IAS_QUERY, &ias_query, &len) == 0) &&
//                (ias_query.irda_attrib_type == IAS_INTEGER)) {
//                port->n_lsap_sel = ias_query.attribute.irda_attrib_int;
//#ifdef EXTRA_DEBUG
//                fprintf(stderr, "LSAP Selector: %d\n", port->n_lsap_sel);
//#endif
//            } else {
//                perror("Couldn't get LSAP Selector -- using LSAP_ANY ");
//                port->n_lsap_sel = LSAP_ANY;
//            }
//
//#ifdef EXTRA_DEBUG
//            fprintf(stderr, "\nIrCOMM Parameters :");
//            len = sizeof(ias_query);
//            ias_query.daddr = list->dev[i].daddr;
//            strcpy(ias_query.irda_class_name, "IrCOMM");
//            strcpy(ias_query.irda_attrib_name, "Parameters");
//            if ((getsockopt(port->sock, SOL_IRLMP, IRLMP_IAS_QUERY, &ias_query, &len) == 0) &&
//                (ias_query.irda_attrib_type == IAS_OCT_SEQ)) {
//                for (j=0; j<ias_query.attribute.irda_attrib_octet_seq.len; j++) {
//                    fprintf(stderr, " %02X", ias_query.attribute.irda_attrib_octet_seq.octet_seq[j]);
//                }
//                fprintf(stderr, "\n");
//                j = ias_query.attribute.irda_attrib_octet_seq.len;
//                pParam = &ias_query.attribute.irda_attrib_octet_seq.octet_seq[0];
//                while (j>0) {
//                    len = DumpParamTuple(pParam);
//                    j -= len;
//                    pParam += len;
//                }
//            } else {
//                fprintf(stderr, " *** Error Couldn't Read Parameters ***\n");
//            }
//            fflush(stderr);
//#endif

            return TRUE;
        }
    }

    /* Dump list found: */
#ifdef EXTRA_DEBUG
    fprintf(stderr, "Looking for: \"%s\" -- NOT FOUND\n", pDevice);
#endif

    /* No match */
    return FALSE;
}

int SendString(COMM_PORT *port, const char *pString)
{
    int len;
    unsigned char txbuf[MAX_TX_FRAME];

    if (port->sock < 0) {
        /* Here if using real kernel module IrCOMM device driver */

        len = strlen(pString);
        return (write(port->desc, pString, len) == len);
    } else {
        /* Here if we're emulating the IrCOMM layer and talking direct to TinyTP socket */

        len = strlen(pString) + 1;
        if (strlen(pString) > (MAX_TX_FRAME-2)) len = MAX_TX_FRAME-1;
        txbuf[0] = 0;   /* No control stream bytes */
        strncpy(&txbuf[1], pString, MAX_TX_FRAME-2);
        if (send(port->sock, txbuf, len, 0) != -1)
            return TRUE;
    }

    return FALSE;
}

int PutChar(COMM_PORT *port, const char c)
{
    unsigned char buf[2];

    if (port->sock < 0) {
        /* Here if using real kernel module IrCOMM device driver */

        if (write(port->desc, &c, 1) == 1) return TRUE;
    } else {
        /* Here if we're emulating the IrCOMM layer and talking direct to TinyTP socket */

        buf[0] = 0;     /* No control stream bytes */
        buf[1] = c;
        if (send(port->sock, buf, 2, 0) != -1)
            return TRUE;
    }

    return FALSE;
}

int CheckForData(COMM_PORT *port)
{
    fd_set rfds;
    struct timeval tv;
    int retval;

    FD_ZERO(&rfds);
    FD_SET(((port->sock < 0) ? port->desc : port->sock), &rfds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    retval = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);
    /* Note: Don't rely on value of tv now! */

    return retval;
}

int ReadString(void *pSource, unsigned char *pBuff, long nBufSize)
{
    COMM_PORT *port = (COMM_PORT*)pSource;
    int havedata;
    int i;
    int len;
    unsigned char *pParam;
    const unsigned char pDTESettingsMessage[] = { 0x03, 0x20, 0x01, 0xC0 };   /* DTR=RTS=on, no delta */

    if (nBufSize < 1) return FALSE;
    if (pBuff == 0) return FALSE;

    nBufSize--;     /* Leave room for terminating nul */
    havedata = FALSE;

    while (1) {
        while ((nBufSize) && (port->dwReturned < port->dwRead)) {
            if (port->rxbuf[port->dwReturned] == '\n') {
                *pBuff = port->rxbuf[port->dwReturned];
                pBuff++;
                *pBuff = 0;
                port->dwReturned++;
                return TRUE;
            }
            *pBuff = port->rxbuf[port->dwReturned];
            pBuff++;
            nBufSize--;
            port->dwReturned++;
            havedata = TRUE;
        }
        if (nBufSize == 0) {
            *pBuff = 0;
            return TRUE;
        }

        if (!CheckForData(port)) {
            *pBuff = 0;
            return havedata;
        }

        if (port->sock < 0) {
            /* Here if using real kernel module IrCOMM device driver */

            port->dwRead = read(port->desc, port->rxbuf, port->rx_bufsize);
            port->dwReturned = 0;

// Warning: Enabling the following causes so much overhead data loss will be experienced!
//#ifdef EXTRA_DEBUG
//            fprintf(stderr, "RxData:");
//            for (i=0; i<port->dwRead; i++) {
//                fprintf(stderr, " %02X", port->rxbuf[i]);
//            }
//            fprintf(stderr, "\n");
//            fflush(stderr);
//#endif

            if (port->dwRead == 0) {
                *pBuff = 0;
                break;
            }
        } else {
            /* Here if we're emulating the IrCOMM layer and talking direct to TinyTP socket */

            port->dwReturned = 0;
            port->dwRead = recv(port->sock, port->rxbuf, port->rx_bufsize, MSG_TRUNC);
            if (port->dwRead == -1) {
                perror("Reading Packet ");
                port->dwRead = 0;
            }
            if (port->dwRead > port->rx_bufsize) {
                fprintf(stderr, "\n    *** Warning: Truncated packet : Size = %ld  Max Allowed = %ld\n", port->dwRead, port->rx_bufsize);
                port->dwRead = port->rx_bufsize;
            }
            if (port->dwRead == 0) continue;    /* loop if we receive no data -- this should never happen */

// Warning: Enabling the following causes so much overhead data loss will be experienced!
//#ifdef EXTRA_DEBUG
//            fprintf(stderr, "RxData:");
//            for (i=0; i<port->dwRead; i++) {
//                fprintf(stderr, " %02X", port->rxbuf[i]);
//            }
//            fprintf(stderr, "\n");
//            if (port->rxbuf[0])
//                fprintf(stderr, "    Num Control Bytes: %ld\n", (long)port->rxbuf[0]);
//            i = port->rxbuf[0];
//            pParam = &port->rxbuf[1];
//            while (i > 0) {
//                len = DumpParamTuple(pParam);
//                i -= len;
//                pParam += len;
//            }
//            fflush(stderr);
//#endif

            port->dwReturned += port->rxbuf[0] + 1;     /* Advance past the control channel info */

            /* Look for and process any request for line status and we'll ignore everything else */
            i = port->rxbuf[0];
            pParam = &port->rxbuf[1];
            while (i > 0) {
                len = pParam[1] + 2;
                if (pParam[0] == 0x22) {
                    if (send(port->sock, pDTESettingsMessage, sizeof(pDTESettingsMessage), 0) == -1)
                        perror("Error Sending Requested Line Status ");
                }
                i -= len;
                pParam += len;
            }
        }
    }

    return havedata;
}

#ifdef EXTRA_DEBUG
int DumpParamTuple(unsigned char *pParam)
{
    int nSize;
    const char strParamLabel[] = "    Param(%s) =";
    int i;

    nSize = pParam[1] + 2;      /* Total Param Size is the PL field value +2 -- one for PI and one for PL */

    switch (pParam[0]) {
        case 0x00:
        case 0x80:
            fprintf(stderr, strParamLabel, "Service Type");
            if (pParam[1]) {
                if (pParam[2] & 0x01) fprintf(stderr, " 3-WireRaw ");
                if (pParam[2] & 0x02) fprintf(stderr, " 3-Wire ");
                if (pParam[2] & 0x04) fprintf(stderr, " 9-Wire ");
                if (pParam[2] & 0x08) fprintf(stderr, " Centronics ");
                if (!pParam[2]) fprintf(stderr, " <none>");
            } else {
                fprintf(stderr, " ???");
            }
            fprintf(stderr, "\n");
            break;
        case 0x01:
        case 0x81:
            fprintf(stderr, strParamLabel, "Port Type");
            if (pParam[1]) {
                if (pParam[2] & 0x01) fprintf(stderr, " Serial ");
                if (pParam[2] & 0x02) fprintf(stderr, " Parallel ");
                if (!pParam[2]) fprintf(stderr, " <none>");
            } else {
                fprintf(stderr, " ???");
            }
            fprintf(stderr, "\n");
            break;
        case 0x02:
        case 0x82:
            fprintf(stderr, strParamLabel, "Fixed Port Name");
            fprintf(stderr, " \"");
            for (i=0; i<(int)pParam[1]; i++) {
                fprintf(stderr, "%c", pParam[2+i]);
            }
            fprintf(stderr, "\"\n");
            break;
        case 0x10:
            fprintf(stderr, strParamLabel, "Data Rate");
            if (pParam[1] == 4) {
                fprintf(stderr, " %lu\n", pParam[2]*16777216ul + pParam[3]*65536ul + pParam[4]*256ul + pParam[5]);
            } else {
                fprintf(stderr, " ???\n");
            }
            break;
        case 0x11:
            fprintf(stderr, strParamLabel, "Data Format");
            if (pParam[1]) {
                if (pParam[2] & 0x08) {
                    switch (pParam[2] & 0x30) {
                        case 0x00:
                            fprintf(stderr, "O,");
                            break;
                        case 0x10:
                            fprintf(stderr, "E,");
                            break;
                        case 0x20:
                            fprintf(stderr, "M,");
                            break;
                        case 0x30:
                            fprintf(stderr, "S,");
                            break;
                    }
                } else {
                    fprintf(stderr, "N,");
                }
                switch (pParam[2] & 0x03) {
                    case 0:
                        fprintf(stderr, "5,");
                        break;
                    case 1:
                        fprintf(stderr, "6,");
                        break;
                    case 2:
                        fprintf(stderr, "7,");
                        break;
                    case 3:
                        fprintf(stderr, "8,");
                        break;
                }
                if (pParam[2] & 0x04) {
                    if (pParam[2] & 0x03) {
                        fprintf(stderr, "2\n");
                    } else {
                        fprintf(stderr, "1.5\n");
                    }
                } else {
                    fprintf(stderr, "1\n");
                }
            } else {
                fprintf(stderr, " ???\n");
            }
            break;
        case 0x12:
            fprintf(stderr, strParamLabel, "Flow Control");
            if (pParam[1]) {
                if (pParam[2] & 0x01) fprintf(stderr, " XON/XOFF(in) ");
                if (pParam[2] & 0x02) fprintf(stderr, " XON/XOFF(out) ");
                if (pParam[2] & 0x04) fprintf(stderr, " RTS/CTS(in) ");
                if (pParam[2] & 0x08) fprintf(stderr, " RTS/CTS(out) ");
                if (pParam[2] & 0x10) fprintf(stderr, " DSR/DTR(in) ");
                if (pParam[2] & 0x20) fprintf(stderr, " DSR/DTR(out) ");
                if (pParam[2] & 0x40) fprintf(stderr, " ENQ/ACK(in) ");
                if (pParam[2] & 0x80) fprintf(stderr, " ENQ/ACK(out) ");
                if (!pParam[2]) fprintf(stderr, " <none>");
                fprintf(stderr, "\n");
            } else {
                fprintf(stderr, " ???\n");
            }
            break;
        case 0x13:
            fprintf(stderr, strParamLabel, "XON/XOFF Chars");
            if (pParam[1] == 2) {
                fprintf(stderr, " XON=0x%02X  XOFF=0x%02X\n", pParam[2], pParam[3]);
            } else {
                fprintf(stderr, " ???\n");
            }
            break;
        case 0x14:
            fprintf(stderr, strParamLabel, "ENQ/ACK Chars");
            if (pParam[1] == 2) {
                fprintf(stderr, " ENQ=0x%02X  ACK=0x%02X\n", pParam[2], pParam[3]);
            } else {
                fprintf(stderr, " ???\n");
            }
            break;
        case 0x15:
            fprintf(stderr, strParamLabel, "Line Status");
            if (pParam[1]) {
                if (pParam[2] & 0x02) fprintf(stderr, " Overrun ");
                if (pParam[2] & 0x04) fprintf(stderr, " Parity ");
                if (pParam[2] & 0x08) fprintf(stderr, " Framing ");
                if (!pParam[2]) {
                    fprintf(stderr, " <none>");
                } else {
                    fprintf(stderr, " Error(s)");
                }
                fprintf(stderr, "\n");
            } else {
                fprintf(stderr, " ???\n");
            }
            break;
        case 0x16:
            fprintf(stderr, strParamLabel, "Break");
            if (pParam[1]) {
                if (pParam[2] & 0x01) {
                    fprintf(stderr, " Set");
                } else {
                    fprintf(stderr, " Clr");
                }
                fprintf(stderr, "\n");
            } else {
                fprintf(stderr, " ???\n");
            }
            break;
        case 0x20:
            fprintf(stderr, strParamLabel, "DTE Lines");
            if (pParam[1]) {
                if (pParam[2] & 0x04) {
                    fprintf(stderr, "DTR=ON");
                } else {
                    fprintf(stderr, "DTR=OFF");
                }
                if (pParam[2] & 0x01) fprintf(stderr, "**");
                fprintf(stderr, "  ");
                if (pParam[2] & 0x08) {
                    fprintf(stderr, "RTS=ON");
                } else {
                    fprintf(stderr, "RTS=OFF");
                }
                if (pParam[2] & 0x02) fprintf(stderr, "**");
                fprintf(stderr, "\n");
            } else {
                fprintf(stderr, " ???\n");
            }
            break;
        case 0x21:
            fprintf(stderr, strParamLabel, "DCE Lines");
            if (pParam[1]) {
                if (pParam[2] & 0x10) {
                    fprintf(stderr, "CTS=ON");
                } else {
                    fprintf(stderr, "CTS=OFF");
                }
                if (pParam[2] & 0x01) fprintf(stderr, "**");
                fprintf(stderr, "  ");
                if (pParam[2] & 0x20) {
                    fprintf(stderr, "DSR=ON");
                } else {
                    fprintf(stderr, "DSR=OFF");
                }
                if (pParam[2] & 0x02) fprintf(stderr, "**");
                fprintf(stderr, "  ");
                if (pParam[2] & 0x40) {
                    fprintf(stderr, "RI=ON");
                } else {
                    fprintf(stderr, "RI=OFF");
                }
                if (pParam[2] & 0x04) fprintf(stderr, "**");
                fprintf(stderr, "  ");
                if (pParam[2] & 0x80) {
                    fprintf(stderr, "CD=ON");
                } else {
                    fprintf(stderr, "CD=OFF");
                }
                if (pParam[2] & 0x08) fprintf(stderr, "**");
                fprintf(stderr, "\n");
            } else {
                fprintf(stderr, " ???\n");
            }
            break;
        case 0x22:
            fprintf(stderr, "    Param(Line Settings Poll Request)\n");
            break;
        default:
            fprintf(stderr, strParamLabel, "Unknown");
            for (i=0; i<(int)pParam[1]+2; i++) {
                fprintf(stderr, " %02X", pParam[i]);
            }
            fprintf(stderr, "\n");
            break;
    }

    return nSize;
}
#endif

/* ========================================================================== */

void SleepFine(long nSeconds, long nuSeconds)
{
    struct timeval tv;

    tv.tv_sec = nSeconds;
    tv.tv_usec = nuSeconds;

    /* Use the select method as a fine-grained sleep */
    select(0, NULL, NULL, NULL, &tv);
    /* Note: Don't rely on value of tv now! */
}

/* ========================================================================== */

int main(int argc, char *argv[])
{
    COMM_PORT myPort;
    FILE *pOutFile;
    FILE *pTmpFile;
    unsigned char databuff[MAX_RECORD_SIZE];
    long nAltitude;
    double nAvgSpeed;
    double nSpeed;
    int type;
    int done;
    long datatype;
    char *pOutFilename;
    char *pDeviceName;
    int nNepVersionHi;
    int nNepVersionLo;
    int nNepVersionRev;
    char strNepSerialNo[10];
    int i;
    const char *strJumpTypes[16] = {
                        "Group 1", "Group 2", "Group 3", "Group 4",
                        "4-way", "8-way", "10-way", "16-way",
                        "Freefly", "Big Way", "Tandem", "AFF",
                        "Birdman", "Camera", "Student", "Group 5" };

    /* Check Arguments */
    if ((argc < 2) || (argc > 3)) {
        fprintf(stderr, "Neptune Read V%d.%02d\n", VERSION/100, VERSION%100);
        fprintf(stderr, "Usage: neptune_read [<IrCOMM-Device>] <output-file>\n\n");
        fprintf(stderr, "       If <IrCOMM-Device> is omitted, this app will emulate the\n");
        fprintf(stderr, "       IrCOMM and do direct comm to the TinyTP IrDA layer, which\n");
        fprintf(stderr, "       is useful on systems where not all layers are supported.\n");
        fprintf(stderr, "       However, to use a kernel module for IrCOMM instead, simply\n");
        fprintf(stderr, "       specify an <IrCOMM-Device>, like /dev/ircomm0, for example.\n\n");
        return -1;
    }
    if (argc == 2) {
        pDeviceName = NULL;
        pOutFilename = argv[1];
    }
    if (argc >= 3) {
        pDeviceName = argv[1];
        pOutFilename = argv[2];
    }

    /* Initialize our port struct: */
    InitPort(&myPort);

    /* Open our port */
    if (!OpenPort(&myPort, pDeviceName))
        return -2;

    /* Open Output File */
    pOutFile = fopen(pOutFilename, "wb");
    if (!pOutFile) {
        fprintf(stderr, "Failed to open \"%s\" for writing!\n\n", pOutFilename);
        ClosePort(&myPort);
        return -3;
    }

    /* Open Temp File */
    pTmpFile = tmpfile();
    if (!pTmpFile) {
        fprintf(stderr, "Failed to open temporary file!\n\n");
        ClosePort(&myPort);
        fclose(pOutFile);
        return -4;
    }

    /* Start polling loop waiting for Neptune discovery: */
    if (!ConnectPort(&myPort)) {
        ClosePort(&myPort);
        fclose(pOutFile);
        fclose(pTmpFile);
        return -5;
    }

    /* Write Magic to output file */
    fprintf(pOutFile, "#NEPTUNE\r\n");

    /* Start data transfer by sending command to Neptune */
    printf("Commanding Version Transfer");
    fflush(stdout);
    SendString(&myPort, " 01 80 80 ");
    printf("\n");
    fflush(stdout);

    /* Loop, but don't exit on bad records or the stupid Neptune will get stuck */
    done = FALSE;
    datatype = 0;
    while ((!done) &&
            ((type = GetNextRecord(&myPort, databuff)) != -1)) {
        fprintf(pTmpFile, "%s\r\n", databuff);

        switch (type) {
            case 0:     /* Version Info */
                fprintf(pOutFile, "!\r\n! Neptune Altimeter Jump Data\r\n!\r\n");
                nNepVersionHi = (ConvHexByte(&databuff[9]) >> 4) & 0x0F;
                nNepVersionLo = ConvHexByte(&databuff[9]) & 0x0F;
                nNepVersionRev = ConvHexByte(&databuff[12]);
                if ((nNepVersionHi == 0) && (nNepVersionLo == 0) && (nNepVersionRev < 14))
                    nNepVersionHi = 2;
                fprintf(pOutFile, "! Neptune Software v%u.%u.%u\r\n",
                            nNepVersionHi, nNepVersionLo, nNepVersionRev);
                for (i=0; i<9; i++) {
                    strNepSerialNo[i] = ConvHexByte(&databuff[i*3+15]);
                    if (strNepSerialNo[i] == 0x20) strNepSerialNo[i] = 0x00;    /* String is right padded with spaces, so whitespace trim */
                }
                strNepSerialNo[9] = 0;
                fprintf(pOutFile, "! Neptune Serial No: %s\r\n", strNepSerialNo);
                fprintf(pOutFile, "!\r\n");

                printf("Commanding Data Transfer");
                SendString(&myPort, "01 80 80 ");
                printf("\nReading");
                break;
            case 1:     /* Jump Summary */
                fprintf(pOutFile, "! Jump Summary:\r\n");
                fprintf(pOutFile, "!    Number Jump Records   = %lu\r\n",
                            ConvHexByte(&databuff[6]) + ConvHexByte(&databuff[9])*256ul);
                fprintf(pOutFile, "!    Number Jump Profiles  = %u\r\n",
                            ConvHexByte(&databuff[12]));
                fprintf(pOutFile, "!    Total Jumps Made      = %lu\r\n",
                            ConvHexByte(&databuff[15]) + ConvHexByte(&databuff[18])*256ul);
                fprintf(pOutFile, "!    Total FreeFall Time   = %lu sec\r\n",
                            ConvHexByte(&databuff[21]) + ConvHexByte(&databuff[24])*256ul +
                            ConvHexByte(&databuff[27])*65536ul + ConvHexByte(&databuff[30])*16777216ul);
                fprintf(pOutFile, "!    Last Jump Number      = %lu\r\n",
                            ConvHexByte(&databuff[33]) + ConvHexByte(&databuff[36])*256ul + 1ul);
                fprintf(pOutFile, "!\r\n");
                break;
            case 2:     /* Jump Record */
                fprintf(pOutFile, "! Jump Record -- Jump Number %lu:\r\n",
                            ConvHexByte(&databuff[6]) + ConvHexByte(&databuff[9])*256ul + 1ul);
                fprintf(pOutFile, "!    Jump Date/Time        = %02u/%02u/%02u  %02u:%02u\r\n",
                            ConvHexByte(&databuff[21]), ConvHexByte(&databuff[18]), ConvHexByte(&databuff[24]),
                            ConvHexByte(&databuff[15]), ConvHexByte(&databuff[12]));
                fprintf(pOutFile, "!    Jump Type             = %s\r\n",
                            ((ConvHexByte(&databuff[27]) < 16) ? strJumpTypes[ConvHexByte(&databuff[27])] : "<Unknown>"));
                fprintf(pOutFile, "!    Data Version          = %u.%u.%u\r\n",
                            ((ConvHexByte(&databuff[57])>>4) & 0x0F) + 1,
                            (ConvHexByte(&databuff[57]) & 0x0F),
                            (ConvHexByte(&databuff[60])));
                fprintf(pOutFile, "!    Data SW Type          = %u\r\n", ConvHexByte(&databuff[63]));
                nAvgSpeed = 0.0;
                i = 0;
                nSpeed = (round(ConvHexByte(&databuff[30])*22.3694))/10.0;
                if (nSpeed) {
                    nAvgSpeed += nSpeed;
                    i++;
                }
                fprintf(pOutFile, "!    Max FF Speed (TAS)    = %.1f mph\r\n", nSpeed);
                nSpeed = (round(ConvHexByte(&databuff[33])*22.3694)/10.0);
                if (nSpeed) {
                    nAvgSpeed += nSpeed;
                    i++;
                }
                fprintf(pOutFile, "!    12K FF Speed (TAS)    = %.1f mph\r\n", nSpeed);
                nSpeed = (round(ConvHexByte(&databuff[36])*22.3694)/10.0);
                if (nSpeed) {
                    nAvgSpeed += nSpeed;
                    i++;
                }
                fprintf(pOutFile, "!     9K FF Speed (TAS)    = %.1f mph\r\n", nSpeed);
                nSpeed = (round(ConvHexByte(&databuff[39])*22.3694)/10.0);
                if (nSpeed) {
                    nAvgSpeed += nSpeed;
                    i++;
                }
                fprintf(pOutFile, "!     6K FF Speed (TAS)    = %.1f mph\r\n", nSpeed);
                fprintf(pOutFile, "!     3K FF Speed (TAS)    = %.1f mph\r\n",
                            (round(ConvHexByte(&databuff[42])*22.3694)/10.0));
                if (i) nAvgSpeed = round((nAvgSpeed*10.0)/i)/10.0;
                fprintf(pOutFile, "!    Avg FF Speed (TAS)    = %.1f mph\r\n", nAvgSpeed);
                fprintf(pOutFile, "!    Exit Altitude (AGL)   = %lu ft\r\n",
                            (unsigned long)lround((ConvHexByte(&databuff[45]) + ConvHexByte(&databuff[48])*256ul)*3.28084));
                fprintf(pOutFile, "!    Deploy Altitude (AGL) = %lu ft\r\n",
                            (unsigned long)lround((ConvHexByte(&databuff[51]) + ConvHexByte(&databuff[54])*256ul)*3.28084));
                fprintf(pOutFile, "!    Freefall Time         = %lu sec\r\n",
                            ConvHexByte(&databuff[66]) + ConvHexByte(&databuff[69])*256ul);
                fprintf(pOutFile, "!\r\n");
                break;
            case 3:     /* End of all data */
                done = TRUE;
                break;
            case 4:     /* Jump Profile Data Stream Type */
                switch (ConvHexByte(&databuff[6])) {
                    case 5:
                        datatype = 1;
                        break;
                    case 6:
                    case 7:
                        datatype = 2;
                        break;
                }
                break;
            case 5:     /* Profile Start */
                fprintf(pOutFile, "! Jump Profile -- Jump Number %lu:\r\n",
                            ConvHexByte(&databuff[6]) + ConvHexByte(&databuff[9])*256ul + 1ul);
                nAltitude = (ConvHexByte(&databuff[12]) + ConvHexByte(&databuff[15])*256ul);
                if (nAltitude > 32767l) nAltitude = nAltitude - 65534l;     /* Why is this 65534 in paralog and not 65536 ?? */
                fprintf(pOutFile, "!    Ground Altitude (MSL) = %ld ft\r\n", lround(nAltitude*3.28084));
                fprintf(pOutFile, "!    Exit Altitude (AGL)   = %lu ft\r\n",
                            (unsigned long)lround((ConvHexByte(&databuff[18]) + ConvHexByte(&databuff[21])*256ul)*3.28084));
                fprintf(pOutFile, "!    Freefall Start Time   = %.2f sec\r\n",
                            (ConvHexByte(&databuff[24]) + ConvHexByte(&databuff[27])*256ul)*0.25);
                fprintf(pOutFile, "!    Canopy Start Time     = %.2f sec\r\n",
                            (ConvHexByte(&databuff[30]) + ConvHexByte(&databuff[33])*256ul)*0.25);
                fprintf(pOutFile, "!\r\n");

                datatype = 0;
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

        printf(".");
        fflush(stdout);
    }
    printf("Done\n\n");

    fseek(pTmpFile, 0L, SEEK_SET);
    while ((i = fgetc(pTmpFile)) != EOF)
        fputc(i, pOutFile);

    /* Close everything */
    ClosePort(&myPort);
    fclose(pOutFile);
    fclose(pTmpFile);

    return 0;
}

