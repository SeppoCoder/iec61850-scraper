#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    // 1. Initialize Winsock (Required before networking functions in Windows)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return 1;
    }

    // 2. Setup variables for GetAdaptersAddresses
    ULONG family = AF_INET; // Let's stick to IPv4 for now
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX; 
    ULONG outBufLen = 0;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;

    // 3. First call to get the required buffer size
    if (GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        if (pAddresses == NULL) {
            printf("Memory allocation failed.\n");
            WSACleanup();
            return 1;
        }
    }

    // 4. Second call to actually retrieve the data
    if (GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen) == NO_ERROR) {
        
        // 5. Iterate through the linked list of adapters
        PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
        wchar_t *prefixToSearch = L"Stationsbuss";

        while (pCurrAddresses != NULL) {
            PWCHAR netName = pCurrAddresses->FriendlyName;
            if(wcsncmp(netName, prefixToSearch, sizeof(prefixToSearch)) == 0){
                printf("found network!\n");
                wprintf(L"%ls \n", pCurrAddresses->FriendlyName);   

                // 6. Iterate through the unicast IP addresses for this adapter
                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                if (pUnicast != NULL) {
                    char ipString[INET_ADDRSTRLEN];
                    struct sockaddr_in *sa_in = (struct sockaddr_in *)pUnicast->Address.lpSockaddr;
                    
                    // Convert the numeric IP to a readable string
                    inet_ntop(AF_INET, &(sa_in->sin_addr), ipString, sizeof(ipString));
                    
                    // Windows gives you the subnet mask as a CIDR prefix (e.g., 24 instead of 255.255.255.0)
                    printf("  IP Address: %s /%d\n", ipString, pUnicast->OnLinkPrefixLength);
                }
                printf("\n");
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    } else {
        printf("Call to GetAdaptersAddresses failed.\n");
    }

    // 7. Cleanup
    if (pAddresses) {
        free(pAddresses);
    }
    WSACleanup();

    return 0;
}