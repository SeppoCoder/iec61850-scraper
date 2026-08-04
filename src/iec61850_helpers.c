#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iec61850_helpers.h"

bool connectToIED(IedConnection connection, IedClientError *err, const char *deviceIP, int tcpPort, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr, int MMSCol){
    LOG(fptr, "Connecting to IED...\n");
    IedConnection_connect(connection, err, deviceIP, tcpPort);     // connect to physical device using the connection above

    if (*err == IED_ERROR_OK){
        IedConnection_getDeviceModelFromServer(connection, err);   // update device model buffer
        if (*err != IED_ERROR_OK){
            LOG(fptr, "Couldn't buffer device data!\n");
        }

        worksheetError = worksheet_write_string(worksheet, row, MMSCol, "ja", NULL); // successfull connect
        if (worksheetError != LXW_NO_ERROR){
            LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
        }
        LOG(fptr, "Connected to device!\n\n");
        return true;
    }
    worksheetError = worksheet_write_string(worksheet, row, MMSCol, "nej", NULL); // unsuccessfull connect
    if (worksheetError != LXW_NO_ERROR){
        LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
    }
    LOG(fptr, "Couldn't connect to device!\n");
    LOG(fptr, "Error: %s\n\n", IedClientError_toString(*err));
    return false; 
}


bool writeDeviceData(MmsValue *value, const char *daRef, lxw_worksheet *worksheet, int col, int row, FILE *fptr, IedClientError *err, bool firstIteration){
    if(value != NULL){
        const char *strVal = MmsValue_toString(value);
        if(strVal != NULL && strVal[0] != '\0'){
            LOG(fptr, "Successfull read from path: %s, with value %s\n", daRef, strVal);
            lxw_error worksheetError = worksheet_write_string(worksheet, row, col, strVal, NULL);  // write to worksheet if the string isn't empty
            if (worksheetError != LXW_NO_ERROR){
                LOG(fptr, "Couldn't write to worksheet, returned with error code %d (%c).\n", worksheetError, lxw_strerror(worksheetError));
            }
            return true;
        }
        if(firstIteration){
            LOG(fptr, "Error while reading from path: %s\n", daRef);
        }
    }
    else{
        LOG(fptr, "Error reading from IED!\n REASON: %s.\n", IedClientError_toString(*err));
    }

    return false;
}

int calculateIpIndex(const char *ipAddr, int ipMinHost, FILE *fptr){
    char *ipHostDot = strrchr(ipAddr, '.');
    char *ipHost = (ipHostDot == NULL) ? NULL : ipHostDot + 1;
    if (!ipHost){
        LOG(fptr,"Error while parsing user given IP.\n");
        return -1;
    }

    int ipHostInt = atoi(ipHost);
    if (ipHostInt == 0){
        LOG(fptr,"Error while converting user given IP to integer.\n");
        return -1;
    }
    int ipIndex = ipHostInt - ipMinHost + 1;
    if (ipIndex <= 0) {
        LOG(fptr,"Error while calculating IP index.\n");
        return -1;
    }

    return ipIndex;
}

void processUserDevice(const char *ipAddr, int tcpPort, const char *path, int ipMinHost, lxw_worksheet *worksheet, int excelCol, FILE *fptr){
    LOG(fptr, "\n-------------------------------\n");
    LOG(fptr, "Processing user specific device...\n");
    int ipIndex = calculateIpIndex(ipAddr, ipMinHost, fptr);
    if (ipIndex < 0){ 
        LOG(fptr, "Invalid IP address!\n");
        return;
    }
    
    LOG(fptr, "Creating connection!\n");
    IedConnection connection = IedConnection_create();
    IedClientError err;
    LOG(fptr, "Connection created!\n");
    IedConnection_connect(connection, &err, ipAddr, tcpPort);
    LOG(fptr, "Connecting!\n");
    if (err != IED_ERROR_OK){
        LOG(fptr, "Couldn't connect to device!\n");
        LOG(fptr, "Error: %s\n\n", IedClientError_toString(err));
        IedConnection_destroy(connection);
        return;
    }
    LOG(fptr, "Connected to user specified device!\n\n");

    bool success = false;
    MmsValue *value = IedConnection_readObject(connection, &err, path, IEC61850_FC_DC);
    success = writeDeviceData(value, path, worksheet, excelCol, ipIndex, fptr, &err, false);
    if (!success){
        LOG(fptr, "Trying again, now reading with FC: IEC61850_FC_EX\n");
        value = IedConnection_readObject(connection, &err, path, IEC61850_FC_EX);
        writeDeviceData(value, path, worksheet, excelCol, ipIndex, fptr, &err, true);
    }
    MmsValue_delete(value);

    IedConnection_destroy(connection);
} 

