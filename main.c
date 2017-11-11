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


void run_child_process(int id)
{
    process_t * self;
    self = &process_list[id];
    int choice;
    while(!self->finished){
        choice = get_which_process(); 
        if(choice == id){
            DEBUG_CHILD("This process got selected")
            /*now the parent is waiting on us to fill out our request*/
            fill_out_request(self);            
            sem_post(current_request_sem);
            DEBUG_CHILD("Filled out our request, waiting for response")
            /*We have made our request, now we wait for the parent's response*/
            sem_wait(wait_response_sem);
            if(current_request[0] == 0){
                DEBUG_CHILD("our request completed succesfully")
                increment_clock(self);  
                self->time_computed += \
                    self->actions[self->next_action].computation_time;
                handle_allocated_resources(self);
                self->next_action++;
                if(self->next_action >= self->num_actions){
                    self->finished = 1;
                    self->finishing_time = get_clock();
                }
            }
            DEBUG_CHILD("Process finished, signalling\n")
            /*need to signal to the parent that we are done for now at least*/
            sem_post(process_finished_sem);
        }
    }
    DEBUG_CHILD("Exiting child process") 
    exit(0);
}


void SPF_bankers(process_t * process_list)
{

    int pid, i;
    *which_process = -1;
    *clock = 0;
    for(i=0; i < num_processes; i++){//spawn all child processes
        pid = fork(); 
        if(pid != 0)
            break;
    }
    if(pid == 0){
        int least = least_laxity(-1), st, temp_least;
        while(least >= 0){
            if(all_processes_finished())
                break;
            set_which_process(least); 
            sem_wait(current_request_sem);
            /*at this point the child has made a request and is waiting on us
              to respond    */
            st = parent_handle_request();
            if(st)
                current_request[0] = 0;
            else{
                DEBUG_PARENT("Hit invalid request, finding next shortest")
                temp_least = least_laxity(least);
                if(temp_least >= 0){
                    DEBUG_PARENT("Found another valid process to run")
                    least = temp_least;
                    current_request[0] = -1;
                }else{
                    DEBUG_PARENT("could not find another valid process") 
                    current_request[0] = 0;
                }
            }
            set_which_process(-1);
            /*tell the child our response is ready*/
            sem_post(wait_response_sem);
            /*wait for the child process to finish all its shit*/
            sem_wait(process_finished_sem);
            print_system_state();
            /*if this was a valid request, we need to find the next process*/
            if(st)
                least = least_laxity(-1);
        }
        DEBUG_PARENT("Ourside of loop, about to exit");
    }else{
        run_child_process(i); 
    }
}

void LLF_bankers(process_t * process_list)
{
    int pid, i;
    *which_process = -1;
    *clock = 0;
    for(i=0; i < num_processes; i++){//spawn all child processes
        pid = fork(); 
        if(pid != 0)
            break;
    }
    if(pid == 0){
        int shortest = find_shortest(-1), st, temp_shortest;
        while(shortest >= 0){
            if(all_processes_finished())
                break;
            set_which_process(shortest); 
            sem_wait(current_request_sem);
            /*at this point the child has made a request and is waiting on us
              to respond    */
            st = parent_handle_request();
            if(st)
                current_request[0] = 0;
            else{
                DEBUG_PARENT("Hit invalid request, finding next shortest")
                temp_shortest = find_shortest(shortest);
                if(temp_shortest >= 0){
                    DEBUG_PARENT("Found another valid process to run")
                    shortest = temp_shortest;
                    current_request[0] = -1;
                }else{
                    DEBUG_PARENT("could not find another valid process") 
                    current_request[0] = 0;
                }
            }
            set_which_process(-1);
            /*tell the child our response is ready*/
            sem_post(wait_response_sem);
            /*wait for the child process to finish all its shit*/
            sem_wait(process_finished_sem);
            print_system_state();
            /*if this was a valid request, we need to find the next process*/
            if(st)
                shortest = find_shortest(-1);
        }
        DEBUG_PARENT("Ourside of loop, about to exit");
    }else{
        run_child_process(i); 
    }

}

int main(int argc, char * argv [])
{
    if(argc < 2){
        printf("ERROR: Need to provide input file to program");
        exit(1);
    }
    char * filename = argv[1]; 
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

    printf("Select which algorothm to run.  1: SPF, 2: LL");
    char in;
    scanf("%c", &in);

    if(in - '0' == 1)
        SPF_bankers(process_list);    
    else
        LLF_bankers(process_list);

    free_processes(process_list, num_processes); 
    dealoc_shared_variables();
    munmap(avaliable, sizeof(int) * num_resources);

    printf("\nAll processes have finished\n\tSimulation over\n");

    DEBUG_PARENT("EXITING PROGRAM")

    exit(0);
    return 0;
}
