#ifndef PARSING_FILE
    #define PARSING_FILE

#include <stdio.h>
#include "process.h"
#include "manager.h"
#include "helper.h"
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>

#define MAX_STR_LEN 1024 

/*  just a constant for the max number of actions for a process that my program 
 *  will accept, did it this way to avoid too much dynamic allocation.
 *  Why use more than the stack when thats all you need?
 */
#define MAX_NUM_ACTIONS 256 


/*  Funtion that will set two ints, num proc and num resourc and will return a
 *  long represnting the offset in the file passed in
 */
long get_proc_and_resc(char * filename, int * num_resources, int * num_processes)
{
    FILE * file = fopen(filename, "r");
    char str [MAX_STR_LEN];
    fgets(str, sizeof(str), file);
    *num_processes = string_to_int(str); 
    memset(str, 0, sizeof(str));
    fgets(str, sizeof(str), file);
    *num_resources = string_to_int(str);
    long offset = ftell(file);
    fclose(file);
    return offset; 
}

/*  Populates our `avaliable` array for avaliable resources
 */
long get_avaliable_resources(char * filename, int num_resources, 
                             int * avaliable, long offset)
{
    FILE * file = fopen(filename, "r");
    char str [MAX_STR_LEN];
    int i;
    fseek(file, offset, SEEK_SET);
    for(i = 0; i < num_resources; i++){
        fgets(str, sizeof(str), file);
        avaliable[i] = string_to_int(str);
        memset(str, 0, sizeof(str)); 
    }
    offset = ftell(file);
    fclose(file);
    return offset;
}

/*  Expects a line in the form of Max[\d, \d]=\d
 *  returns the int for the assignment
 */
int get_max_resource(char * str)
{
    int i = 0, found_equals = 0;
    int num = 0;
    while(str[i] != '\0' && str[i] != '\n'){
        if(found_equals){
            if(!isdigit(str[i]))
                break;
            num*=10;
            num+= str[i] - '0';
        } else if(str[i] == '=')
            found_equals = 1;
        i++;
    }
    return num;
}

/*  Populates the `needed` variable for all of the proceses
 */
long populate_process_needed(char * filename, int num_processes, 
                             process_t * processes, int num_resources, 
                             int * avaliable, long offset)
{
    FILE * file = fopen(filename, "r");
    fseek(file, offset, SEEK_SET);
    int i, j;
    char str [MAX_STR_LEN];
    for(i = 0; i < num_processes; i++){
        processes[i].pid = i;
        processes[i].num_resources = num_resources;
        processes[i].actions = 0;
        processes[i].allocated_resources = malloc(sizeof(int) * num_resources);
        processes[i].needed = malloc(sizeof(int) * num_resources);
        for(j = 0; j < num_resources; j++){
            processes[i].allocated_resources[j] = 0;
            fgets(str, sizeof(str), file);
            processes[i].needed[j] = get_max_resource(str); 
        }
    }
    offset = ftell(file);
    fclose(file);
    return offset;
}

/*  Expects to find an int in between two parenthesis
 *  used for handling calulate and useresource actions
 */
int get_int_between_paren(char * str)
{
    int i = 0, num = 0, found_parenth = 0;
    while(str[i] != '\0' && str[i] != '\n'){
        if(found_parenth){
            if(str[i] == ')')
                break;
            num*=10;
            num+= str[i] - '0';
        }
        if(str[i] == '(')
            found_parenth = 1;
        i++;
    }
    return num;
}

void populate_action_resources(char * str, process_t * process_list, 
                               int pid, int action_id)
{
    int i = 0, num = 0, found_parenth = 0, cur_res = 0; 
    while(str[i] != '\0' && str[i] != '\n'){
        if(found_parenth){
            if(str[i] == ',' || str[i] == ')'){
                process_list[pid].actions[action_id].resources[cur_res] = num;
                num = 0;
                cur_res++;
                if(str[i] == ')')
                    break;
            }else{
                num *= 10;
                num += str[i] - '0';
            }
        }else{
            if(str[i] == '(')
                found_parenth = 1; 
        }
        i++;
    }
}

/*  Populates a handle entry for a process struct
 */
void handle_actions(process_t * process_list, int pid, action_e action_type, 
                    int action_index, char * str)
{
    int val, num_resources = process_list[pid].num_resources; 
    
    process_list[pid].actions[action_index].action = action_type;
    switch(action_type){
        case REQUEST:
        case RELEASE:
            process_list[pid].actions[action_index].computation_time = 1;
            process_list[pid].actions[action_index].num_resources = num_resources;
            process_list[pid].actions[action_index].resources = malloc(        \
                    sizeof(int) * num_resources);
            populate_action_resources(str,                                     \
                    process_list, pid, action_index); 
            return;

        case CALCULATE:
        case USERESOURCES:
            val = get_int_between_paren(str);
            process_list[pid].actions[action_index].computation_time = val;
            return;
    }
}

/*  Function to take in our array of action strings, parse them, and add them 
 *  to the correct struct for a particular process
 *  takes in process list because would have had to take in a pointer regardless
 */
void append_actions(process_t * process_list, int pid, 
                    int num_actions, char ** actions)
{
    process_list[pid].num_actions = num_actions;
    process_list[pid].next_action = 0;
    process_list[pid].actions = malloc(sizeof(process_actions_t) * num_actions);
    int i;
    for(i = 0; i < num_actions; i++){
        if(line_contains(actions[i], "request", 7)){
            handle_actions(process_list, pid, REQUEST, i, actions[i]); 
        }else if(line_contains(actions[i], "release", 7)){
            handle_actions(process_list, pid, RELEASE, i, actions[i]); 
        }else if(line_contains(actions[i], "useresources", 12)){
            handle_actions(process_list, pid, USERESOURCES, i, actions[i]); 
        }else if(line_contains(actions[i], "calculate", 9)){
            handle_actions(process_list, pid, CALCULATE, i, actions[i]); 
        }
    } 
}

/*  This function will populate the rest of the data for the processes from the input file
 */
long populate_processes(char * filename, int num_processes, int num_resources,
                        process_t * process_list, long offset)
{
    int i;
    FILE * file = fopen(filename, "r");
    fseek(file, offset, SEEK_SET);
    char str [MAX_STR_LEN];
    for(i = 0; i < num_processes; i++){
        while(1){//loop until we find line of form process_[\d+]
            fgets(str, sizeof(str), file);
            if(line_contains(str, "process", 7))
                break;
        }
        fgets(str, sizeof(str), file);
        process_list[i].deadline = string_to_int(str); 
        process_list[i].finished = 0;
        memset(str, 0, sizeof(str));
        fgets(str, sizeof(str), file);
        process_list[i].computation_time = string_to_int(str);
        char ** actions = calloc(MAX_NUM_ACTIONS, sizeof(char*));
        int j = 0, k = 0;
        while(1){
            memset(str, 0, sizeof(str));
            fgets(str, sizeof(str), file);
            if(line_contains(str, "end", 3))
                break;
            actions[j] = malloc(sizeof(MAX_STR_LEN));
            strcpy(actions[j], str);
            j++; 
        }
        append_actions(process_list, i, j, actions);
        for(k = 0; k < j; k++)
            free(actions[j]);
        free(actions);
    }
    offset = ftell(file);
    fclose(file);
    return offset; 
}

#endif
