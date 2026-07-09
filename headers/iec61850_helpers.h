#ifndef IEC61850_HELPERS_H
#define IEC61850_HELPERS_H

// 1. Save whatever definition of DEPRECATED xlsxwriter (or anyone else) made
#pragma push_macro("DEPRECATED")

// 2. Wipe it out completely
#undef DEPRECATED

// 3. Force it to be empty just for libiec61850
#define DEPRECATED 

// 4. Safely include the client
#include "iec61850_client.h"

// 5. Restore the original DEPRECATED macro so xlsxwriter stays happy
#pragma pop_macro("DEPRECATED")

#include "xlsx_helpers.h"

typedef struct {
    const char *DA;
    const char *DO;
    int excelCol;
} dataAttribute;

/* Checks if we can connect to the given IP via IEC61850 MMS, and writes the result to the worksheet. */
bool canConnectToIED(IedConnection connection, IedClientError *err, const char *deviceIP, int tcpPort, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr, int MMSCol);

/* Tries to find the given global data attributes and write their data to the worksheet. */
void processDeviceData(IedConnection con, IedClientError *err, const char *deviceName, const char *nodeName, dataAttribute DAs[], int DASize, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr);

/* Tries to find logical device 'LD0', if not found it return the first logical device. returns NULL if no devices found. */
char *get_LD_name(IedConnection con, IedClientError *err, FILE *fptr, char *iedName);

char *findLogicalNode(LinkedList logicalNodes, const char *priorityName);

void testLDList(IedConnection connection, IedClientError *err, const char *hostname);

#endif