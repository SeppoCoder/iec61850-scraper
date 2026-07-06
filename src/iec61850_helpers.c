#include <stdio.h>
#include <stdlib.h>

#include "iec61850_helpers.h"

bool canConnectToIED(IedConnection connection, IedClientError *err, const char *deviceIP, int tcpPort, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr, int MMSCol){
    fprintf(fptr, "Connecting to IED...\n");
    IedConnection_connect(connection, err, deviceIP, tcpPort);     // connect to physical device using the connection above

    if (*err == IED_ERROR_OK){
        worksheetError = worksheet_write_string(worksheet, row, MMSCol, "ja", NULL); // successfull connect
        if (worksheetError != LXW_NO_ERROR){
            fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
        }
        fprintf(fptr, "Connected to device!\n");
        return true;
    }
    worksheetError = worksheet_write_string(worksheet, row, MMSCol, "nej", NULL); // unsuccessfull connect
    if (worksheetError != LXW_NO_ERROR){
        fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
    }
    fprintf(fptr, "Couldn't connect to device!\n");
    fprintf(fptr, "Error: %s\n", IedClientError_toString(*err));
    return false; 
}


void processDeviceData(IedConnection con, IedClientError *err, const char *deviceName, lxw_worksheet *worksheet, int row, int NUM_COLUMNS, spreadsheetColumnMapping schema[], lxw_error worksheetError, FILE *fptr){
    // possible paths for accessing data attributes
    const char *topLevelPaths[] = {"LPHD0", "LPHD1", "LLN0"};
    const char *midLevelPaths[] = {"PhyNam", "NamPlt", "PhyHealth"};
    char daRef[50];     // create buffer for copy and modification of attribute path

    /* iterate through all possible paths or until some data is found for each data attribute */
    for (size_t top = 0; top < sizeof(topLevelPaths) / sizeof(topLevelPaths[0]); top++){
        for(size_t mid = 0; mid < sizeof(midLevelPaths) / sizeof(midLevelPaths[0]); mid++){
            for(size_t daIndex = 0; daIndex < NUM_COLUMNS; daIndex++){
                snprintf(daRef, sizeof(daRef), "%s/%s.%s.%s", deviceName, topLevelPaths[top], midLevelPaths[mid], schema[daIndex].dataAttributeRef);     //concatenate the different paths to a full path
                fprintf(fptr, "reading from path: %s\n", daRef);
                MmsValue *value = IedConnection_readObject(con, err, daRef, IEC61850_FC_DC);    // read object at path
                if(value != NULL){
                    const char *strVal = MmsValue_toString(value);
                    if(strVal != NULL && strVal[0] != '\0'){
                        worksheetError = worksheet_write_string(worksheet, row, daIndex, strVal, NULL);  // write to worksheet if the string isn't empty
                        if (worksheetError != LXW_NO_ERROR){
                            fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
                        }
                        fprintf(fptr, "Successfull read from path: %s\n", daRef);
                        break;
                    }
                }
                else{
                    fprintf(fptr, "Error reading from IED!\n REASON: %s.\n", IedClientError_toString(*err));
                }
                MmsValue_delete(value);
            }
        }
    }
}


const char *get_LD_name(IedConnection con, IedClientError *err, FILE *fptr){
    fprintf(fptr, "Finding logical device...\n");
    LinkedList LDList = IedConnection_getServerDirectory(con, err, false);  // fetch all Logical devices
    if (LDList != NULL){
        if(!LinkedList_contains(LDList, "LD0")){   
            fprintf(fptr, "Couldn't find logical device 'LD0'. Defaulting to first logical device found.\n");
            LinkedList firstLD = LinkedList_getNext(LDList); // get first logical device
            if(firstLD != NULL){ 
                return firstLD->data; // return first element in the list if LDO isn't in the list 
            }
            fprintf(fptr, "Couldn't find any logical device.\n");  
        }else{
            printf("Found device LD0\n");
            return "LD0";
        }
    }
    return NULL;
} 