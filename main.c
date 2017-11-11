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

#define DEBUG(MSG) printf("%s\n", MSG)

/*  just a constant for the max number of actions for a process that my program 
 *  will accept, did it this way to avoid too much dynamic allocation.
 *  Why use more than the stack when thats all you need?
 */
#define MAX_NUM_ACTIONS 256 

/*  Our global, shared varaibles */
int * clock = 0;
int * last_request = 0;
int * which_process = 0;    /*whos turn is it to run*/
int * avaliable = 0;            /*size */
int * current_request = 0;      /*size n + 1, represents either request 
                                  or release and the ammount*/
process_t * process_list;

sem_t *current_request_sem, *wait_response_sem, *clock_sem, *which_process_sem;

int num_resources, num_processes;



void make_variables_shared()
{
    clock = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, 
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0); 
    last_request = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, 
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0); 
    avaliable = mmap(NULL, sizeof(int) * num_resources, PROT_READ | PROT_WRITE, 
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0); 
    current_request = mmap(NULL, sizeof(int) * (num_resources+1), PROT_READ | PROT_WRITE, 
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0); 
    which_process = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, 
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0); 
    
    current_request_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, 
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    wait_response_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, 
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    clock_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, 
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    which_process_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, 
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);

}

void dealoc_shared_variables()
{
    munmap(clock, sizeof(int));
    munmap(last_request, sizeof(int));
    munmap(avaliable, sizeof(int)*num_resources);
    munmap(current_request, sizeof(int)*(num_resources+1));
    munmap(process_list, sizeof(process_t)*num_processes);
    munmap(which_process, sizeof(process_t));

    munmap(current_request_sem, sizeof(sem_t));
    munmap(wait_response_sem, sizeof(sem_t));
    munmap(clock_sem, sizeof(sem_t));
    munmap(which_process_sem, sizeof(sem_t));
}

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

/*  Not a good algorithm, this assumes that there is no other word in the line 
 *  that starts with the same letter
 */
