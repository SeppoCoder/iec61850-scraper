// gcc main.c -o main -liec61850 -lxlsxwriter
// x86_64-w64-mingw32-gcc main.c -o main -liec61850 -lxlsxwriter

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// IEC61850 client library to handle connections to server
#include <libiec61850/iec61850_client.h>

// xlsx library for handling writing to excel spreadsheet
#include <xlsxwriter.h>

/* returns the name of the first logical device found, NULL if not found. */
const char *get_LD_name(IedConnection con, IedClientError *err){
    LinkedList LDList = IedConnection_getServerDirectory(con, err, false);  // fetch all Logical devices
    if (LDList != NULL){
        LinkedList LD = LinkedList_getNext(LDList); // get first logical device
        if (LD != NULL){
            return LD->data;    // return the name (data) of the LD
        }
    }
    return NULL;
}

int main(){
    const char *topLevelPaths[] = {"LPHD0", "LPHD1", "LLN0"};
    const char *midLevelPaths[] = {"PhyNam", "NamPlt", "PhyHealth"};
    const char *dataAttributePaths[] = {"model", "serNum", "vendor", "name", "swRev"};
    const char *hostname = "127.0.0.1";   // IP address of device you want to connect to
    int tcpPort = 102;  // TCP port of device (IEC 16850 has port 102 as standard)

    IedClientError err;     // error reason

    const char workbookName[] = "AA11.xlsx";
    lxw_workbook  *workbook  = workbook_new(workbookName);    // create a new workbook
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, NULL);  // add a sheet to the newly created workbook

    IedConnection connection = IedConnection_create();  // create a connection

    IedConnection_connect(connection, &err, hostname, tcpPort);     // connect to physical device using the connection above
    printf("Connecting to %s:%i\n", hostname, tcpPort);

    if (err != IED_ERROR_OK)
    {
        printf("couldn't connect to %s:%i\n", hostname, tcpPort);   
        IedConnection_destroy(connection);      // destroy (disconnect) 
        workbook_close(workbook);   // close and save workbook
        return -1;
    }

    printf("Connected!\n\n");

    worksheet_write_string(worksheet, 0, 0, "Device", NULL);    
    worksheet_write_string(worksheet, 1, 0, hostname, NULL);    // write IP-address to excel worksheet

    const char *deviceName = get_LD_name(connection, &err);   // fetch name of logical device
    if(deviceName == NULL){
        printf("couldn't find device name\n");   
        IedConnection_destroy(connection);      // destroy (disconnect) 
        workbook_close(workbook);   // close and save workbook
        return -1;
    }

    char daRef[50];     // create buffer for copy and modification of attribute path
    /* iterate through all possible paths from the given path lists */
    for(size_t daIndex = 0; daIndex < sizeof(dataAttributePaths) / sizeof(dataAttributePaths[0]); daIndex++){
        worksheet_write_string(worksheet, 0, daIndex+1, dataAttributePaths[daIndex], NULL);  // write the data attribute name to spreadsheet
        for (size_t top = 0; top < sizeof(topLevelPaths) / sizeof(topLevelPaths[0]); top++){
            for(size_t mid = 0; mid < sizeof(midLevelPaths) / sizeof(midLevelPaths[0]); mid++){

                snprintf(daRef, sizeof(daRef), "%s/%s.%s.%s", deviceName, topLevelPaths[top], midLevelPaths[mid], dataAttributePaths[daIndex]);     //concatenate the different paths to a full path
                MmsValue *value = IedConnection_readObject(connection, &err, daRef, IEC61850_FC_DC);    // read object at path

                if(value != NULL){
                    switch (MmsValue_getType(value)){    // Parse result based on data type

                        case MMS_VISIBLE_STRING:
                        case MMS_STRING: {
                            const char *strVal = MmsValue_toString(value);
                            worksheet_write_string(worksheet, 1, daIndex+1, strVal, NULL);  // write value to spreadsheet
                            printf("read string value '%s' at path '%s'\n", strVal, daRef);
                            break;
                        }

                        case MMS_INTEGER:
                        case MMS_UNSIGNED: {
                            int intVal = MmsValue_toInt32(value);
                            worksheet_write_number(worksheet, 1, daIndex+1, intVal, NULL);  // write value to spreadsheet
                            printf("read number value: %d\n", intVal);
                            break;
                        }

                        case MMS_BOOLEAN: {
                            bool boolVal = MmsValue_getBoolean(value);
                            worksheet_write_boolean(worksheet, 1, daIndex+1, boolVal, NULL);  // write value to spreadsheet
                            printf("read boolean value: %d\n", boolVal);
                            break;
                        }

                        default:
                            break;
                    }
                }
                else{
                    printf("Error reading from object!\n");
                }
                MmsValue_delete(value);    // always free dynamically allocated memory when MmsValue has been created
            }
        }
    }
    
    workbook_close(workbook);   // close and save workbook
    IedConnection_destroy(connection);      // destroy connection (disconnect)
    printf("\n");
    printf("Wrote results to %s\n", workbookName);
    printf("Disconnected!\n");
}   