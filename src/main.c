/**
 * @file      main.c
 * @author    Sebastian Ling
 * @brief     Main file for automatic IEC61850 information scraper
 * @version   1.0
 * @date      2026-08-04
 * @compile   x86_64-w64-mingw32-gcc src/main.c src/iec61850_helpers.c src/xlsx_helpers.c src/networking.c src/gui.c \
    src/userpath_helpers.c src/jsmn.c -o IEC61850-Scraper.exe -I/home/sebli/libiec61850/.install/include \
    -I/home/sebli/libxlsxwriter/include -Iheaders/ -L/home/sebli/iec61850-scraper/libs -L. -liec61850 -lhal \
    -lxlsxwriter -lzs -liphlpapi -lws2_32 -lbcrypt -lraylib -lgdi32 -lwinmm -static -static-libgcc

 * @run       wine scraper.exe
 */


#include "usefulmacros.h"   // useful macros header file

#include "gui.h"        // gui header for including GUI functions
#include "networking.h"     // networking header including functions for networking 
#include "iec61850_helpers.h"   // helper functions for iec61850 library
#include "xlsx_helpers.h"   // helper functions for xlsx library
#include "userpath_helpers.h"   // helper functions for handling reading from config file 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>


int main(){
    int exitCode = 0;  // exit code for whole program

    FILE *fptr; // file pointer for handling reading/writing to log file
    fptr = fopen("stationslogg.log", "w"); // create log file

    char *fileContents; // pointer pointing at file contents of path config file
    int bytesRead = readFileToString("config.json", &fileContents);  // read whole of path config file
    if(bytesRead < 0){
        LOG(fptr, "Error while reading user json\n");
        goto exit_wo_cleanup;
    }

    jsmn_parser parser;
    jsmntok_t *tokens;
    int readTokens = initJsmn(&parser, &tokens, fileContents, bytesRead, fptr); // Initialize JSON parser
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

    NetworkInfo netInfo = findMatchingNetwork(fptr); // try to find station network 
    if(!netInfo.success){
        LOG(fptr, "Couldn't fetch info from predefined networks, asking user.\n");
        char *pointerToSSIDs[32] = { NULL };    // array to hold SSIDs
        int fetchedNetworks = fetchAvailableNetworks(pointerToSSIDs);   // fetch all available network SSIDs 
        if(fetchedNetworks < 0){
            LOG(fptr, "couldn't find any suitable network!\n\n");
            exitCode = -1;
            goto exit_wo_cleanup;
        }

        const char *ssid = showListBox(pointerToSSIDs, fetchedNetworks);    // fetch user given ssid
        if (!ssid){
            LOG(fptr, "No SSID chosen by user\n");
            exitCode = -1;
            goto exit_wo_cleanup;
        }
        wchar_t wideCharSSID[32];  // buffer for conversion to widechar* 
        mbstowcs(wideCharSSID, ssid, sizeof(wideCharSSID)); // convert to widechar*
        netInfo = getInfoFromNetwork(wideCharSSID, fptr);     // get network info with user chosen SSID
    }

    IPRange ipRange = getIPRange(&netInfo, fptr); // calculate ranges of IP addresses
    if(ipRange.success == 0){
        LOG(fptr, "Couldn't calculate IP range!\n\n");
        exitCode = -1;
        goto exit_wo_cleanup;
    }
    LOG(fptr, "IP is in range %s - %s\n\n", ipRange.hostMin, ipRange.hostMax);

    const char *predefinedSpreadsheetColumns[] = {"Device", "Ping", "MMS", "IED Name"}; // array containing the standard spreadsheet columns, which will always populate the spreadsheet 
    int numberOfColumns = ARRAY_SIZE(predefinedSpreadsheetColumns);

    const char workbookNameSuffix[] = "stationsinformation.xlsx";
    char workbookName[64];
    snprintf(workbookName, sizeof(workbookName), "%s %s", netInfo.computerName, workbookNameSuffix);    // concatenate hostname and suffix to create complete workbook name
    lxw_workbook  *workbook  = workbook_new(workbookName);    // create a new workbook
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, netInfo.computerName);  // add a sheet to the newly created workbook
    lxw_error worksheetError;   // error codes for spreadsheet
    initWorksheet(worksheet, worksheetError, fptr, predefinedSpreadsheetColumns, numberOfColumns);   // initialize worksheet with predefined titles
    IedConnection connection = IedConnection_create();  // create a connection for IEDs
    IedClientError err; // error reason
    int tcpPort = 102;  // The tcp port which IEC61850 uses to communicate 

    int writtenSpreadsheetColumns = numberOfColumns;    // copy the number of predefined spreadsheet columns.
    int count = 0;  // count for keeping track of how many different attributes (spreadsheet columns) we will found
    GeneralPath *generalPaths = parseGeneralPaths(tokens, fileContents, readTokens, fptr, &writtenSpreadsheetColumns, &count);  // parse general paths from user path config file
    if (!generalPaths){
        LOG(fptr, "Couldn't parse genereal paths!\n\n");
        exitCode = -1;
        goto exit_wo_cleanup;
    }
    writeWorksheetTitles(worksheet, worksheetError, fptr, generalPaths, count, numberOfColumns);    // write the rest of the titles found

    int IPCol = get_column_index("Device", predefinedSpreadsheetColumns, numberOfColumns);  // find which column in the spreadsheet that corresponds to the IP-addresses
    if (IPCol == -1){
        LOG(fptr, "Couldn't find spreadsheet title 'Device'.\n\n");
    }

    int nameCol = get_column_index("IED Name", predefinedSpreadsheetColumns, numberOfColumns);  // find which column in the spreadsheet that corresponds to the IED names
    if (nameCol == -1){
        LOG(fptr, "Couldn't find spreadsheet title 'IED Name'.\n\n");
    }

    // iterate through all ip addresses in the range
    for(size_t min = ipRange.minSuffix; (min <= ipRange.maxSuffix && min <= 254); min++){ 
        char concatenatedIP[16]; // buffer for holding IP address
        snprintf(concatenatedIP, sizeof(concatenatedIP), "%s%d", ipRange.IPPrefix, min);    // concatenate prefix and suffix to create whole IP
        LOG(fptr, "-------------------------------------\n");
        LOG(fptr, "Current device: %s\n", concatenatedIP);
        
        int row = min - ipRange.minSuffix + 1;  // calculate which row in the spreadsheet we are currently on
        worksheetError = worksheet_write_string(worksheet, row, IPCol, concatenatedIP, NULL);  // write device IP address to spreadsheet
        if(worksheetError != LXW_NO_ERROR){
            LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%s)\n", worksheetError, lxw_strerror(worksheetError));
        }

        int pingCol = get_column_index("Ping", predefinedSpreadsheetColumns, numberOfColumns);   // find which column in the spreadsheet that corresponds to ping
        if (pingCol == -1){
            LOG(fptr, "Couldn't find spreadsheet title 'Ping'.\n");
        }

        bool canPing = checkForPingAnswer(concatenatedIP, worksheet, row, worksheetError, fptr, pingCol);   // check if device answers to ping
        if (canPing){
            int MMSCol = get_column_index("MMS", predefinedSpreadsheetColumns, numberOfColumns);     // find which column in the spreadsheet that corresponds to MMS communication
            if (MMSCol == -1){
                LOG(fptr, "Couldn't find spreadsheet title 'MMS'.\n");
            }
            
            bool canConnect = connectToIED(connection, &err, concatenatedIP, tcpPort, worksheet, row, worksheetError, fptr, MMSCol);    // connect to IED
            if(canConnect){
                char iedName[128];
                char *deviceName = get_LD_name(connection, &err, fptr, iedName);  // fetch name of logical device and try to parse IED name from LD name
                if (iedName){
                    worksheetError = worksheet_write_string(worksheet, row, nameCol, iedName, NULL);  // write device IP address to spreadsheet
                    if(worksheetError != LXW_NO_ERROR){
                        LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%s)\n", worksheetError, lxw_strerror(worksheetError));
                    }
                }
                if (*deviceName != '\0'){
                    processDeviceData(connection, &err, deviceName, generalPaths, count, worksheet, row, worksheetError, fptr);   // read device data and write it to the worksheet 
                }
            }
        }

        LOG(fptr, "\n");
        IedConnection_close(connection);    // close IED connection after each try
    } 

    parseUserPaths(tokens, fileContents, readTokens, fptr, tcpPort, ipRange.minSuffix, worksheet, writtenSpreadsheetColumns, generalPaths, count);  // parse user specific paths

cleanup:
    IedConnection_destroy(connection); 
    LOG(fptr, "\nIedConnection destroyed!\n");

    worksheetError = workbook_close(workbook);   // close and save workbook
    if (worksheetError != LXW_NO_ERROR){
        LOG(fptr, "Couldn't close worksheet, returned with error code %d (%s)\n", worksheetError, lxw_strerror(worksheetError));
    }else{
        LOG(fptr, "Wrote results to %s\n", workbookName);
    }

    if (fileContents){
        free(fileContents);
    }
    if (generalPaths){
        free(generalPaths);
    }

    LOG(fptr, "Exiting with exit code %d\n", exitCode);
    return exitCode;

exit_wo_cleanup:
    LOG(fptr, "Exiting with exit code %d\n", exitCode);
    return exitCode;
}   