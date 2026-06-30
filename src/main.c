// gcc main.c -o main -liec61850 -lxlsxwriter

/* x86_64-w64-mingw32-gcc src/main.c -o src/main.exe -I/home/sebli/libiec61850/.install/include -I/home/sebli/libxlsxwriter/include -L/home/sebli/iec61850-scraper/libs -liec61850 -lxlsxwriter -lz -liphlpapi -lws2_32 -lbcrypt */

#if defined _WIN32 || defined _WIN64
    #include <winsock2.h>
    #include <iphlpapi.h>
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
    PWCHAR  hostname;
    char ip[16];
    UINT8 subnet_mask;
    int success; 
} NetworkInfo;

typedef struct {
    char hostMin[16];
    char hostMax[16];
    int success;
} IPRange;

/* returns the name of the first logical device found, NULL if not found. */
const char *get_LD_name(IedConnection con, IedClientError *err){
    LinkedList LDList = IedConnection_getServerDirectory(con, err, false);  // fetch all Logical devices
    if (LDList != NULL){
        LinkedList LD = LinkedList_getNext(LDList); // get first logical device
        if (LD != NULL){
            return LD->data;    // return the name (data) of the LD
        }
    }
    return NULL;
}

NetworkInfo getInfoFromNetwork(wchar_t *friendlyName){
    NetworkInfo info = {0}; // initialize "empty" struct

    // Setup variables for GetAdaptersAddresses
    ULONG family = AF_INET; // IPv4
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX; 
    ULONG outBufLen = 0;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;

    // 3. First call to get the required buffer size
    if (GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        if (pAddresses == NULL) {
            printf("Memory allocation failed.\n");
            WSACleanup();
            info.success = 0;
            return info;
        }
    }

    // 4. Second call to actually retrieve the data
    if (GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen) == NO_ERROR) {
        
        // 5. Iterate through the linked list of adapters
        PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
        while (pCurrAddresses != NULL) {
            PWCHAR netName = pCurrAddresses->FriendlyName;
            if(wcsncmp(netName, friendlyName, sizeof(friendlyName)) == 0){
                printf("found network!\n");
                wprintf(L"%ls \n", pCurrAddresses->FriendlyName);
                info.hostname = friendlyName;

                // 6. Iterate through the unicast IP addresses for this adapter
                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                if (pUnicast != NULL) {
                    char ipString[INET_ADDRSTRLEN];
                    struct sockaddr_in *sa_in = (struct sockaddr_in *)pUnicast->Address.lpSockaddr;
                    
                    // Convert the numeric IP to a readable string
                    inet_ntop(AF_INET, &(sa_in->sin_addr), ipString, sizeof(ipString));
                    
                    // Windows gives you the subnet mask as a CIDR prefix (e.g., 24 instead of 255.255.255.0)
                    printf("  IP Address: %s /%d\n", ipString, pUnicast->OnLinkPrefixLength);
                    strncpy(info.ip, ipString, sizeof(info.ip));
                    info.subnet_mask = pUnicast->OnLinkPrefixLength;
                    info.success = 1;
                }
                printf("\n");
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    } else {
        printf("Call to GetAdaptersAddresses failed.\n");
        info.success = 0;
    }

    // 7. Cleanup
    if (pAddresses) {
        free(pAddresses);
    }
    WSACleanup();
    return info;
}

IPRange getIPRange(NetworkInfo netInfo){
    IPRange range = {0};
    range.success = 0;

    if(netInfo.subnet_mask > 32){ return range; }
     
    size_t usableIPAddresses = (size_t) (pow(2, 32 - netInfo.subnet_mask) - 2); // calculate how many IP addresses that can be used

    char *lastDot = strrchr(netInfo.ip, '.'); // find the last dot in the ip-address
    char *lastHostIP = (lastDot == NULL) ? NULL : lastDot + 1;  // find the last characters in ip-address
    if(lastHostIP == NULL){ return range; }
    int lastHostToi = atoi(lastHostIP);

    unsigned subnetIndex = floor(lastHostToi / (usableIPAddresses + 2)); // calculate in which subnet our host is in
    printf("subnet index for (%s) is '%d'\n", netInfo.ip, subnetIndex);
    int hostMin = 1 + subnetIndex * (usableIPAddresses + 2);        // calculate min host
    int hostMax = 1 + subnetIndex * (usableIPAddresses + 2) + usableIPAddresses;    // calculate max host

    char IPSuffix[12];
    unsigned lengthOfIPSuffix = strlen(netInfo.ip) - strlen(lastHostIP);    // calculate how many characters in IP address before last characters (i.e. 192.168.0.XX)
    strncpy(IPSuffix, netInfo.ip, lengthOfIPSuffix);    // copy suffix (192.168.0.)

    int assignMinChars = snprintf(range.hostMin, sizeof(range.hostMin), "%s%d", IPSuffix, hostMin);  // concatenate suffix and min host
    if (assignMinChars < 0){ return range; }
    int assignMaxChars = snprintf(range.hostMax, sizeof(range.hostMax), "%s%d", IPSuffix, hostMax);  // concatenate suffix and max host
    if (assignMaxChars < 0){ return range; }

    range.success = 1;
    return range;
}


int main(){
    int tcpPort = 102;  // TCP port of device (IEC 16850 has port 102 as standard)
    wchar_t *SSIDs[] = {L"Stationsbuss", L"StationsBuss", L"virbr0", L"stationbuss", L"stationsBuss" }; // array of network names to look for 
    NetworkInfo netInfo; // struct for holding information about the local network

    WSADATA wsaData;    // struct for holding information about networking and sockets
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return 1;
    }

    // iterate through possible network SSIDs and try to get information about it
    for (size_t i = 0; i < sizeof(SSIDs) / sizeof(SSIDs[0]); i++)
    {
        netInfo = getInfoFromNetwork(SSIDs[i]);
        if(netInfo.success){
          printf("found a network with IP: %s, and netmask: %d\n", netInfo.ip, netInfo.subnet_mask);
          break;  
        } 
    }

    if(!netInfo.success){
        printf("coudln't find any suitable network!");
        return -1;
    }

    // possible paths for accessing data attributes
    const char *topLevelPaths[] = {"LPHD0", "LPHD1", "LLN0"};
    const char *midLevelPaths[] = {"PhyNam", "NamPlt", "PhyHealth"};
    const char *dataAttributePaths[] = {"model", "serNum", "vendor", "name", "swRev"};

    const char workbookName[] = "stationsinformation.xlsx";
    lxw_error xlsxError;
    lxw_workbook  *workbook  = workbook_new(workbookName);    // create a new workbook
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, NULL);  // add a sheet to the newly created workbook

    IedConnection connection = IedConnection_create();  // create a connection
    IedClientError err;     // error reason

    IPRange ipRange = getIPRange(netInfo);
    if(ipRange.success == 0){
        printf("Couldn't calculate IP range!");
        return -1;
    }
    printf("IP in range %s - %s\n", ipRange.hostMin, ipRange.hostMax);

    IedConnection_connect(connection, &err, netInfo.ip, tcpPort);     // connect to physical device using the connection above
    printf("Connecting to %s:%i\n", netInfo.ip, tcpPort);

    if (err != IED_ERROR_OK)
    {
        printf("couldn't connect to %s:%i\n", netInfo.ip, tcpPort);   
        IedConnection_destroy(connection);      // destroy (disconnect) 
        workbook_close(workbook);   // close and save workbook
        
        return -1;
    }

    printf("Connected!\n\n");

    worksheet_write_string(worksheet, 0, 0, "Device", NULL);    
    worksheet_write_string(worksheet, 1, 0, netInfo.ip, NULL);    // write IP-address to excel worksheet

    const char *deviceName = get_LD_name(connection, &err);   // fetch name of logical device
    if(deviceName == NULL){
        printf("couldn't find device name\n");   
        IedConnection_destroy(connection);      // destroy (disconnect) 
        workbook_close(workbook);   // close and save workbook
        return -1;
    }

    char daRef[50];     // create buffer for copy and modification of attribute path
    /* iterate through all possible paths from the given path lists */
    for(size_t daIndex = 0; daIndex < sizeof(dataAttributePaths) / sizeof(dataAttributePaths[0]); daIndex++){
        worksheet_write_string(worksheet, 0, daIndex+1, dataAttributePaths[daIndex], NULL);  // write the data attribute name to spreadsheet
        for (size_t top = 0; top < sizeof(topLevelPaths) / sizeof(topLevelPaths[0]); top++){
            for(size_t mid = 0; mid < sizeof(midLevelPaths) / sizeof(midLevelPaths[0]); mid++){

                snprintf(daRef, sizeof(daRef), "%s/%s.%s.%s", deviceName, topLevelPaths[top], midLevelPaths[mid], dataAttributePaths[daIndex]);     //concatenate the different paths to a full path
                MmsValue *value = IedConnection_readObject(connection, &err, daRef, IEC61850_FC_DC);    // read object at path

                if(value != NULL){
                    switch (MmsValue_getType(value)){    // Parse result based on data type

                        case MMS_VISIBLE_STRING:
                        case MMS_STRING: {
                            const char *strVal = MmsValue_toString(value);
                            xlsxError = worksheet_write_string(worksheet, 1, daIndex+1, strVal, NULL);  // write value to spreadsheet
                            printf("read string value '%s' at path '%s'\n", strVal, daRef);
                            if(xlsxError != LXW_NO_ERROR){
                                printf("failed to write value to workbook. \n REASON: %s\n", strerror(xlsxError));
                            }
                            break;
                        }

                        case MMS_INTEGER:
                        case MMS_UNSIGNED: {
                            int intVal = MmsValue_toInt32(value);
                            xlsxError = worksheet_write_number(worksheet, 1, daIndex+1, intVal, NULL);  // write value to spreadsheet
                            printf("read number value: %d\n", intVal);
                            if(xlsxError != LXW_NO_ERROR){
                                printf("failed to write value to workbook. \n REASON: %s\n", strerror(xlsxError));
                            }
                            break;
                        }

                        case MMS_BOOLEAN: {
                            bool boolVal = MmsValue_getBoolean(value);
                            xlsxError = worksheet_write_boolean(worksheet, 1, daIndex+1, boolVal, NULL);  // write value to spreadsheet
                            printf("read boolean value: %d\n", boolVal);
                            if(xlsxError != LXW_NO_ERROR){
                                printf("failed to write value to workbook. \n REASON: %s\n", strerror(xlsxError));
                            }
                            break;
                        }

                        default:
                            break;
                    }
                }
                else{
                    printf("Error reading from object!\n REASON: %s\n", IedClientError_toString(err));
                }
                MmsValue_delete(value);    // always free dynamically allocated memory when MmsValue has been created
            }
        }
    }
    
    workbook_close(workbook);   // close and save workbook
    IedConnection_destroy(connection);      // destroy connection (disconnect)

    printf("\n");
    printf("Wrote results to %s\n", workbookName);
    printf("Disconnected!\n");
}   