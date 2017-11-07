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
#endif
