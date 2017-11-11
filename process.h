#ifndef PROCESS_HEADER
    #define PROCESS_HEADER
#include <stdlib.h>

/*Even though freeing a null ptr is supposed to be safe*/
#define CHECK_AND_FREE(ptr) do{if(ptr)free(ptr);}while(0);

enum action{ /*enum representing actions a process can take*/
    REQUEST,
    RELEASE,
    CALCULATE, 
    USERESOURCES
} typedef action_e;


struct process_actions{
    action_e action;
    int computation_time;
    int num_resources;
    int * resources; /*array representing the number of resources needed for 
                       the current action.  NULL if a calcualte or usereseource
                       action (as they do not need any resources)*/
} typedef process_actions_t;


struct process{
    int pid;/*this is a custom pid and not the unix pid*/
    int finished;

    int deadline;
    int computation_time;

    int num_resources;
    int * allocated_resources;
    int * needed;

    int num_actions; /*total number of things this process needs to do*/
    int next_action; /*index of current thing process needs to do*/
    process_actions_t * actions; /*array representing all the things a 
                                   process needs to do. */

} typedef process_t;

void process_destructor(process_t process)
{
    int i;
    for(i = 0; i < process.num_actions; i++)
        if(process.actions[i].num_resources > 0)
            CHECK_AND_FREE(process.actions[i].resources);
    CHECK_AND_FREE(process.actions);
    CHECK_AND_FREE(process.needed);
    CHECK_AND_FREE(process.allocated_resources);
}

void free_processes(process_t * process_list, int num_processes)
{
    int i;
    for(i = 0; i < num_processes; i++)
        process_destructor(process_list[i]); 
}

#endif
