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

#include "usefulmacros.h"

#include "xlsx_helpers.h"
#include "userpath_helpers.h"

typedef struct {
    const char *DA;
    const char *DO;
    int excelCol;
} dataAttribute;

/* Checks if we can connect to the given IP via IEC61850 MMS, and writes the result to the worksheet. */
bool canConnectToIED(IedConnection connection, IedClientError *err, const char *deviceIP, int tcpPort, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr, int MMSCol);

/* Tries to write an MMS value to the spreadsheet. */
bool writeDeviceData(MmsValue *value, const char *daRef, lxw_worksheet *worksheet, int col, int row, FILE *fptr, IedClientError *err, bool firstIteration);

/* Calculates which row a specific device with the given IP-address should write to in the excel spreadsheet.*/
int calculateIpIndex(const char *ipAddr, int ipMinHost, FILE *fptr);

/* Gets user provided data attribute and writes it to the spreadsheet */
void processUserDevice(const char *ipAddr, int tcpPort, const char *path, int ipMinHost, lxw_worksheet *worksheet, int excelCol, FILE *fptr);

/* Tries to find and retrieve the given data attributes  */
void processDeviceData(IedConnection con, IedClientError *err, const char *deviceName, GeneralPath generalPath[], int count, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr);

/* Tries to find logical device 'LD0', if not found it return the first logical device. returns NULL if no devices found. */
char *get_LD_name(IedConnection con, IedClientError *err, FILE *fptr, char *iedName);

char *findLogicalNode(LinkedList logicalNodes);
#endif