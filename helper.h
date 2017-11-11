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

#endif