void processDeviceData(IedConnection con, IedClientError *err, const char *deviceName, GeneralPath generalPath[], int count, lxw_worksheet *worksheet, int row, lxw_error worksheetError, FILE *fptr){
    char daRef[64];     // create buffer for copy and modification of attribute path
    /* iterate through all data attributes*/
    for(size_t i = 0; i < count; i++){
        for (size_t j = 0; *generalPath[i].paths[j] != '\0'; j++){
            bool success = false;
            snprintf(daRef, sizeof(daRef), "%s/%s", deviceName, generalPath[i].paths[j]);     //concatenate the different paths to a full path

            MmsValue *value = IedConnection_readObject(con, err, daRef, IEC61850_FC_DC);    // read object at path
            success = writeDeviceData(value, daRef, worksheet, generalPath[i].excelColumn, row, fptr, err, false);
            if (!success){
                value = IedConnection_readObject(con, err, daRef, IEC61850_FC_EX);
                success = writeDeviceData(value, daRef, worksheet, generalPath[i].excelColumn, row, fptr, err, true);
            }

            MmsValue_delete(value);    
            if (success){
                break;
            }
        }
    }
}

char *get_LD_name(IedConnection con, IedClientError *err, FILE *fptr, char *iedName){
    LOG(fptr, "Finding logical device...\n");
    const char *LDs[] = {"LD0", "Application"}; // Priority LDs
    char *pos;  // pointer for pointing where substring in string is found
    int iedNameLength;

    LinkedList LDList = IedConnection_getServerDirectory(con, err, false);  // fetch all Logical devices
    if(!LDList){
        LOG(fptr, "No logical devices found!\n\n");
        return '\0';
    }

    LinkedList currentDevice = LinkedList_getNext(LDList);  
    while (currentDevice){
        for (int i = 0; i < ARRAY_SIZE(LDs); i++){
            if(pos = strstr(currentDevice->data, LDs[i])){     // looking for device which includes substring
                LOG(fptr, "Found device %s\n\n", currentDevice->data);

                iedNameLength = strlen(currentDevice->data) - strlen(pos);   // calculate length of Ied name
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
        LOG(fptr, "Couldn't find logical device including 'LD0' or 'Application'. Defaulting to first logical device found (%s).\n\n", firstDevice->data);
        iedNameLength = strlen(firstDevice->data);
        if (iedNameLength > 0){
            strncpy(iedName, firstDevice->data, iedNameLength);   // take out IED name via copying 'iedNameLength' chars to iedName
            iedName[iedNameLength] = '\0';
        }  
        return firstDevice->data;
    }

    LOG(fptr, "No logical device found!\n\n");
    return '\0';
} 


__attribute__((unused)) char *findLogicalNode(LinkedList logicalNodes){
    char *LNsToSearch[] = {"LPHD0", "LPHD1", "LLN0"};

    LinkedList current = LinkedList_getNext(logicalNodes);

    for (size_t i = 0; i < ARRAY_SIZE(LNsToSearch); i++){
        LinkedList temp = current;  // copy 

        while (temp){
            if (strcmp(LNsToSearch[i], temp->data) == 0) {
                return temp->data; 
            }   
            temp = temp->next;
        }
    }
    
    if(current){
        return current->data;
    }
    return '\0';
}
