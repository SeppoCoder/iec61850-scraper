#ifndef NETWORKING_H
#define NETWORKING_H

#include "xlsx_helpers.h"

/* Define a struct to hold the network data */
typedef struct {
    char  hostname[32];
    char ip[16];
    uint8_t subnet_mask;
    int success; 
} NetworkInfo;

/* struct for holding info about the IP range */
typedef struct {
    char IPPrefix[16];
    char hostMin[16];
    int minSuffix;
    char hostMax[16];
    int maxSuffix;
    int success;
} IPRange;

int initNetwork(void);

/* Tries to find the network with the given friendlyName */
NetworkInfo getInfoFromNetwork(const wchar_t *friendlyName, FILE *fptr);

/* calculates and return info about the available IP range of this subnet */
IPRange getIPRange(NetworkInfo *netInfo, FILE *fptr);

/* Tries to find and return info about an available network out of those given by the list of SSIDs. */
NetworkInfo findMatchingNetwork(FILE *fptr);

/* Checks if the device with the given IP address is pingable, and writes the result to the worksheet. */
void checkForPingAnswer(const char *ipAddress, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr, int pingCol);

/* Fetch the names of all found networks and return how many found */
int fetchAvailableNetworks(char *ptr[]);

#endif