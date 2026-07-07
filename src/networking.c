// x86_64-w64-mingw32-gcc src/networking.c -o src/net.exe -I/home/sebli/libiec61850/.install/include -I/home/sebli/libxlsxwriter/include -Iheaders/ -L/home/sebli/iec61850-scraper/libs -L. -lxlsxwriter -lz -liphlpapi -lws2_32 -lbcrypt


#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>

#include "networking.h"


int initNetwork(void){
    WSADATA wsaData;    // struct for holding information about networking and sockets
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return -1;
    }
    return 0;
}

NetworkInfo getInfoFromNetwork(const wchar_t *friendlyName, FILE *fptr){
    fwprintf(fptr, L"Fetching network info from '%ls'\n", friendlyName);
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
            fprintf(fptr, "Couldn't allocate memory for network network buffer.\n");
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
            if(_wcsicmp(netName, friendlyName) == 0){  // check current network name against wanted network
                wcstombs(info.hostname, pCurrAddresses->FriendlyName, sizeof(info.hostname)); // assign and convert friendlyName to multibyte chars

                // Fetch IP address and subnet mask (in CIDR format) from found network
                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                if (pUnicast != NULL) {
                    char ipString[INET_ADDRSTRLEN];
                    struct sockaddr_in *sa_in = (struct sockaddr_in *)pUnicast->Address.lpSockaddr;
                    
                    // Convert the numeric IP to a readable string
                    inet_ntop(AF_INET, &(sa_in->sin_addr), ipString, sizeof(ipString));
                    
                    strncpy(info.ip, ipString, sizeof(info.ip));
                    info.subnet_mask = pUnicast->OnLinkPrefixLength;
                    info.success = 1;
                }
            }
            pCurrAddresses = pCurrAddresses->Next;  // continue stepping through the linked list
        }
    } else {
        fprintf(fptr, "Call to GetAdaptersAddresses failed.\n");
        info.success = 0;
    }

    // Cleanup
    if (pAddresses) {
        free(pAddresses);
    }
    WSACleanup();
    return info;
}


IPRange getIPRange(NetworkInfo *netInfo, FILE *fptr){
    fprintf(fptr, "Calculating IP Range...\n");
    IPRange range = {0};
    range.success = 0;  // init to fail

    if(netInfo->subnet_mask > 32){ 
        fprintf(fptr, "Subnet is to large.\n");
        return range; 
    }   
    
    int usableIPAddresses = (int) pow(2, 32 - netInfo->subnet_mask) - 2; // calculate how many IP addresses that can be used

    char *lastDot = strrchr(netInfo->ip, '.'); // find the last dot in the ip-address
    char *lastHostIP = (lastDot == NULL) ? NULL : lastDot + 1;  // find the last characters in ip-address
    if(lastHostIP == NULL){ 
        fprintf(fptr, "Couldn't parse IP address.\n");
        return range; 
    }

    int lastHostToi = atoi(lastHostIP);     // convert to integer for use in subnet index calculation
    if(lastHostToi == 0){ 
        fprintf(fptr, "Couldn't convert IP address back to integer, IP is probaly malformed in some way.\n");  
        return range; 
    } 

    unsigned subnetIndex = floor(lastHostToi / (usableIPAddresses + 2)); // calculate in which subnet our host is in
    range.minSuffix = 2 + subnetIndex * (usableIPAddresses + 2);        // calculate min host
    range.maxSuffix = range.minSuffix + usableIPAddresses - 1;    // calculate max host
    if(range.minSuffix > range.maxSuffix) { 
        fprintf(fptr, "Maximum host in IP address is smaller than minimum host, subnet mask is probably not valid.\n");
        return range;
    }

    char IPPrefix[13]; // maximum IPv4 length of 12 chars (+ null terminating character)
    unsigned lengthOfIPPrefix = strlen(netInfo->ip) - strlen(lastHostIP);    // calculate how many characters in IP address before last characters (e.g. 192.168.122.XX)
    strncpy(range.IPPrefix, netInfo->ip, lengthOfIPPrefix);    // copy prefix (192.168.122.)
    range.IPPrefix[sizeof(range.IPPrefix) - 1] = '\0';

    int assignMinChars = snprintf(range.hostMin, sizeof(range.hostMin), "%s%d", range.IPPrefix, range.minSuffix);  // concatenate suffix and min host
    if (assignMinChars < 0){ 
        fprintf(fptr, "Couldn't concatenate IP address, IP address is probably malformed.\n");
        return range; 
    }   
    int assignMaxChars = snprintf(range.hostMax, sizeof(range.hostMax), "%s%d", range.IPPrefix, range.maxSuffix);  // concatenate suffix and max host
    if (assignMaxChars < 0){ 
        fprintf(fptr, "Couldn't concatenate IP address, IP address is probably malformed.\n");
        return range; 
    }

    range.success = 1;
    return range;
}


