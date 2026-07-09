/*
    main.c   

    Author: Sebastian Ling
    Last modified: 2026-07-03

    ------------- Compiling and running the program -------------
    Compiling on linux with mingw32: x86_64-w64-mingw32-gcc src/main.c src/iec61850_helpers.c src/xlsx_helpers.c src/networking.c src/gui.c -o src/main.exe \
    -I/home/sebli/libiec61850/.install/include -I/home/sebli/libxlsxwriter/include -Iheaders/ -L/home/sebli/iec61850-scraper/libs -L. -liec61850 -lhal -lxlsxwriter \
    -lzs -liphlpapi -lws2_32 -lbcrypt -lraylib -lgdi32 -lwinmm -static -static-libgcc -mwindows

    Running on linux with wine: wine src/main.exe 
*/

#include "gui.h"        // gui header for including GUI functions
#include "networking.h"     // networking header including functions for networking 
#include "iec61850_helpers.h"   // helper functions for iec61850 library
#include "xlsx_helpers.h"   // helper functions for xlsx library

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>


int main(){
    int exitCode = 0;

    FILE *fptr; 
    fptr = fopen("stationslogg.log", "w"); // create log file

    int networkSuccess = initNetwork();
    if(networkSuccess != 0){
        fprintf(fptr, "WSAStartup failed.\n\n");
        exitCode = -1;
        goto exit_wo_cleanup;
    }    

    // try to find station network
    NetworkInfo netInfo = findMatchingNetwork(fptr); 
    if(!netInfo.success){
        fprintf(fptr, "Couldn't fetch info from predefined networks, asking user.\n");

        char *pointerToSSIDs[10] = { NULL };
        int fetchedNetworks = fetchAvailableNetworks(pointerToSSIDs);
        if(fetchedNetworks < 0){
            fprintf(fptr, "couldn't find any suitable network!\n\n");
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
        fprintf(fptr, "Couldn't calculate IP range!\n\n");
        exitCode = -1;
        goto exit_wo_cleanup;
    }
    fprintf(fptr, "IP in range %s - %s\n\n", ipRange.hostMin, ipRange.hostMax);

    /* schema for keeping track of which spreadsheet titles that match to which data */
    spreadsheetColumnMapping schema[] = {
        {"Device", "IP"},
        {"Ping", "ping"},
        {"MMS", "mms"},
        {"IED Name", "name"},
        {"Vendor", "vendor"},
        {"Model", "model"},
        {"S/N", "serNum"},
        {"SW Version", "swRev"},
        {"HW Version", "hwRev"},
        {"Config Revision", "configRev"},
        {"IEC61850 Version", "ldNs"}
    };
    const int NUM_COLUMNS = sizeof(schema) / sizeof(schema[0]); 

    const char workbookName[] = "stationsinformation.xlsx";
    lxw_workbook  *workbook  = workbook_new(workbookName);    // create a new workbook
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, NULL);  // add a sheet to the newly created workbook
    lxw_error worksheetError;
    initWorksheet(worksheet, worksheetError, fptr, schema, NUM_COLUMNS);   // initialize worksheet with titles
    IedConnection connection = IedConnection_create();  // create a connection
    IedClientError err;     // error reason



    int IPCol = get_column_index("Device", schema, NUM_COLUMNS);
    if (IPCol == -1){
        fprintf(fptr, "Couldn't find spreadsheet title 'Device'.\n\n");
    }

    int nameCol = get_column_index("IED Name", schema, NUM_COLUMNS);
    if (nameCol == -1){
        fprintf(fptr, "Couldn't find spreadsheet title 'IED Name'.\n\n");
    }

    int tcpPort = 102;  // TCP port of device (IEC 16850 has port 102 as standard)

    for(size_t min = ipRange.minSuffix; (min <= ipRange.maxSuffix && min <= 254); min++){   // iterate through all ip addresses in the range
        char concatenatedIP[16]; // buffer for holding IP address
        snprintf(concatenatedIP, sizeof(concatenatedIP), "%s%d", ipRange.IPPrefix, min);    // concatenate prefix and suffix to create whole IP
        fprintf(fptr, "-------------------------------------\n");
        fprintf(fptr, "Current device: %s\n", concatenatedIP);
        printf("-------------------------------------\n");
        printf("Current device: %s\n\n", concatenatedIP);
        
        int row = min - ipRange.minSuffix + 1;
        worksheetError = worksheet_write_string(worksheet, row, IPCol, concatenatedIP, NULL);  // write device IP address to spreadsheet
        if(worksheetError != LXW_NO_ERROR){
            fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%s)\n", worksheetError, lxw_strerror(worksheetError));
        }

        int pingCol = get_column_index("Ping", schema, NUM_COLUMNS);
        if (pingCol == -1){
            fprintf(fptr, "Couldn't find spreadsheet title 'Ping'.\n");
        }

        bool canPing = checkForPingAnswer(concatenatedIP, worksheet, row, worksheetError, fptr, pingCol);   // check if device answers to ping
        if (canPing){
            int MMSCol = get_column_index("MMS", schema, NUM_COLUMNS);
            if (MMSCol == -1){
                fprintf(fptr, "Couldn't find spreadsheet title 'MMS'.\n");
            }
            
            bool canConnect = canConnectToIED(connection, &err, concatenatedIP, tcpPort, worksheet, row, worksheetError, fptr, MMSCol);    // connect to IED
            if(canConnect){
                char iedName[64];
                char *deviceName = get_LD_name(connection, &err, fptr, iedName);  // fetch name of logical device
                if (iedName){
                    worksheetError = worksheet_write_string(worksheet, row, nameCol, iedName, NULL);  // write device IP address to spreadsheet
                    if(worksheetError != LXW_NO_ERROR){
                        fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%s)\n", worksheetError, lxw_strerror(worksheetError));
                    }
                }

                LinkedList logicalNodes = IedConnection_getLogicalDeviceDirectory(connection, &err, deviceName);    // fetch all logical nodes

                char *logicalNode = findLogicalNode(logicalNodes, "LPHD0");    // find correct logical node (LPHD0, LPHD1, LLN0)
                fprintf(fptr, "found logicalNode: %s\n\n", logicalNode);

                dataAttribute DAs1[] = {
                    {"vendor", "PhyNam", 4},
                    {"model", "PhyNam", 5},
                    {"serNum", "PhyNam", 6},
                    {"swRev", "PhyNam", 7},
                    {"hwRev", "PhyNam", 8}
                };
                int DASize = sizeof(DAs1) / sizeof(DAs1[0]);
                processDeviceData(connection, &err, deviceName, logicalNode, DAs1, DASize, worksheet, row, worksheetError ,fptr);   // read device data and write it to the worksheet

                logicalNode = findLogicalNode(logicalNodes, "LLN0");    // find correct logical node (LPHD0, LPHD1, LLN0)
                fprintf(fptr, "found logicalNode: %s\n\n", logicalNode);

                dataAttribute DAs2[] = {
                    {"configRev", "NamPlt", 9},
                    {"ldNs", "NamPlt", 10}
                };
                DASize = sizeof(DAs2) / sizeof(DAs2[0]);
                processDeviceData(connection, &err, deviceName, logicalNode, DAs2, DASize, worksheet, row, worksheetError ,fptr);   // read device data and write it to the worksheet
            }
        }

        fprintf(fptr, "\n");
        IedConnection_close(connection);
    } 

cleanup:
    IedConnection_destroy(connection);

    worksheetError = workbook_close(workbook);   // close and save workbook
    if (worksheetError != LXW_NO_ERROR){
        fprintf(fptr, "Couldn't close worksheet, returned with error code %d (%s)\n", worksheetError, lxw_strerror(worksheetError));
    }else{
        fprintf(fptr, "Wrote results to %s\n", workbookName);
    }

    fprintf(fptr, "IedConnection destroyed!\n");
    fprintf(fptr, "Exiting with exit code %d\n", exitCode);
    return exitCode;

exit_wo_cleanup:
    fprintf(fptr, "Exiting with exit code %d\n", exitCode);
    return exitCode;
}   