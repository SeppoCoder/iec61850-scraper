#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iec61850_helpers.h"

bool canConnectToIED(IedConnection connection, IedClientError *err, const char *deviceIP, int tcpPort, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr, int MMSCol){
    fprintf(fptr, "Connecting to IED...\n");
    IedConnection_connect(connection, err, deviceIP, tcpPort);     // connect to physical device using the connection above

    if (*err == IED_ERROR_OK){
        IedConnection_getDeviceModelFromServer(connection, err);   // update device model buffer
        if (*err != IED_ERROR_OK){
            fprintf(fptr, "Couldn't buffer device data!\n");
            printf("Couldn't buffer device data!\n");
        }

        worksheetError = worksheet_write_string(worksheet, row, MMSCol, "ja", NULL); // successfull connect
        if (worksheetError != LXW_NO_ERROR){
            fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
        }
        fprintf(fptr, "Connected to device!\n\n");
        printf("Connected to device!\n\n");
        return true;
    }
    worksheetError = worksheet_write_string(worksheet, row, MMSCol, "nej", NULL); // unsuccessfull connect
    if (worksheetError != LXW_NO_ERROR){
        fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
    }
    fprintf(fptr, "Couldn't connect to device!\n");
    fprintf(fptr, "Error: %s\n\n", IedClientError_toString(*err));
    printf("Couldn't connect to device!\n");
    printf("Error: %s\n\n", IedClientError_toString(*err));
    return false; 
}


void processDeviceData(IedConnection con, IedClientError *err, const char *deviceName, const char *nodeName, dataAttribute DAs[], int DASize, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr){
    char daRef[64];     // create buffer for copy and modification of attribute path
    bool tryAgain;
    const char *strVal; 

    /* iterate through all data attributes*/
    for(size_t i = 0; i < DASize; i++){
        tryAgain = true;
        snprintf(daRef, sizeof(daRef), "%s/%s.%s.%s", deviceName, nodeName, DAs[i].DO, DAs[i].DA);     //concatenate the different paths to a full path
        MmsValue *value = IedConnection_readObject(con, err, daRef, IEC61850_FC_DC);    // read object at path
        if(value != NULL){
            strVal = MmsValue_toString(value);
            if(strVal != NULL && strVal[0] != '\0'){
                tryAgain = false;
                worksheetError = worksheet_write_string(worksheet, row, DAs[i].excelCol, strVal, NULL);  // write to worksheet if the string isn't empty
                if (worksheetError != LXW_NO_ERROR){
                    fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
                }
                fprintf(fptr, "Successfull read from path: %s\n", daRef);
                printf("Successfull read from path: %s\n", daRef);
            }
        }
        else{
            fprintf(fptr, "Error reading from IED!\n REASON: %s.\n", IedClientError_toString(*err));
            printf("Error reading from IED!\n REASON: %s.\n", IedClientError_toString(*err));
            tryAgain = false;   // no idea changing FC, likely failed due to other reasons.
        }

        /* try to read other functional constraint if above failed */
        if (tryAgain){
            value = IedConnection_readObject(con, err, daRef, IEC61850_FC_EX);
            if(value != NULL){
                strVal = MmsValue_toString(value);
                if(strVal != NULL && strVal[0] != '\0'){
                    worksheetError = worksheet_write_string(worksheet, row, DAs[i].excelCol, strVal, NULL);  // write to worksheet if the string isn't empty
                    if (worksheetError != LXW_NO_ERROR){
                        fprintf(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
                    }
                    fprintf(fptr, "Successfull read from path: %s\n", daRef);
                    printf("Successfull read from path: %s\n", daRef);
                }else{
                    fprintf(fptr, "Error reading from path: %s\n", daRef);
                    printf("Error reading from path: %s\n", daRef);
                }
            }else{
                fprintf(fptr, "Error reading from IED!\n REASON: %s.\n", IedClientError_toString(*err));
                printf("Error reading from IED!\n REASON: %s.\n", IedClientError_toString(*err));
            }
        }

        MmsValue_delete(value);
    }
    printf("\n");
}


char *get_LD_name(IedConnection con, IedClientError *err, FILE *fptr, char *iedName){
    fprintf(fptr, "Finding logical device...\n");
    const char *LDs[] = {"LD0", "Application"};
    const int LDSize = sizeof(LDs) / sizeof(LDs[0]);
    char *pos;

    LinkedList LDList = IedConnection_getServerDirectory(con, err, false);  // fetch all Logical devices
    if(!LDList){
        fprintf(fptr, "No logical devices found!\n\n");
        return "";
    }

    LinkedList currentDevice = LinkedList_getNext(LDList);  
    while (currentDevice){

        for (int i = 0; i < LDSize; i++){
            if(pos = strstr(currentDevice->data, LDs[i])){     // looking for device which includes substring
                fprintf(fptr, "Found device %s\n\n", currentDevice->data);

                int iedNameLength = strlen(currentDevice->data) - strlen(pos);   // calculate length of Ied name
                if (iedNameLength > 0){
                    strncpy(iedName, currentDevice->data, iedNameLength);   // take out IED name via copying 'iedNameLength' chars to iedName
                    iedName[iedNameLength] = '\0';  
                }
                return currentDevice->data;
            }    
        }
        
        currentDevice = currentDevice->next;    // move down the linkedlist
    }

    LinkedList firstDevice = LinkedList_getNext(LDList);
    if (firstDevice) {
        fprintf(fptr, "Couldn't find logical device including 'LD0'. Defaulting to first logical device found (%s).\n\n", firstDevice->data);
        return firstDevice->data;
    }

    fprintf(fptr, "No logical device found!\n\n");
    return "";
} 


char *findLogicalNode(LinkedList logicalNodes, const char *priorityName){
    char *LNsToSearch[] = {"LPHD0", "LPHD1", "LLN0"};

    LinkedList current = LinkedList_getNext(logicalNodes);

    LinkedList currentCopy1 = current;  // copy 
    while (currentCopy1){
        if (strcmp(priorityName, currentCopy1->data) == 0) {
            return currentCopy1->data; 
        }   
        currentCopy1 = currentCopy1->next;
    }

    for (size_t i = 0; i < sizeof(LNsToSearch) / sizeof(LNsToSearch[0]); i++){
        LinkedList currentCopy2 = current;  // copy 

        while (currentCopy2){
            if (strcmp(LNsToSearch[i], currentCopy2->data) == 0) {
                return currentCopy2->data; 
            }   
            currentCopy2 = currentCopy2->next;
        }
    }
    
    if(current){
        return current->data;
    }
    return "";
}

void testLDList(IedConnection connection, IedClientError *err, const char *hostname){
    LinkedList LDList = IedConnection_getServerDirectory(connection, err, false);  // fetch all Logical devices
    if(LDList != NULL){
        LinkedList currentDevice = LinkedList_getNext(LDList);  
        while (currentDevice != NULL){
            printf("    LD: %s\n", currentDevice->data);
            currentDevice = currentDevice->next;    // move down the linkedlist
        }
    }
}