NetworkInfo findMatchingNetwork(FILE *fptr){
    fprintf(fptr, "Finding available network...\n");
    NetworkInfo netInfo;

    netInfo = getInfoFromNetwork(L"stationsbuss", fptr);
    if(netInfo.success){
        fprintf(fptr, "found network '%s' with IP: %s, and netmask: %d\n\n", netInfo.hostname, netInfo.ip, netInfo.subnet_mask);
    } 

    fprintf(fptr, "\n");
    return netInfo;
}  


bool checkForPingAnswer(const char *ipAddress, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr, int pingCol){
    fprintf(fptr, "Pinging Device...\n");

    HANDLE hIcmpFile;
    unsigned long ipaddr = INADDR_NONE;
    DWORD dwRetVal = 0;
    char SendData[32] = "Data Buffer";
    LPVOID ReplyBuffer = NULL;
    DWORD ReplySize = 0;
    
    worksheetError = worksheet_write_string(worksheet, row, pingCol, "nej", NULL);   // initialize to unsuccessfull ping in spreadsheet   
    if (worksheetError != LXW_NO_ERROR){
        fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
    }

    ipaddr = inet_addr(ipAddress);  // convert ip to be able to send with ICMP
    if (ipaddr == INADDR_NONE) { 
        fprintf(fptr, "Unable to convert IP address.\n\n");
        return false; 
    }

    
    hIcmpFile = IcmpCreateFile();   // open handler for ICMP echo request
    if (hIcmpFile == INVALID_HANDLE_VALUE) { 
        fprintf(fptr, "Couldn't create ICMP handler.\n\n");
        return false; 
    } 

    // calculate size and allocate reply buffer
    ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData);
    ReplyBuffer = (VOID*) malloc(ReplySize);    
    if (ReplyBuffer == NULL) { 
        fprintf(fptr, "Couldn't allocate memory for reply buffer.\n\n");
        IcmpCloseHandle(hIcmpFile);
        return false; 
    }   
    
    bool returnStatus = false;

    dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof(SendData), 
        NULL, ReplyBuffer, ReplySize, 1000);    // send echo request (ping)

    if (dwRetVal != 0) {
        PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
        if (pEchoReply->Status == 0) {
            returnStatus = true;
            fprintf(fptr, "Successfull ping\n");
            worksheetError = worksheet_write_string(worksheet, row, pingCol, "ja", NULL);  // ping sent successfully, update spreadsheet
            if (worksheetError != LXW_NO_ERROR){
                fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%c)\n", worksheetError, lxw_strerror(worksheetError));
            }
        }else{
            fprintf(fptr, "ICMP reply has unsuccessfull status code: %d\n", pEchoReply->Status);
        }
    }else{
        fprintf(fptr, "No echo recieved, error code: %d\n", GetLastError());
    }

    fprintf(fptr, "\n");
    IcmpCloseHandle(hIcmpFile);
    free(ReplyBuffer);
    
    return returnStatus;
}

int fetchAvailableNetworks(char *ptr[]){
    // Setup variables for GetAdaptersAddresses
    ULONG family = AF_INET; // IPv4
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX; 
    ULONG outBufLen = 0;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    int pointerIndex = 0;   // index for assigning pointer array values

    // Get the required buffer size
    if (GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        if (pAddresses == NULL) {
            WSACleanup();
            return -1;
        }
    }

    // Retrieve linked list of adapters
    if (GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen) == NO_ERROR) {
        // Iterate through the linked list of adapters
        PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
        while (pCurrAddresses != NULL) {

            PWCHAR netName = pCurrAddresses->FriendlyName;  // save current network name
            if (netName != NULL){
                char buf[50];   // buffer for holding name as string
                wcstombs(buf, netName, sizeof(buf));    // convert to string
                ptr[pointerIndex] = strdup(buf);    // add it to pointer array

                pointerIndex++; // update array index
            }
            pCurrAddresses = pCurrAddresses->Next;  // continue stepping through the linked list
        }
    }

    // Cleanup
    if (pAddresses) {
        free(pAddresses);
    }
    WSACleanup();
    return pointerIndex;
}