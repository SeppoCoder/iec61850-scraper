/*
*   main.c   
*
*   Author: Sebastian Ling
*   Last modified: 2026-07-03
*
*   ------------- Compiling and running the program -------------
*   Compiling on linux with mingw32: x86_64-w64-mingw32-gcc src/main.c -o src/main.exe -I/home/sebli/libiec61850/.install/include -I/home/sebli/libxlsxwriter/include -L/home/sebli/iec61850-scraper/libs -liec61850 -lxlsxwriter -lz -liphlpapi -lws2_32 -lbcrypt
*   Running on linux with wine: sudo wine src/main.exe 
*/

#if defined _WIN32 || defined _WIN64
    #include <winsock2.h>
    #include <iphlpapi.h>
    #include <icmpapi.h>
    #include <ws2tcpip.h>
    #include <stdio.h>
    #include <stdlib.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

// IEC61850 client library to handle connections to server
#include "iec61850_client.h"

// xlsx library for handling writing to excel spreadsheet
#include "xlsxwriter.h"

// Define a struct to hold the network data
typedef struct {
    char  hostname[32];
    char ip[16];
    uint8_t subnet_mask;
    int success; 
} NetworkInfo;

// struct for holding info about the IP range
typedef struct {
    char IPPrefix[16];
    char hostMin[16];
    int minSuffix;
    char hostMax[16];
    int maxSuffix;
    int success;
} IPRange;

// struct for mapping spreadsheet title and data attribute
typedef struct {
    char *title;
    char *dataAttributeRef;
} spreadsheetColumnMapping;

/* schema for keeping track of which spreadsheet titles that match to witch data */
spreadsheetColumnMapping schema[] = {
    {"Device", "IP"},
    {"Name", "name"},
    {"Vendor", "vendor"},
    {"Model", "model"},
    {"S/N", "serNum"},
    {"SW Version", "swRev"},
    {"Ping", "ping"},
    {"MMS", "mms"}
};
const int NUM_COLUMNS = sizeof(schema) / sizeof(schema[0]); 

const wchar_t *SSIDs[] = {L"Stationsbuss", L"StationsBuss", L"lo", L"stationbuss", L"stationsBuss", L"STATIONSBUSS" }; // array of network names to look for 

/* Returns the column index for the given title for the spreadsheet, -1 if not found. */
int get_column_index(const char* targetTitle) {
    for (int i = 0; i < NUM_COLUMNS; i++) {
        // Compare the target title with the schema
        if (strcmp(schema[i].title, targetTitle) == 0) {
            return i; 
        }
    }
    return -1; // didnt find the title
}

void initWorksheet(lxw_worksheet *worksheet){
    for(size_t i = 0; i < NUM_COLUMNS; i++){
        worksheet_write_string(worksheet, 0, i, schema[i].title, NULL);
    }
}

/* Tries to find logical device 'LD0', if not found it return the first logical device. returns NULL if no devices found. */
const char *get_LD_name(IedConnection con, IedClientError *err){
    LinkedList LDList = IedConnection_getServerDirectory(con, err, false);  // fetch all Logical devices
    if (LDList != NULL){
        if(!LinkedList_contains(LDList, "LD0")){   
            LinkedList firstLD = LinkedList_getNext(LDList); // get first logical device
            if(firstLD != NULL){ return firstLD->data; }  // return first element in the list if LDO isn't in the list
        }else{
            printf("Found device LD0\n");
            return "LD0";
        }
    }
    return NULL;
}

/* Tries to find the network with the given friendlyName */
NetworkInfo getInfoFromNetwork(const wchar_t *friendlyName){
    NetworkInfo info = {0}; // initialize "empty" struct

    // Setup variables for GetAdaptersAddresses
    ULONG family = AF_INET; // IPv4
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX; 
    ULONG outBufLen = 0;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;

    // Get the required buffer size
    if (GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        if (pAddresses == NULL) {
            printf("Memory allocation failed.\n");
            WSACleanup();
            info.success = 0;
            return info;
        }
    }

    // Retrieve linked list of adapters
    if (GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen) == NO_ERROR) {
        // Iterate through the linked list of adapters
        PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
        while (pCurrAddresses != NULL) {

            PWCHAR netName = pCurrAddresses->FriendlyName;  // save current network name
            if(wcsncmp(netName, friendlyName, sizeof(friendlyName)) == 0){  // check current network name against wanted network
                wcstombs(info.hostname, pCurrAddresses->FriendlyName, sizeof(info.hostname)); // assign and convert friendlyName to multibyte chars

                // Fetch IP address and subnet mask (in CIDR format) from found network
                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                if (pUnicast != NULL) {
                    char ipString[INET_ADDRSTRLEN];
                    struct sockaddr_in *sa_in = (struct sockaddr_in *)pUnicast->Address.lpSockaddr;
                    
                    // Convert the numeric IP to a readable string
                    inet_ntop(AF_INET, &(sa_in->sin_addr), ipString, sizeof(ipString));
                    
                    printf("IP Address: %s /%d\n", ipString, pUnicast->OnLinkPrefixLength);
                    strncpy(info.ip, ipString, sizeof(info.ip));
                    info.subnet_mask = pUnicast->OnLinkPrefixLength;
                    info.success = 1;
                    break;
                }
            }
            pCurrAddresses = pCurrAddresses->Next;  // continue stepping through the linked list
        }
    } else {
        printf("Call to GetAdaptersAddresses failed.\n");
        info.success = 0;
    }

    // Cleanup
    if (pAddresses) {
        free(pAddresses);
    }
    WSACleanup();
    return info;
}

