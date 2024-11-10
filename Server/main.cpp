/*
    HashNet X Server
    https://github.com/invpe/HashNet
*/ 
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include "CServer.h"

CServer _Server;
void my_handler(int s)
{
    _Server.Stop();
    sleep(3);
    exit(0);
    
}

int main(int argc, char **argv)
{
    
    srand(time(0));


    // Bind CTRLC
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    printf("---------------------------------------------\n");
    printf("HashNet server version "SERVER_VERSION"\n");
    printf("https://github.com/invpe/HashNet\n");
    printf("---------------------------------------------\n\n"); 
    
     
    if(_Server.Start(SERVER_PORT,1024) == false)
        return -1;
    
    
    while(1)
    {
        _Server.Tick();
    }
    return 0;
}

