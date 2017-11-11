#ifndef DEBUG_FILE
    #define DEBUG_FILE
#include <stdio.h>
#include "process.h"
#include "manager.h"
#include "helper.h"
#include "parsing.h"
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>

int debug_on = 1;

#define DEBUG_PARENT(MSG) if(debug_on){printf("PARENT: %s\n", MSG);}
#define DEBUG_CHILD(MSG) if(debug_on){printf("CHILD: %s\n", MSG);}


void print_process_info()
{
    int i;
    for(i = 0; i < num_processes; i++){

    }
}




#endif