/* calculates and return info about the available IP range of this subnet */
IPRange getIPRange(NetworkInfo *netInfo){
    IPRange range = {0};
    range.success = 0;  // init to fail

    if(netInfo->subnet_mask > 32){ return range; }   // subnet mask in CIDR notation to large for calculating IP range
    
    int usableIPAddresses = (int) pow(2, 32 - netInfo->subnet_mask) - 2; // calculate how many IP addresses that can be used

    char *lastDot = strrchr(netInfo->ip, '.'); // find the last dot in the ip-address
    char *lastHostIP = (lastDot == NULL) ? NULL : lastDot + 1;  // find the last characters in ip-address
    if(lastHostIP == NULL){ return range; }

    int lastHostToi = atoi(lastHostIP);     // convert to integer for use in subnet index calculation
    if(lastHostToi == 0){ return range; }   // conversion not valid

    unsigned subnetIndex = floor(lastHostToi / (usableIPAddresses + 2)); // calculate in which subnet our host is in
    range.minSuffix = 2 + subnetIndex * (usableIPAddresses + 2);        // calculate min host
    range.maxSuffix = range.minSuffix + usableIPAddresses - 1;    // calculate max host
    if(range.minSuffix > range.maxSuffix) { return range; }

    char IPPrefix[13]; // maximum IPv4 length of 12 chars (+ null terminating character)
    unsigned lengthOfIPPrefix = strlen(netInfo->ip) - strlen(lastHostIP);    // calculate how many characters in IP address before last characters (e.g. 192.168.122.XX)
    strncpy(range.IPPrefix, netInfo->ip, lengthOfIPPrefix);    // copy prefix (192.168.122.)
    range.IPPrefix[sizeof(range.IPPrefix) - 1] = '\0';

    int assignMinChars = snprintf(range.hostMin, sizeof(range.hostMin), "%s%d", range.IPPrefix, range.minSuffix);  // concatenate suffix and min host
    if (assignMinChars < 0){ return range; }    // snprintf couldn't complete 
    int assignMaxChars = snprintf(range.hostMax, sizeof(range.hostMax), "%s%d", range.IPPrefix, range.maxSuffix);  // concatenate suffix and max host
    if (assignMaxChars < 0){ return range; }    // snprintf couldn't complete 

    range.success = 1;
    return range;
}

/* Tries to find and return info about an available network out of those given by the list of SSIDs. */
NetworkInfo findMatchingNetwork(){
    NetworkInfo netInfo;
    for (size_t i = 0; i < sizeof(SSIDs) / sizeof(SSIDs[0]); i++)
    {
        netInfo = getInfoFromNetwork(SSIDs[i]);
        if(netInfo.success){
          printf("found a network '%s' with IP: %s, and netmask: %d\n", netInfo.hostname, netInfo.ip, netInfo.subnet_mask);
          printf("\n");
          break;  
        } 
    }
    return netInfo;
}

/* Tries to find the given global data attributes and write their data to the worksheet. */
void processDeviceData(IedConnection con, IedClientError *err, const char *deviceName, lxw_worksheet *worksheet, int row){
    // possible paths for accessing data attributes
    const char *topLevelPaths[] = {"LPHD0", "LPHD1", "LLN0"};
    const char *midLevelPaths[] = {"PhyNam", "NamPlt", "PhyHealth"};
    char daRef[50];     // create buffer for copy and modification of attribute path

    /* iterate through all possible paths or until some data is found for each data attribute */
    for (size_t top = 0; top < sizeof(topLevelPaths) / sizeof(topLevelPaths[0]); top++){
        for(size_t mid = 0; mid < sizeof(midLevelPaths) / sizeof(midLevelPaths[0]); mid++){
            for(size_t daIndex = 0; daIndex < NUM_COLUMNS; daIndex++){
                snprintf(daRef, sizeof(daRef), "%s/%s.%s.%s", deviceName, topLevelPaths[top], midLevelPaths[mid], schema[daIndex].dataAttributeRef);     //concatenate the different paths to a full path
                MmsValue *value = IedConnection_readObject(con, err, daRef, IEC61850_FC_DC);    // read object at path
                if(value != NULL){
                    const char *strVal = MmsValue_toString(value);
                    if(strVal != NULL && strVal[0] != '\0'){
                        worksheet_write_string(worksheet, row, daIndex, strVal, NULL);  // write to worksheet if the string isn't empty
                        break;
                    }
                }
                else{
                    printf("Error reading from object!\n REASON: %s\n", IedClientError_toString(*err));
                }
                MmsValue_delete(value);
            }
        }
    }
}

