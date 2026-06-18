// gcc main.c -o main -liec61850 -lxlsxwriter

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// IEC61850 client library to handle connections to server
#include <libiec61850/iec61850_client.h>

// xlsx library for handling writing to excel spreadsheet
#include <xlsxwriter.h>

/* returns the name of the dataAttribute for the given path.*/
char* get_dataAttribute_name(char *path){
    char pathCopy[50];      // buffer for copy
    strcpy(pathCopy, path); // copy original path as to not overwrite it
    char* localToken = strtok(pathCopy, " . ");  // get first part of splitted string 
    printf("localToken1: %s\n", localToken);
    for(size_t i = 0; i < 2; i++){
        localToken = strtok(NULL, " . ");        // keep on splitting until we get the third (last) token.
    }
    printf("localToken2: %s\n", localToken);
    return localToken;
}

int main(){
    size_t arrSize = 3;
    char *pathSuffix[3] = {"/LLN0.NamPlt.vendor", "/LLN0.NamPlt.swRev", "/LLN0.NamPlt.d"};      // paths to data attributes
    char *hostname = "127.0.0.1";   // IP address of device you want to connect to
    int tcpPort = 102;              // TCP port of device (IEC 16850 has port 102 as standard)

    IedClientError err;     // error reason

    lxw_error xlsxErr;
    lxw_workbook  *workbook  = workbook_new("newWorkbook.xlsx");    // create a new workbook
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, NULL);  // add a sheet to the newly created workbook

    IedConnection connection = IedConnection_create();  // create a connection
    IedConnection_connect(connection, &err, hostname, tcpPort);     // connect to physical device using the connection above
    printf("Connecting to %s:%i\n", hostname, tcpPort);

    if (err != IED_ERROR_OK)
    {
        printf("couldn't connect to %s:%i\n", hostname, tcpPort);   
        IedConnection_destroy(connection);      // destroy (disconnect) 
        return -1;
    }

    printf("Connected!\n\n");
    xlsxErr = worksheet_write_string(worksheet, 0, 0, "Device", NULL);    
    xlsxErr = worksheet_write_string(worksheet, 1, 0, hostname, NULL);    // write IP-address to excel worksheet

    char *deviceName = LinkedList_getNext(IedConnection_getLogicalDeviceList(connection, &err))->data;  // Fetch logical device name 
    char daRef[50];     // create buffer for copy and modification of attribute path
    for (size_t i = 0; i < arrSize; i++)
    {
        const char *token = (const char*) get_dataAttribute_name(pathSuffix[i]);
        xlsxErr = worksheet_write_string(worksheet, 5, i+1, token, NULL);
        printf("excel error: %s\n", lxw_strerror(xlsxErr));

        strcpy(daRef, deviceName);  // copy deviceName into dataAttribute reference as to not overwrite deviceName
        strcat(daRef, pathSuffix[i]);   // concatenate deviceName + pathToAttribute to get whole dataAttribute reference
        printf("reading from path: '%s'\n", daRef);

        MmsValue* value = IedConnection_readObject(connection, &err, daRef, IEC61850_FC_DC);    // read object at path
        if(value != NULL){
            switch (MmsValue_getType(value))    // Parse result based on data type
            {
            case MMS_VISIBLE_STRING:
            case MMS_STRING:
                const char *strVal = MmsValue_toString(value);
                xlsxErr = worksheet_write_string(worksheet, 1, i+1, strVal, NULL);
                printf("read string value: %s\n", strVal);
                break;
            case MMS_INTEGER:
            case MMS_UNSIGNED:
                int intVal = MmsValue_toInt32(value);
                xlsxErr = worksheet_write_number(worksheet, 1, i+1, intVal, NULL);
                printf("read number value: %d\n", intVal);
                break;
            case MMS_BOOLEAN:
                bool boolVal = MmsValue_getBoolean(value);
                xlsxErr = worksheet_write_boolean(worksheet, 1, i+1, boolVal, NULL);
                printf("read boolean value: %d\n", boolVal);
                break;
            default:
                xlsxErr = worksheet_write_string(worksheet, 1, i+1, "ERROR", NULL);
                printf("Couldn't read from object '%s'\n", daRef);
                break;
            }
        }
        printf("\n");
        MmsValue_delete(value);    // always free dynamically allocated memory when MmsValue has been created
    }

    IedConnection_destroy(connection);      // destroy (disconnect)
    printf("Disconnected!\n");

    workbook_close(workbook);   // close and save workbook
}   