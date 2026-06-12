// gcc main.c -o main -liec61850

#include <stdlib.h>
#include <stdio.h>

// IEC61850 client library to handle connections to server
#include <libiec61850/iec61850_client.h>

int main(){
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

    printf("Connected!\n");

    Ied

    IedConnection_destroy(connection);      // destroy (disconnect)
}