#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define PATH_MAX 128

struct stat path_stat;

int is_directory(char *path)
{
    memset(&path_stat, 0, sizeof(path_stat));
    stat(path, &path_stat);

    return S_ISDIR(path_stat.st_mode);
}

int main(int argc, char *argv[])
{
    FILE *fp;
    char file_name[PATH_MAX], ch; 
    int i;

    if (argc == 1) {
        int rc = 0;
        while ((rc = read(0, file_name, PATH_MAX)) > 0)
            write(1, file_name, rc);

        exit(rc);
    }

    for(i = 1; i < argc; i++) {
        strncpy(file_name, argv[i], PATH_MAX);
        if (is_directory(file_name)) {
            printf("%s: %s\n", argv[0], strerror(EISDIR));
            return 1;
        }
        fp = fopen(file_name, "r");
        if (fp == NULL) {
            perror(argv[0]);
            return 1;
        }

        while((ch = fgetc(fp)) != EOF){
            putchar(ch);
        }

        fclose(fp);
    }
    return 0;
}
