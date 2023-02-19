#include <stdio.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char**argv)
{
    if(argc != 3)
    {
        printf("Error. The number of arguments must be 2 \n");
        return 1;
    }

    openlog(NULL,0,LOG_USER);

    char *file_name = argv[1];
    char *string_to_write = argv[2];

    FILE *fp = NULL;
    fp = fopen(file_name, "w");
    if(fp == NULL)
    {
        //LOG
        syslog(LOG_ERR, "Error. The file %s cannot be open", file_name);
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", string_to_write, file_name);
    size_t nr_bytes = fwrite(string_to_write, 1, strlen(string_to_write), fp);
    if(nr_bytes == 0)
    {
        syslog(LOG_ERR, "Error. No bytes written to the file");
        return 1;
    }

    return 0;
}