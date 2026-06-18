// gcc main.c -o main -liec61850

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// IEC61850 client library to handle connections to server
#include <libiec61850/iec61850_client.h>

int main(){
    size_t arrSize = 3;
    char *pathSuffix[3] = {"/LLN0.NamPlt.vendor", "/LLN0.NamPlt.swRev", "/LLN0.NamPlt.d"};
    char *hostname = "localhost";   // IP address of device you want to connect to
    int tcpPort = 102;              // TCP port of device (IEC 16850 has port 102 as standard)

    IedClientError err;     // error reason
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

    char *deviceName = LinkedList_getNext(IedConnection_getLogicalDeviceList(connection, &err))->data;
    char daRef[50];
    for (size_t i = 0; i < arrSize; i++)
    {
        strcpy(daRef, deviceName);
        strcat(daRef, pathSuffix[i]);
        printf("reading from path: '%s'\n", daRef);

        MmsValue* value = IedConnection_readObject(connection, &err, daRef, IEC61850_FC_DC);
        if(value != NULL){
            switch (MmsValue_getType(value))
            {
            case MMS_VISIBLE_STRING:
            case MMS_STRING:
                const char *strVal = MmsValue_toString(value);
                printf("read string value: %s\n", strVal);
                break;
            case MMS_INTEGER:
            case MMS_UNSIGNED:
                int intVal = MmsValue_toInt32(value);
                printf("read number value: %d\n", intVal);
                break;
            case MMS_BOOLEAN:
                bool boolVal = MmsValue_getBoolean(value);
                printf("read boolean value: %d\n", boolVal);
            default:
                printf("Couldn't read from object '%s'\n", daRef);
                break;
            }
        }
        printf("\n");
        MmsValue_delete(value);    // always free dynamically allocated memory when MmsValue has been created
    }

    IedConnection_destroy(connection);      // destroy (disconnect)
    printf("Disconnected!\n");
}

