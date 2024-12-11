#include <signal.h>
#include <assert.h>
#include <random>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "CMiner.h"

CMiner _Miner;

int main(int argc, char*argv[]) {   
   
    printf("--------------------------------------------------------------\n");
    printf("HASHNET Miner https://github.com/invpe/HashNet " MINER_VERSION "\n");
    printf("Compilation " __TIME__ " " __DATE__ "\n");

    printf("--------------------------------------------------------------\n");

    if(argc<3)
    {
        printf("./start server node_id \n");
        exit(0);
    }
 
    std::string strHost = argv[1];
    std::string strIdent = argv[2];
    

    if(_Miner.Setup(strHost,strIdent) == false)
        exit(0);

    while(1)
    {
    	_Miner.Tick();
    }
    return 0;
}