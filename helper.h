#ifndef HELPER
    #define HELPER

#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>

/*  Our global, shared varaibles */
int * clock = 0;
int * last_request = 0;
int * which_process = 0;    /*whos turn is it to run*/
int * avaliable = 0;            /*size */
int * current_request = 0;      /*size n + 1, represents either request 
                                  or release and the ammount*/
process_t * process_list;

sem_t *current_request_sem, *wait_response_sem, *clock_sem, *which_process_sem,
      *process_finished_sem;

int num_resources, num_processes;

int string_to_int(char * str)
{
    int val = 0, ii = 0;
    while(str[ii] != '\0' && str[ii] != '\n'){
        val *= 10;
        val += str[ii] - '0';
        ii++;
    }
    return val;
}

void make_variables_shared()
{
    clock = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, 
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0); 
    last_request = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, 
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
    process_finished_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, 
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
    munmap(process_finished_sem, sizeof(sem_t));
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

/*  Finds the next valid shortest process, returns -1 if all processes are 
 *  finished
 */
int find_shortest(int ignore)
{
    int shortest = -1, i;
    for(i = 0; i < num_processes; i++){
        if(process_list[i].finished)
            continue;
        if(ignore >= 0 && i == ignore)
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

void subtract_list(int * list1, int * list2, int size)
{
    int i;
    for(i = 0; i < size; i++)
        list1[i] -= list2[i];
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

void set_which_process(int value)
{
    sem_wait(which_process_sem);
    *which_process = value;
    sem_post(which_process_sem); 
}

int get_which_process()
{
    int value;
    sem_wait(which_process_sem);
    value = *which_process;
    sem_post(which_process_sem); 
    return value;
}

/*  Function called by children processes, moves a request into current_request
 */
void fill_out_request(process_t * self)
{
    int i;
    current_request[0] = (int)self->actions[self->next_action].action; 
    if((action_e)current_request[0] == REQUEST || 
       (action_e)current_request[0] == RELEASE){
        for(i = 0; i < self->num_resources; i++)
            current_request[i+1] = self->actions[self->next_action].resources[i];
    }
}

void increment_clock(process_t * self)
{
    sem_wait(clock_sem);
    printf("\nincreasing clock by %d\n", self->actions[self->next_action].computation_time);

    *clock += self->actions[self->next_action].computation_time;
    sem_post(clock_sem);
}

int get_clock()
{   
    int value;
    sem_wait(clock_sem);
    value = *clock;
    sem_post(clock_sem);
    return value;
}

/*  This is a function that figures out how to increment or decrement allocated
 *  resources for a process based on the command it just ran
 */
void handle_allocated_resources(process_t * self)
{
    action_e action = self->actions[self->next_action].action;
    int i;
    if(action == REQUEST){
    /*increment all our allocated by request*/
        for(i = 0; i < self->num_resources; i++)
            self->allocated_resources[i] += \
                    self->actions[self->next_action].resources[i];
    }else if(action == RELEASE){
    /*decrement all our allocarted by release*/
        for(i = 0; i < self->num_resources; i++)
            self->allocated_resources[i] -= \
                    self->actions[self->next_action].resources[i];
    }
}


void print_process(int id)
{
    int i, clock_time;
    printf("Process #%d\n\tFinished: %d\tPerforming action #%d\n", 
           id+1, process_list[id].finished, process_list[id].next_action);
    printf("Allocated resources: [");
    for(i = 0; i < num_resources; i++)
        printf("%d, ", process_list[id].allocated_resources[i]);
    printf("]\n");
    clock_time = get_clock();
    printf("This process has a deadline of: %d\n", process_list[id].deadline);
    if(clock_time > process_list[id].deadline)
        printf("\tThis process has missed it's deadline\n");
    else
        printf("\tThis process has not missed its deadline at this time\n");
}

/*  Function for the system output.
 */
void print_system_state()
{
    int i;
    sem_wait(clock_sem);
    printf("Clock is at: %d\n", *clock);     
    sem_post(clock_sem);

    printf("Avaliable: [");
    for(i=0; i < num_resources; i++)
        printf("%d, ", avaliable[i]);
    printf("]\n"); 
    
    for(i = 0; i < num_processes; i++)
        print_process(i);
}

#endif
