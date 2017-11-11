#ifndef DEBUG_FILE
    #define DEBUG_FILE
#include <stdio.h>

int debug_on = 0;

#define DEBUG_PARENT(MSG) if(debug_on){printf("PARENT: %s\n", MSG);}
#define DEBUG_CHILD(MSG) if(debug_on){printf("CHILD: %s\n", MSG);}

#endif
