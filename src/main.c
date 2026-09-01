#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        (void) fprintf(stderr, "usage: %s <symbol>\n", argv[0]);
        return 1;
    }

    const char* query = argv[1];

    (void) printf("searching for: %s\n", query);

    FILE* pipe = popen("manpath", "r");
    if (pipe == NULL)
    {
        (void) fprintf(stderr, "jafm: failed to run manpath\n");
        return 1;
    }

    char buffer[4096];
    if (fgets(buffer, sizeof(buffer), pipe) == NULL)
    {
        (void) fprintf(stderr, "jafm: failed to read manpath\n");
        (void) pclose(pipe);
        return 1;
    }

    (void) pclose(pipe);

    buffer[strcspn(buffer, "\n")] = '\0';

    char* saveptr = NULL;
    for (char* dir = strtok_r(buffer, ":", &saveptr);
         dir != NULL;
         dir = strtok_r(NULL, ":", &saveptr))
        (void) printf("man dir: %s\n", dir);

    return 0;
}