int line_contains(char * input, char * check, int check_len)
{
    int i = 0, j = 0;
    while(input[i] != '\0' && input[i] != '\n'){
        if(check[j] == input[i]){
            j++;
            if(j >= check_len)
                return 1;
        }else
            j = 0;
        i++;
    }
    return 0; 
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
            num+= str[i] = '0';
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

void test_fork(int pid)
{
    sem_wait(current_request_sem);
    printf("avaliable[0] = %d\n", avaliable[0]);
    avaliable[0] = 5;
    sem_post(current_request_sem);
}

void subtract_list(int * list1, int * list2, int size)
{
    int i;
    for(i = 0; i < size; i++)
        list1[i] -= list2[i];
}

void run_child_process(int id)
{
    process_t * self;
    self = &process_list[id];

    while(!self->finished){ 
        sem_wait(which_process_sem);
        DEBUG("GRANTED WHICH PROCESS SEM IN CHILD");
        printf("which process: %d\n", *which_process);
        if(*which_process == id && !process_list[id].finished){
            DEBUG("RUNNING THIS PROCESS");
            int i;
                    switch(self->actions[self->next_action].action){
                /*  Populate the current_request register with the correct values
                 */
                case REQUEST:
                    current_request[0] = (int)REQUEST;
                    for(i=0; i < self->num_resources; i++)
                        current_request[i+1] = \
                            self->actions[self->next_action].resources[i];
                    break;
                case RELEASE:
                    current_request[0] = (int)RELEASE;
                    for(i=0; i < self->num_resources; i++)
                        current_request[i+1] = \
                            self->actions[self->next_action].resources[i];
                    break;
                case CALCULATE:
                    current_request[0] = (int)CALCULATE;
                    break;
                case USERESOURCES:
                    current_request[0] = (int)USERESOURCES;
                    break;
            }
            sem_post(current_request_sem);
            DEBUG("POSTED CURRENT REQUEST, WAITING ON RESPONSE");

            sem_wait(wait_response_sem);/*waits until the parent has responded*/
            DEBUG("GOT RESPONSE FROM PARENT");
            if(current_request[i] != 0){
                /*our action was run succesfully by parent*/    
                sem_wait(clock_sem);
                clock += self->actions[self->next_action].computation_time;
                sem_post(clock_sem);
                self->next_action++;
            }
            if(self->next_action >= self->num_actions)
                self->finished = 1;
            DEBUG("FINISHED RUNNING CHILD");
        }
        printf("child %d\n", id);
        sem_post(which_process_sem);
        //if this process_t is done, we can kill the actual linux process
        if(process_list[id].finished)
            exit(0);
    }
}

/*  Finds the next valid shortest process, returns -1 if all processes are 
 *  finished
 */
int find_shortest(int ignore)
{
    int shortest = -1, i;
    for(i = 0; i < num_processes; i++){
        if(process_list[i].finished)
            continue;
        if(ignore > 0 && i == ignore)
            continue;
        if(shortest == -1){
            shortest = i;
        }
        if(process_list[i].computation_time < \
           process_list[shortest].computation_time)
            shortest = i;
        else if(process_list[i].computation_time == \
                process_list[shortest].computation_time && 
                process_list[i].deadline < process_list[shortest].deadline)
            shortest = i;
    }
    return shortest;
}

int check_valid_request()
{
    int i;
    for(i = 0; i < num_resources; i++){
        if(current_request[i+1] > avaliable[i])
            return 0;    
    }
    return 1;
}

void bankers_algorithm(process_t * process_list)
{
    /*
 *  parent process;
 *      find the shortest process
 *      check if its finished, 
 *          if it is, find next shortest process and see if it is finished
 *          if all processes are finished exit
 *      see what the process wants to do, if its a request, check if it was the last request
 *          if it did make the last request, find the next shortest valid process
 *          unless this is the only process left or none of the other requests are possible
 *              in that case run this process
 *      if its not a request, or it is a request and this wasn't the last 
 *              process to make a request run until you hit two requests
 *      update the avaliable and clock registers
 *
 *  child processes
 *      check if it is your turn to run using semaphore
 *      if it is your turn
 *          change the request register to indicate to the parent process what you want to do
 *          check what the parent process returned
 *              a leading 0 in the array indicates sucess, -1 indicates that a 
 *              request couldn't be made
 *          after every succesfully ran request, remember to increment
 *                  the process's action counter
 *              if the action counter is greater than the number of actions 
 *                      mark the process as finished
 *      once a request can't be made set the first element in the request 
 *              register to the total time taken to run commands
 *      signal semaphores
 *      
 */ 

    int pid, i;
    *which_process = -1;
    for(i=0; i < num_processes; i++){//spawn all child processes
        pid = fork(); 
        if(pid != 0)
            break;
    }
    if(pid == 0){
        DEBUG("IN PARENT AT BEGINNING");
        int shortest = find_shortest(-1), i;
        while(shortest >= 0){
            printf("shortest: %d\n", shortest);
            /*let the process know its his turn*/
            sem_wait(which_process_sem);
            *which_process = shortest;
            sem_post(which_process_sem);

            DEBUG("WAITING FOR CURRENT REQUEST PARENT");
            /*wait for the child to make it's request*/
            sem_wait(current_request_sem);
            DEBUG("AFTER WAITING FOR CURRENT REQUEST IN PARENT");
            action_e action = (action_e)current_request[0];
            int temp_shortest;
            DEBUG("BEFORE PARENT SWITCH");printf("action: %d\n", (int)action);
            switch(action){/*determine how to respond to request*/
                case REQUEST:
                    if(*last_request == shortest){
                    /*if this is a request and this was the last process to 
                     *make a request        */
                        temp_shortest = find_shortest(shortest);
                        if(temp_shortest >= 0){
                            current_request[0] = -1;
                            shortest = temp_shortest;
                            sem_post(wait_response_sem);
                            continue;
                        }
                    } 
                    if(check_valid_request()){
                        for(i = 0; i < num_processes; i++) 
                            avaliable[i] -= current_request[i+1];
                    }else{
                        current_request[0] = -1;
                        shortest = find_shortest(shortest);  
                        sem_post(wait_response_sem);
                        continue;
                    } 
                    current_request[0] = 1;
                    *last_request = shortest;
                    break;
                case RELEASE:
                    for(i = 0; i < num_processes; i++) 
                        avaliable[i] += current_request[i+1];
                    break;
                case CALCULATE:
                case USERESOURCES:
                    break;
            }
            sem_post(wait_response_sem);
            DEBUG("OUTSIDE SWITCH");
        }
        //find shortest process 
    }else{
        run_child_process(i); 
    }
}

int main(int argc, char * argv [])
{
    char * filename = "sample-input1.txt";
    if(argc >= 2)
        filename = argv[1];
    num_processes = 0, num_resources = 0;
    long offset = get_proc_and_resc(filename, &num_processes, &num_resources);
    
    //populate avaliabnle resources
    avaliable = mmap(NULL, sizeof(int) * num_resources, 
                           PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 
                           -1, 0);
    offset = get_avaliable_resources(filename, num_resources, avaliable, offset);

    process_list = mmap(NULL, sizeof(process_t) * (num_processes), PROT_READ | PROT_WRITE, 
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0); 

    offset = populate_process_needed(filename, num_processes, process_list, 
                                     num_resources, avaliable, offset);
    offset = populate_processes(filename, num_processes, num_resources,
                                process_list, offset);
    make_variables_shared(); 
    *last_request = -1;
    
    sem_init(current_request_sem, 1, 0);
    sem_init(which_process_sem, 1, 1);
    sem_init(wait_response_sem, 1, 0);
    sem_init(clock_sem, 1, 1);
    
    DEBUG("Before calling bankers");
    bankers_algorithm(process_list);    

    free_processes(process_list, num_processes); 
    dealoc_shared_variables();
    munmap(avaliable, sizeof(int) * num_resources);

    return 0;
}
