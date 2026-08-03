/*
    main.c   

    Author: Sebastian Ling
    Last modified: 2026-07-03

    ------------- Compiling and running the program -------------
    Compiling on linux with mingw32: x86_64-w64-mingw32-gcc src/main.c src/iec61850_helpers.c src/xlsx_helpers.c src/networking.c src/gui.c src/userpath_helpers.c src/jsmn.c -o src/scraper8.exe \    
    -I/home/sebli/libiec61850/.install/include -I/home/sebli/libxlsxwriter/include -Iheaders/ -L/home/sebli/iec61850-scraper/libs -L. -liec61850 -lhal -lxlsxwriter \    
    -lzs -liphlpapi -lws2_32 -lbcrypt -lraylib -lgdi32 -lwinmm -static -static-libgcc

    Running on linux with wine: wine src/main.exe 
*/

#include "usefulmacros.h"

#include "gui.h"        // gui header for including GUI functions
#include "networking.h"     // networking header including functions for networking 
#include "iec61850_helpers.h"   // helper functions for iec61850 library
#include "xlsx_helpers.h"   // helper functions for xlsx library
#include "userpath_helpers.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>


int main(){
    int exitCode = 0;

    FILE *fptr; 
    fptr = fopen("stationslogg.log", "w"); // create log file


    char *fileContents; // dynamically allocated inside readFileToString
    int bytesRead = readFileToString("userPaths.json", &fileContents);
    if(bytesRead < 0){
        LOG(fptr, "Error while reading user json\n");
        goto exit_wo_cleanup;
    }

    jsmn_parser parser;
    jsmntok_t *tokens;
    int readTokens = initJsmn(&parser, &tokens, fileContents, bytesRead, fptr);
    if (readTokens < 0){
        LOG(fptr, "Error while initializing JSON.\n");
        goto exit_wo_cleanup;
    }

    int networkSuccess = initNetwork();
    if(networkSuccess != 0){
        LOG(fptr, "WSAStartup failed.\n\n");
        exitCode = -1;
        goto exit_wo_cleanup;
    }

    // try to find station network
    NetworkInfo netInfo = findMatchingNetwork(fptr); 
    if(!netInfo.success){
        LOG(fptr, "Couldn't fetch info from predefined networks, asking user.\n");

        char *pointerToSSIDs[10] = { NULL };
        int fetchedNetworks = fetchAvailableNetworks(pointerToSSIDs);
        if(fetchedNetworks < 0){
            LOG(fptr, "couldn't find any suitable network!\n\n");
            exitCode = -1;
            goto exit_wo_cleanup;
        }

        const char *ssid = showListBox(pointerToSSIDs, fetchedNetworks);    // fetch user given ssid
        wchar_t wideCharSSID[32];  // buffer for conversion to widechar* 
        mbstowcs(wideCharSSID, ssid, sizeof(wideCharSSID)); // convert to widechar*
        netInfo = getInfoFromNetwork(wideCharSSID, fptr);     // get network info
    }

    IPRange ipRange = getIPRange(&netInfo, fptr); // calculate ranges of IP addresses
    if(ipRange.success == 0){
        LOG(fptr, "Couldn't calculate IP range!\n\n");
        exitCode = -1;
        goto exit_wo_cleanup;
    }
    LOG(fptr, "IP in range %s - %s\n\n", ipRange.hostMin, ipRange.hostMax);

    const char *predefinedSpreadsheetColumns[] = {"Device", "Ping", "MMS", "IED Name"};
    int numberOfColumns = ARRAY_SIZE(predefinedSpreadsheetColumns);

    const char workbookName[] = "stationsinformation.xlsx";
    lxw_workbook  *workbook  = workbook_new(workbookName);    // create a new workbook
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, NULL);  // add a sheet to the newly created workbook
    lxw_error worksheetError;
    initWorksheet(worksheet, worksheetError, fptr, predefinedSpreadsheetColumns, numberOfColumns);   // initialize worksheet with titles
    IedConnection connection = IedConnection_create();  // create a connection
    IedClientError err;     // error reason
    int tcpPort = 102;

    int writtenSpreadsheetColumns = numberOfColumns;
    int count = 0;
    GeneralPath *generalPaths = parseGeneralPaths(tokens, fileContents, readTokens, fptr, &writtenSpreadsheetColumns, &count);
    if (!generalPaths){
        LOG(fptr, "Couldn't parse genereal paths!\n\n");
        exitCode = -1;
        goto exit_wo_cleanup;
    }
    writeWorksheetTitles(worksheet, worksheetError, fptr, generalPaths, count, numberOfColumns);

    int IPCol = get_column_index("Device", predefinedSpreadsheetColumns, numberOfColumns);
    if (IPCol == -1){
        LOG(fptr, "Couldn't find spreadsheet title 'Device'.\n\n");
    }

    int nameCol = get_column_index("IED Name", predefinedSpreadsheetColumns, numberOfColumns);
    if (nameCol == -1){
        LOG(fptr, "Couldn't find spreadsheet title 'IED Name'.\n\n");
    }

    for(size_t min = ipRange.minSuffix; (min <= ipRange.maxSuffix && min <= 254); min++){   // iterate through all ip addresses in the range
        char concatenatedIP[16]; // buffer for holding IP address
        snprintf(concatenatedIP, sizeof(concatenatedIP), "%s%d", ipRange.IPPrefix, min);    // concatenate prefix and suffix to create whole IP
        LOG(fptr, "-------------------------------------\n");
        LOG(fptr, "Current device: %s\n", concatenatedIP);
        
        int row = min - ipRange.minSuffix + 1;
        worksheetError = worksheet_write_string(worksheet, row, IPCol, concatenatedIP, NULL);  // write device IP address to spreadsheet
        if(worksheetError != LXW_NO_ERROR){
            LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%s)\n", worksheetError, lxw_strerror(worksheetError));
        }

        int pingCol = get_column_index("Ping", predefinedSpreadsheetColumns, numberOfColumns);
        if (pingCol == -1){
            LOG(fptr, "Couldn't find spreadsheet title 'Ping'.\n");
        }

        bool canPing = checkForPingAnswer(concatenatedIP, worksheet, row, worksheetError, fptr, pingCol);   // check if device answers to ping
        if (canPing){
            int MMSCol = get_column_index("MMS", predefinedSpreadsheetColumns, numberOfColumns);
            if (MMSCol == -1){
                LOG(fptr, "Couldn't find spreadsheet title 'MMS'.\n");
            }
            
            bool canConnect = canConnectToIED(connection, &err, concatenatedIP, tcpPort, worksheet, row, worksheetError, fptr, MMSCol);    // connect to IED
            if(canConnect){
                char iedName[128];
                char *deviceName = get_LD_name(connection, &err, fptr, iedName);  // fetch name of logical device
                if (iedName){
                    worksheetError = worksheet_write_string(worksheet, row, nameCol, iedName, NULL);  // write device IP address to spreadsheet
                    if(worksheetError != LXW_NO_ERROR){
                        LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%s)\n", worksheetError, lxw_strerror(worksheetError));
                    }
                }

                processDeviceData(connection, &err, deviceName, generalPaths, count, worksheet, row, worksheetError, fptr);   // read device data and write it to the worksheet
            }
        }

        LOG(fptr, "\n");
        IedConnection_close(connection);
    } 

    parseUserPaths(tokens, fileContents, readTokens, fptr, tcpPort, ipRange.minSuffix, worksheet, writtenSpreadsheetColumns, generalPaths, count);

cleanup:
    IedConnection_destroy(connection);

    worksheetError = workbook_close(workbook);   // close and save workbook
    if (worksheetError != LXW_NO_ERROR){
        LOG(fptr, "Couldn't close worksheet, returned with error code %d (%s)\n", worksheetError, lxw_strerror(worksheetError));
    }else{
        LOG(fptr, "Wrote results to %s\n", workbookName);
    }

    LOG(fptr, "IedConnection destroyed!\n");
    LOG(fptr, "Exiting with exit code %d\n", exitCode);
    return exitCode;

exit_wo_cleanup:
    LOG(fptr, "Exiting with exit code %d\n", exitCode);
    return exitCode;
}   