#ifndef IEC61850_HELPERS_H
#define IEC61850_HELPERS_H

#pragma push_macro("DEPRECATED")
#undef DEPRECATED
#define DEPRECATED 

#include "iec61850_client.h"
#pragma pop_macro("DEPRECATED")

#include "usefulmacros.h"

#include "xlsx_helpers.h"
#include "userpath_helpers.h"
#include "networking.h"

/* Checks if we can connect to the given IP via IEC61850 MMS, and writes the result to the worksheet. */
bool connectToIED(IedConnection connection, IedClientError *err, const char *deviceIP, int tcpPort, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr, int MMSCol);

/* Tries to write an MMS value to the spreadsheet. */
bool writeDeviceData(MmsValue *value, const char *daRef, lxw_worksheet *worksheet, int col, int row, FILE *fptr, IedClientError *err, bool firstIteration);

/* Calculates which row a specific device with the given IP-address should write to in the excel spreadsheet.*/
int calculateIpIndex(const char *ipAddr, int ipMinHost, FILE *fptr, IPRange *ipRange);

/* Gets user provided data attribute and writes it to the spreadsheet */
void processUserDevice(const char *ipAddr, int tcpPort, const char *path, int ipMinHost, lxw_worksheet *worksheet, int excelCol, FILE *fptr, IPRange *ipRange);

/* Tries to find and retrieve the given data attributes  */
void processDeviceData(IedConnection con, IedClientError *err, const char *deviceName, GeneralPath generalPath[], int count, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr);

/* Tries to find logical device 'LD0', if not found it return the first logical device. returns NULL if no devices found. */
char *get_LD_name(IedConnection con, IedClientError *err, FILE *fptr, char *iedName);

/* Tries to find one of the predefined logical nodes. Returns the first LN in the list if none of those are found. */
char *findLogicalNode(LinkedList logicalNodes);
#endif