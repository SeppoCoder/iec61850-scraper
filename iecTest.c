#include <stdio.h>
#include "iec61850_client.h"

int main(){
    printf("Initializing...\n");

    const char *hostname = "127.0.0.1";   // IP address of device you want to connect to
    int tcpPort = 102;  // TCP port of device (IEC 16850 has port 102 as standard)

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

    IedConnection_destroy(connection);      // destroy connection (disconnect)
    printf("Disconnected!\n");

    return 0;
}