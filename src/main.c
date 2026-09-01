#include <dirent.h>
#include <stdio.h>
#include <string.h>

static void list_pages(const char* section)
{
    DIR* section_stream = opendir(section);
    if (section_stream == NULL)
        return;

    for (const struct dirent* entry = readdir(section_stream);
         entry != NULL;
         entry = readdir(section_stream))
        if (entry->d_name[0] != '.')
            (void) printf("    page: %s/%s\n", section, entry->d_name);

    (void) closedir(section_stream);
}

static void list_sections(const char* directory)
{
    DIR* directory_stream = opendir(directory);
    if (directory_stream == NULL)
        return;

    for (const struct dirent* entry = readdir(directory_stream);
         entry != NULL;
         entry = readdir(directory_stream))
        if (strncmp(entry->d_name, "man", 3) == 0)
        {
            char section[4096];
            (void) snprintf(section, sizeof(section), "%s/%s", directory, entry->d_name);
            list_pages(section);
        }

    (void) closedir(directory_stream);
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        (void) fprintf(stderr, "usage: %s <symbol>\n", argv[0]);
        return 1;
    }

    const char* query = argv[1];

    (void) printf("searching for: %s\n", query);

    FILE* manpath_pipe = popen("manpath", "r");
    if (manpath_pipe == NULL)
    {
        (void) fprintf(stderr, "jafm: failed to run manpath\n");
        return 1;
    }

    char buffer[4096];
    if (fgets(buffer, sizeof(buffer), manpath_pipe) == NULL)
    {
        (void) fprintf(stderr, "jafm: failed to read manpath\n");
        (void) pclose(manpath_pipe);
        return 1;
    }

    (void) pclose(manpath_pipe);

    buffer[strcspn(buffer, "\n")] = '\0';

    char* save_pointer = NULL;
    for (const char* directory = strtok_r(buffer, ":", &save_pointer);
         directory != NULL;
         directory = strtok_r(NULL, ":", &save_pointer))
    {
        (void) printf("man dir: %s\n", directory);
        list_sections(directory);
    }

    return 0;
}
