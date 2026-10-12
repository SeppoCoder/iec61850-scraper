// x86_64-w64-mingw32-gcc src/networking.c -o src/net.exe -I/home/sebli/libiec61850/.install/include -I/home/sebli/libxlsxwriter/include -Iheaders/ -L/home/sebli/iec61850-scraper/libs -L. -lxlsxwriter -lz -liphlpapi -lws2_32 -lbcrypt


#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

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
    wprintf(L"Fetching network info from '%ls'\n", friendlyName);
    NetworkInfo info = {0}; // initialize "empty" struct

    char hostnameBuf[128];  // Buffer for holding computer name
    int hostnameSuccess = gethostname(hostnameBuf, sizeof(hostnameBuf));
    if (hostnameSuccess != 0){
        LOG(fptr, "Couldn't retrieve computer name.\n");
        snprintf(hostnameBuf, sizeof(hostnameBuf), "%s", "Unknown");
    }
    snprintf(info.computerName, sizeof(info.computerName), "%s", hostnameBuf);

    // Setup variables for GetAdaptersAddresses
    ULONG family = AF_INET; // IPv4
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX; 
    ULONG outBufLen = 0;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;

    // Get the required buffer size
    if (GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        if (pAddresses == NULL) {
            LOG(fptr, "Couldn't allocate memory for network buffer.\n");
            WSACleanup();
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
                    
                    int bytesWritten = snprintf(info.ip, sizeof(info.ip), "%s", ipString);
                    if (bytesWritten >= sizeof(info.ip)) {
                        LOG(fptr, "IP is truncated!\n");
                        return info;
                    } 

                    info.subnet_mask = pUnicast->OnLinkPrefixLength;    // assign subnet mask 
                    info.success = 1;   // only update to success if we successfully find and copy the IP address
                }
            }
            pCurrAddresses = pCurrAddresses->Next;  // continue stepping through the linked list
        }
    } else {
        LOG(fptr, "Call to GetAdaptersAddresses failed.\n");
    }

    // Cleanup
    if (pAddresses) {
        free(pAddresses);
    }
    WSACleanup();
    return info;
}


IPRange getIPRange(NetworkInfo *netInfo, FILE *fptr){
    LOG(fptr, "Calculating IP Range...\n");
    IPRange range = {0};

    if(netInfo->subnet_mask > 32){ 
        LOG(fptr, "Subnet is to large.\n");
        return range; 
    }   
    
    int usableIPAddresses = (int) pow(2, 32 - netInfo->subnet_mask) - 2; // calculate how many IP addresses that can be used

    char *lastDot = strrchr(netInfo->ip, '.'); // find the last dot in the ip-address
    char *lastHostIP = (lastDot == NULL) ? NULL : lastDot + 1;  // find the last characters in ip-address
    if(lastHostIP == NULL){ 
        LOG(fptr, "Couldn't parse IP address.\n");
        return range; 
    }

    int lastHostToi = atoi(lastHostIP);     // convert to integer for use in subnet index calculation
    if(lastHostToi == 0){ 
        LOG(fptr, "Couldn't convert IP address back to integer, IP is probaly malformed in some way.\n");  
        return range; 
    } 

    unsigned subnetIndex = floor(lastHostToi / (usableIPAddresses + 2)); // calculate in which subnet our host is in
    range.minSuffix = 2 + subnetIndex * (usableIPAddresses + 2);        // calculate min host
    range.maxSuffix = range.minSuffix + usableIPAddresses - 2;    // calculate max host
    if(range.minSuffix > range.maxSuffix) { 
        LOG(fptr, "Maximum host in IP address is smaller than minimum host, subnet mask is probably not valid.\n");
        return range;
    }

    char IPPrefix[13]; // maximum IPv4 length of 12 chars (+ null terminating character)

    int lengthOfIPPrefix = strlen(netInfo->ip) - strlen(lastHostIP);    // calculate how many characters in IP address before last characters (e.g. 192.168.122.XX)
    if (lengthOfIPPrefix < 0 || lengthOfIPPrefix >= sizeof(range.IPPrefix)){
        LOG(fptr, "Calculation of IP prefix length went wrong!\n");
        return range;
    }

    int bytesWritten = snprintf(range.IPPrefix, sizeof(range.IPPrefix), "%.*s", lengthOfIPPrefix, netInfo->ip);
    if (bytesWritten < 0 || bytesWritten >= sizeof(range.IPPrefix)){
        LOG(fptr, "IP prefix is truncated, or formatting failed!\n");
        return range;
    }

    int assignMinChars = snprintf(range.hostMin, sizeof(range.hostMin), "%s%d", range.IPPrefix, range.minSuffix);  // concatenate suffix and min host
    if (assignMinChars < 0 || assignMinChars >= sizeof(range.hostMin)){ 
        LOG(fptr, "Couldn't concatenate min IP address, IP address is probably malformed.\n");
        return range; 
    }   
    
    int assignMaxChars = snprintf(range.hostMax, sizeof(range.hostMax), "%s%d", range.IPPrefix, range.maxSuffix);  // concatenate suffix and max host
    if (assignMaxChars < 0 || assignMaxChars >= sizeof(range.hostMax)){ 
        LOG(fptr, "Assigned chars: %d\n Max size: %d\n", assignMaxChars, sizeof(range.hostMax));
        LOG(fptr, "Couldn't concatenate max IP address (%s), IP address is probably malformed.\n", range.hostMax);
        return range; 
    }

    range.success = 1;
    return range;
}


