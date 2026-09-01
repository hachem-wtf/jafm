#include <dirent.h>
#include <zlib.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define LINE_SIZE 8192

static bool ends_with(const char* string, const char* suffix)
{
    size_t string_length = strlen(string);
    size_t suffix_length = strlen(suffix);

    if (suffix_length > string_length)
        return false;

    return strcmp(string + string_length - suffix_length, suffix) == 0;
}

static bool plain_page_contains(const char* path, const char* query)
{
    FILE* page = fopen(path, "r");
    if (page == NULL)
        return false;

    char line[LINE_SIZE];
    bool found = false;
    while (fgets(line, sizeof(line), page) != NULL)
        if (strstr(line, query) != NULL)
        {
            found = true;
            break;
        }

    (void) fclose(page);
    return found;
}

static bool gzip_page_contains(const char* path, const char* query)
{
    gzFile page = gzopen(path, "r");
    if (page == NULL)
        return false;

    char line[LINE_SIZE];
    bool found = false;
    while (gzgets(page, line, LINE_SIZE) != NULL)
        if (strstr(line, query) != NULL)
        {
            found = true;
            break;
        }

    (void) gzclose(page);
    return found;
}

static bool page_contains(const char* path, const char* query)
{
    if (ends_with(path, ".gz"))
        return gzip_page_contains(path, query);

    return plain_page_contains(path, query);
}

static void list_pages(const char* section, const char* query)
{
    DIR* section_stream = opendir(section);
    if (section_stream == NULL)
        return;

    for (const struct dirent* entry = readdir(section_stream);
         entry != NULL;
         entry = readdir(section_stream))
        if (entry->d_name[0] != '.')
        {
            char path[4096];
            (void) snprintf(path, sizeof(path), "%s/%s", section, entry->d_name);
            if (page_contains(path, query))
                (void) printf("    match: %s\n", path);
        }

    (void) closedir(section_stream);
}

static void list_sections(const char* directory, const char* query)
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
            list_pages(section, query);
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
        list_sections(directory, query);
    }

    return 0;
}
