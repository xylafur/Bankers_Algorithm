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
            DEBUG_CHILD("This process got selected")printf("\tprocess: %d\n", id);
            /*now the parent is waiting on us to fill out our request*/
            fill_out_request(self);            
            sem_post(current_request_sem);
            DEBUG_CHILD("Filled out our request, waiting for response")
            /*We have made our request, now we wait for the parent's response*/
            sem_wait(wait_response_sem);
            if(current_request[0] == 0){
                DEBUG_CHILD("our request completed succesfully")
                increment_clock(self);  
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
    exit(0);
}

/*  Returns a bool wether or not this is a valid request
 */
int check_request_valid()
{
    if(*last_request == *which_process)
        return 0;
    int i;
    for(i = 0; i < num_resources; i++)
        if(avaliable[i] < current_request[i+1])
            return 0;
    return 1;
}

void perform_request()
{
    int i;
    for(i = 0; i < num_resources; i++)
        avaliable[i] -= current_request[i+1];
    *last_request = *which_process;
}

void perform_release()
{
    int i;
    for(i = 0; i < num_resources; i++)
        avaliable[i] += current_request[i+1];
}

/*  Fucntion called by parent process, determinecs if the request is a valid 
 *  one.  If it is it will make the request. 
 *  Returns the status of the request, 1 for success 0 for failure
 */
int parent_handle_request()
{
    int st;
    DEBUG_PARENT("Figuring out how to handle current request")
    action_e action = (action_e)current_request[0];
    switch(action){
        case REQUEST:
            if(check_request_valid()){
                DEBUG_PARENT("We have marked this as a valid request")
                perform_request();
                st = 1;                
            }else{
                DEBUG_PARENT("That was an invalid request")
                st = 0;
            } 
            break;
        case RELEASE:
            DEBUG_PARENT("Performing a release")
            perform_release();
            st = 1; 
            break;
        default:
            st = 1;
    }
    return st;
}

int all_processes_finished()
{
    int i;
    for(i = 0; i < num_processes; i++)
        if(!process_list[i].finished)
            return 0;
    return 1;
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
        int shortest = find_shortest(-1), st, temp_shortest;
        while(shortest >= 0){
            //print_system_state();
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
            if(all_processes_finished())
                break;
        }
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
 
    SPF_bankers(process_list);    

    free_processes(process_list, num_processes); 
    dealoc_shared_variables();
    munmap(avaliable, sizeof(int) * num_resources);

    return 0;
}