NetworkInfo findMatchingNetwork(FILE *fptr){
    LOG(fptr, "Finding available network...\n");
    NetworkInfo netInfo;

    netInfo = getInfoFromNetwork(L"stationsbuss", fptr);    // try to find network
    if(netInfo.success){
        LOG(fptr, "found network '%s' with IP: %s, and netmask: %d\n\n", netInfo.hostname, netInfo.ip, netInfo.subnet_mask);
    } 

    LOG(fptr, "\n");
    return netInfo;
}  


bool checkForPingAnswer(const char *ipAddress, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr, int pingCol){
    LOG(fptr, "Pinging Device...\n");

    HANDLE hIcmpFile;
    unsigned long ipaddr = INADDR_NONE;
    DWORD dwRetVal = 0;
    char SendData[32] = "Data Buffer";
    LPVOID ReplyBuffer = NULL;
    DWORD ReplySize = 0;
    
    worksheetError = worksheet_write_string(worksheet, row, pingCol, "nej", NULL);   // initialize to unsuccessfull ping in spreadsheet   
    if (worksheetError != LXW_NO_ERROR){
        LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
    }

    ipaddr = inet_addr(ipAddress);  // convert ip to be able to send with ICMP
    if (ipaddr == INADDR_NONE) { 
        LOG(fptr, "Unable to convert IP address.\n\n");
        return false; 
    }

    
    hIcmpFile = IcmpCreateFile();   // open handler for ICMP echo request
    if (hIcmpFile == INVALID_HANDLE_VALUE) { 
        LOG(fptr, "Couldn't create ICMP handler.\n\n");
        return false; 
    } 

    // calculate size and allocate reply buffer
    ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData);
    ReplyBuffer = (VOID*) malloc(ReplySize);    
    if (ReplyBuffer == NULL) { 
        LOG(fptr, "Couldn't allocate memory for reply buffer.\n\n");
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
            LOG(fptr, "Successfull ping\n");
            worksheetError = worksheet_write_string(worksheet, row, pingCol, "ja", NULL);  // ping sent successfully, update spreadsheet
            if (worksheetError != LXW_NO_ERROR){
                LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%c)\n", worksheetError, lxw_strerror(worksheetError));
            }
        }else{
            LOG(fptr, "ICMP reply has unsuccessfull status code: %d\n", pEchoReply->Status);
        }
    }else{
        LOG(fptr, "No echo recieved, error code: %d\n", GetLastError());
    }

    LOG(fptr, "\n");
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