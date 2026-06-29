// gcc main.c -o main -liec61850 -lxlsxwriter

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// IEC61850 client library to handle connections to server
#include <libiec61850/iec61850_client.h>

// xlsx library for handling writing to excel spreadsheet
#include <xlsxwriter.h>

/* returns the name of the dataAttribute for the given path.*/
const char *get_dataAttribute_name(const char *path){
    const char *lastDot = strchr(path, '.');    // get last occurence of '.' in path name
    if(lastDot != NULL){
        return lastDot + 1;     // use pointer arithmetic to access characters after last dot
    }
    return NULL;
}

/* returns the name of the first logical device found */
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
    const char *pathSuffix[] = {"/LLN0.NamPlt.vendor", "/LLN0.NamPlt.swRev", "/LLN0.NamPlt.d"};      // paths to data attributes
    const char *hostname = "127.0.0.1";   // IP address of device you want to connect to
    int tcpPort = 102;              // TCP port of device (IEC 16850 has port 102 as standard)

    IedClientError err;     // error reason

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
    worksheet_write_string(worksheet, 0, 0, "Device", NULL);    
    worksheet_write_string(worksheet, 1, 0, hostname, NULL);    // write IP-address to excel worksheet

    const char *deviceName = get_LD_name(connection, &err);   // fetch name of logical device
    if(deviceName == NULL){
        printf("%s\n", "couldn't find device name");   
        IedConnection_destroy(connection);      // destroy (disconnect) 
        return -1;
    }

    char daRef[50];     // create buffer for copy and modification of attribute path
    for (size_t i = 0; i < sizeof(pathSuffix) / sizeof(pathSuffix[0]); i++)
    {
        const char *daName = get_dataAttribute_name(pathSuffix[i]); // fetch name of data attribute
        if(daName == NULL){
            printf("couldn't find find data attribute at (%s)\n", pathSuffix[i]);   
            IedConnection_destroy(connection);      // destroy (disconnect) 
            return -1;
        }
        worksheet_write_string(worksheet, 0, i+1, daName, NULL);  // write the data attribute name to spreadsheet 

        snprintf(daRef, sizeof(daRef), "%s%s", deviceName, pathSuffix[i]);
        printf("reading from path: '%s'\n", daRef);

        MmsValue *value = IedConnection_readObject(connection, &err, daRef, IEC61850_FC_DC);    // read object at path
        if(value != NULL){
            switch (MmsValue_getType(value))    // Parse result based on data type
            {
            case MMS_VISIBLE_STRING:
            case MMS_STRING: {
                const char *strVal = MmsValue_toString(value);
                worksheet_write_string(worksheet, 1, i+1, strVal, NULL);  // write value to spreadsheet
                printf("read string value: %s\n", strVal);
                break;
            }
            case MMS_INTEGER:
            case MMS_UNSIGNED: {
                int intVal = MmsValue_toInt32(value);
                worksheet_write_number(worksheet, 1, i+1, intVal, NULL);  // write value to spreadsheet
                printf("read number value: %d\n", intVal);
                break;
            }
            case MMS_BOOLEAN: {
                bool boolVal = MmsValue_getBoolean(value);
                worksheet_write_boolean(worksheet, 1, i+1, boolVal, NULL);  // write value to spreadsheet
                printf("read boolean value: %d\n", boolVal);
                break;
            }
            default:
                worksheet_write_string(worksheet, 1, i+1, "ERROR", NULL);  // write Error to spreadsheet
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