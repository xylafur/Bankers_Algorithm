#include <stdio.h>
#include <ctype.h>
#include "process.h"
#include "manager.h"
#include "helper.h"
#include <string.h>

#define MAX_STR_LEN 1024 

/*  memory maps our global variables that need to be shared across all processes
 */
void make_variables_shared()
{

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


/*  Our global, shared varaibles */
int clock = 0;
int last_request = -1;

int main()
{
    int i;
    char * filename = "sample-input1.txt";
    int num_processes = 0, num_resources = 0;
    long offset = get_proc_and_resc(filename, &num_processes, &num_resources);
    
    //populate avaliabnle resources
    int avaliable [num_resources];//this also needs to be shared
    offset = get_avaliable_resources(filename, num_resources, avaliable, offset);

    process_t process_list [num_processes];
    offset = populate_process_needed(filename, num_processes, process_list, 
                                     num_resources, avaliable, offset);

    printf("processes: %d\tresources: %d\n", num_processes, num_resources);
    printf("avaliable: ");
    for(i = 0; i < num_resources; i++)
        printf("%d ", avaliable[i]);
    printf("\n");

    free_processes(process_list, num_processes); 
    return 0;
}
