#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iec61850_helpers.h"

bool canConnectToIED(IedConnection connection, IedClientError *err, const char *deviceIP, int tcpPort, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr, int MMSCol){
    fprintf(fptr, "Connecting to IED...\n");
    IedConnection_connect(connection, err, deviceIP, tcpPort);     // connect to physical device using the connection above

    if (*err == IED_ERROR_OK){
        worksheetError = worksheet_write_string(worksheet, row, MMSCol, "ja", NULL); // successfull connect
        if (worksheetError != LXW_NO_ERROR){
            fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
        }
        fprintf(fptr, "Connected to device!\n\n");
        return true;
    }
    worksheetError = worksheet_write_string(worksheet, row, MMSCol, "nej", NULL); // unsuccessfull connect
    if (worksheetError != LXW_NO_ERROR){
        fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
    }
    fprintf(fptr, "Couldn't connect to device!\n");
    fprintf(fptr, "Error: %s\n\n", IedClientError_toString(*err));
    return false; 
}


void processDeviceData(IedConnection con, IedClientError *err, const char *deviceName, const char *nodeName, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr){
    const char *dataAttributes[] = {"name", "vendor", "model", "serNum", "swRev"};
    char daRef[50];     // create buffer for copy and modification of attribute path

    /* iterate through all data attributes*/
    for(size_t daIndex = 0; daIndex < sizeof(dataAttributes) / sizeof(dataAttributes[0]); daIndex++){
        snprintf(daRef, sizeof(daRef), "%s/%s.%s.%s", deviceName, nodeName, "PhyNam", dataAttributes[daIndex]);     //concatenate the different paths to a full path
        fprintf(fptr, "reading from path: %s\n", daRef);
        MmsValue *value = IedConnection_readObject(con, err, daRef, IEC61850_FC_DC);    // read object at path
        if(value != NULL){
            const char *strVal = MmsValue_toString(value);
            if(strVal != NULL && strVal[0] != '\0'){
                worksheetError = worksheet_write_string(worksheet, row, daIndex + 1, strVal, NULL);  // write to worksheet if the string isn't empty
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


const char *get_LD_name(IedConnection con, IedClientError *err, FILE *fptr){
    fprintf(fptr, "Finding logical device...\n");
    LinkedList LDList = IedConnection_getServerDirectory(con, err, false);  // fetch all Logical devices

    LinkedList currentDevice = LinkedList_getNext(LDList);  
    while (currentDevice != NULL){
        if(strstr(currentDevice->data, "LD0") != NULL){     // looking for device which includes substring "LD0"
            fprintf(fptr, "Found device %s\n\n", currentDevice->data);
            return currentDevice->data;
        }

        currentDevice = currentDevice->next;    // move down the linkedlist
    }

    LinkedList firstDevice = LinkedList_getNext(LDList);
    if (firstDevice != NULL) {
        fprintf(fptr, "Couldn't find logical device including 'LD0'. Defaulting to first logical device found (%s).\n\n", firstDevice->data);
        return firstDevice->data;
    }

    fprintf(fptr, "No logical device found!\n\n");
    return "";
} 


const char *findLogicalNode(LinkedList logicalNodes){
    char *LNsToSearch[] = {"LPHD0", "LPHD1", "LLN0"};

    LinkedList current = LinkedList_getNext(logicalNodes);

    for (size_t i = 0; i < sizeof(LNsToSearch) / sizeof(LNsToSearch[0]); i++){
        LinkedList temp = current;  // copy 

        while (temp != NULL){
            if (strcmp(LNsToSearch[i], temp->data) == 0) {
                return LNsToSearch[i]; 
            }   
            temp = temp->next;
        }
    }
    
    if(current != NULL){
        return current->data;
    }
    return "";
}
