#ifndef HELPER
    #define HELPER
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

void store_file_in_buffer(char * filename, char * buf)
{
    char cur;
    while(1)
        break;
}

#endif
