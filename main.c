#include <stdio.h>
#include "process.h"
#include "manager.h"
#include "helper.h"
#include "debug.h"
#include "parsing.h"
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>


#define DEBUG_PARENT(MSG) printf("PARENT: %s\n", MSG)
#define DEBUG_CHILD(MSG) printf("CHILD: %s\n", MSG)


void run_child_process(int id)
{
    process_t * self;
    self = &process_list[id];

    while(!self->finished){ 
        sem_wait(which_process_sem);
        int chosen = *which_process; 
        sem_post(which_process_sem);
        if(chosen == id && !process_list[id].finished){
            DEBUG_CHILD("RUNNING THIS PROCESS");printf("\tprocess: %d\taction: %d\n", id, self->next_action);
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
            DEBUG_CHILD("POSTED CURRENT REQUEST, WAITING ON RESPONSE");

            sem_wait(wait_response_sem);/*waits until the parent has responded*/
            DEBUG_CHILD("GOT RESPONSE FROM PARENT");
            if(current_request[i] != 0){
                /*our action was run succesfully by parent*/    
                sem_wait(clock_sem);
                clock += self->actions[self->next_action].computation_time;
                sem_post(clock_sem);
                self->next_action++;
            }
            if(self->next_action >= self->num_actions)
                self->finished = 1;
            sem_post(process_finished_sem);
            DEBUG_CHILD("FINISHED RUNNING CHILD");printf("\tchild: %d\n", id);
        }
        //if this process_t is done, we can kill the actual linux process
        if(process_list[id].finished)
            exit(0);
    }
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
        DEBUG_PARENT("IN PARENT AT BEGINNING");
        int shortest = find_shortest(-1), i;
        while(shortest >= 0){
            printf("PARENT BEGINNING shortest: %d\n", shortest);

            /*let the process know its his turn*/
            sem_wait(which_process_sem);
            *which_process = shortest;
            sem_post(which_process_sem);

            DEBUG_PARENT("WAITING FOR CURRENT REQUEST");
            /*wait for the child to make it's request*/
            sem_wait(current_request_sem);
            DEBUG_PARENT("AFTER WAITING FOR CURRENT REQUEST");
            action_e action = (action_e)current_request[0];
            int temp_shortest;
            DEBUG_PARENT("BEFORE PARENT SWITCH");printf("action: %d\n", (int)action);
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
            *which_process = -1;
            sem_post(wait_response_sem);
            sem_wait(process_finished_sem);
            DEBUG_PARENT("OUTSIDE SWITCH");
            printf("PARENT ENDING: shortest: %d\n", shortest);
        }
        DEBUG_PARENT("OUTSIDE WHILE");
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
    sem_init(process_finished_sem, 1, 0);
    
    bankers_algorithm(process_list);    

    free_processes(process_list, num_processes); 
    dealoc_shared_variables();
    munmap(avaliable, sizeof(int) * num_resources);

    return 0;
}