/* Checks if we can connect to the given IP via IEC61850 MMS, and writes the result to the worksheet. */
bool canConnectToIED(IedConnection connection, IedClientError *err, const char *deviceIP, int tcpPort, lxw_worksheet *worksheet, int row){
    IedConnection_connect(connection, err, deviceIP, tcpPort);     // connect to physical device using the connection above

    int MMSCol = get_column_index("MMS");
    if (*err == IED_ERROR_OK){
        worksheet_write_string(worksheet, row, MMSCol, "ja", NULL); // successfull connect
        return true;
    }
    worksheet_write_string(worksheet, row, MMSCol, "nej", NULL); // unsuccessfull connect
    return false; 
}

/* Checks if the device with the given IP address is pingable, and writes the result to the worksheet. */
void checkForPingAnswer(const char *ipAddress, lxw_worksheet *worksheet, int row){
    HANDLE hIcmpFile;
    unsigned long ipaddr = INADDR_NONE;
    DWORD dwRetVal = 0;
    char SendData[32] = "Data Buffer";
    LPVOID ReplyBuffer = NULL;
    DWORD ReplySize = 0;
    
    int PingCol = get_column_index("Ping"); // get the column where ping should be stored in spreadsheet
    worksheet_write_string(worksheet, row, PingCol, "nej", NULL);   // initialize to unsuccessfull ping in spreadsheet   
    ipaddr = inet_addr(ipAddress);  // convert ip to be able to send with ICMP
    if (ipaddr == INADDR_NONE) { return; }

    
    hIcmpFile = IcmpCreateFile();   // open handler for ICMP echo request
    if (hIcmpFile == INVALID_HANDLE_VALUE) { return; } 

    // calculate size and allocate reply buffer
    ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData);
    ReplyBuffer = (VOID*) malloc(ReplySize);    
    if (ReplyBuffer == NULL) { 
        IcmpCloseHandle(hIcmpFile);
        return; 
    }   
    
    dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof(SendData), 
        NULL, ReplyBuffer, ReplySize, 1000);    // send echo request (ping)

    if (dwRetVal != 0) {
        PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
        if (pEchoReply->Status == 0) {
            worksheet_write_string(worksheet, row, PingCol, "ja", NULL);  // ping sent successfully, update spreadsheet
        }
    }else{
        printf("last error: %d\n", GetLastError());
    }

    IcmpCloseHandle(hIcmpFile);
    free(ReplyBuffer);
}


int main(){
    int exitCode = 0;

    WSADATA wsaData;    // struct for holding information about networking and sockets
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        exitCode = -1;
        goto exit_wo_cleanup;
    }

    // try to find station network
    NetworkInfo netInfo = findMatchingNetwork(); 

    if(!netInfo.success){
        printf("coudln't find any suitable network!\n");
        exitCode = -1;
        goto exit_wo_cleanup;
    }

    IPRange ipRange = getIPRange(&netInfo); // calculate ranges of IP addresses
    if(ipRange.success == 0){
        printf("Couldn't calculate IP range!");
        exitCode = -1;
        goto exit_wo_cleanup;
    }
    printf("IP in range %s - %s\n", ipRange.hostMin, ipRange.hostMax);

    const char workbookName[] = "stationsinformation.xlsx";
    lxw_workbook  *workbook  = workbook_new(workbookName);    // create a new workbook
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, NULL);  // add a sheet to the newly created workbook
    initWorksheet(worksheet);   // initialize worksheet with titles
    IedConnection connection = IedConnection_create();  // create a connection
    IedClientError err;     // error reason

    int tcpPort = 102;  // TCP port of device (IEC 16850 has port 102 as standard)
    char concatenatedIP[16]; 
    int IPCol = get_column_index("Device");
    for(size_t min = ipRange.minSuffix; (min <= ipRange.maxSuffix && min <= 254); min++){
        int row = min - ipRange.minSuffix + 1;
        snprintf(concatenatedIP, sizeof(concatenatedIP), "%s%d", ipRange.IPPrefix, min);    // concatenate prefix and suffix to create whole IP
        worksheet_write_string(worksheet, row, IPCol, concatenatedIP, NULL);  // write device IP address to spreadsheet

        checkForPingAnswer(concatenatedIP, worksheet, row);   // check if device answers to ping

        bool canConnect = canConnectToIED(connection, &err, concatenatedIP, tcpPort, worksheet, row);    // connect to IED
        if(canConnect){
            const char *deviceName = get_LD_name(connection, &err);  // fetch name of logical device
            processDeviceData(connection, &err, deviceName, worksheet, row);   // read device data and write it to the worksheet
        }
    } 

cleanup:
    IedConnection_destroy(connection);
    workbook_close(workbook);   // close and save workbook
    printf("\n");
    printf("Wrote results to %s\n", workbookName);
    printf("Disconnected from IED!\n");
    return exitCode;

exit_wo_cleanup:
    printf("\n");
    printf("Exiting!");
    return exitCode;
}   