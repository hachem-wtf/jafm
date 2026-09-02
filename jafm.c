#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <zlib.h>

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_SIZE 8192
#define MAX_VISIBLE 12
#define SYMBOL_SIZE 128
#define KIND_SIZE 16

#define TIER_BODY       1
#define TIER_SYNOPSIS   2
#define TIER_DEFINITION 3
#define TIER_NAME       4

#define WEIGHT_FILENAME   1000000
#define WEIGHT_NAME       5000
#define WEIGHT_DEFINITION 2000
#define WEIGHT_SYNOPSIS   50
#define WEIGHT_BODY       1

struct Match
{
    char* path;
    long score;
    char symbol[SYMBOL_SIZE];
    char kind[KIND_SIZE];
};

struct MatchList
{
    struct Match* items;
    size_t count;
    size_t capacity;
};

static bool match_list_add(struct MatchList* matches, const char* path, long score,
                           const char* symbol, const char* kind)
{
    if (matches->count == matches->capacity)
    {
        size_t new_capacity = matches->capacity == 0 ? 16 : matches->capacity * 2;
        struct Match* grown =
            realloc(matches->items, new_capacity * sizeof(struct Match));
        if (grown == NULL)
            return false;

        matches->items = grown;
        matches->capacity = new_capacity;
    }

    char* copy = strdup(path);
    if (copy == NULL)
        return false;

    struct Match* item = &matches->items[matches->count];
    item->path = copy;
    item->score = score;
    (void) snprintf(item->symbol, sizeof(item->symbol), "%s", symbol);
    (void) snprintf(item->kind, sizeof(item->kind), "%s", kind);
    matches->count++;
    return true;
}

static void match_list_free(struct MatchList* matches)
{
    for (size_t index = 0; index < matches->count; index++)
        free(matches->items[index].path);

    free(matches->items);
}

static int compare_matches(const void* left, const void* right)
{
    const struct Match* first = left;
    const struct Match* second = right;

    if (first->score != second->score)
        return first->score > second->score ? -1 : 1;

    return strcmp(first->path, second->path);
}

static bool ends_with(const char* string, const char* suffix)
{
    size_t string_length = strlen(string);
    size_t suffix_length = strlen(suffix);

    if (suffix_length > string_length)
        return false;

    return strcmp(string + string_length - suffix_length, suffix) == 0;
}

static bool is_section_header(const char* line)
{
    return strncmp(line, ".SH", 3) == 0 || strncmp(line, ".Sh", 3) == 0;
}

static bool is_definition_line(const char* line)
{
    if (strchr(line, '{') == NULL)
        return false;

    return strstr(line, "struct") != NULL || strstr(line, "union") != NULL
           || strstr(line, "enum") != NULL || strstr(line, "typedef") != NULL;
}

static long count_occurrences(const char* line, const char* query)
{
    size_t query_length = strlen(query);
    if (query_length == 0)
        return 0;

    long count = 0;
    for (const char* cursor = strstr(line, query);
         cursor != NULL;
         cursor = strstr(cursor + query_length, query))
        count++;

    return count;
}

static bool is_identifier_char(char character)
{
    return isalnum((unsigned char) character) || character == '_';
}

static void extract_symbol(const char* line, const char* query, char* symbol,
                           size_t symbol_size)
{
    const char* found = strstr(line, query);
    if (found == NULL)
        return;

    const char* start = found;
    while (start > line && is_identifier_char(start[-1]))
        start--;

    const char* end = found + strlen(query);
    while (is_identifier_char(*end))
        end++;

    size_t length = (size_t) (end - start);
    if (length >= symbol_size)
        length = symbol_size - 1;

    memcpy(symbol, start, length);
    symbol[length] = '\0';
}

static void classify_kind(const char* line, const char* symbol, char* kind,
                          size_t kind_size)
{
    const char* word = NULL;
    if (strstr(line, "struct") != NULL)
        word = "struct";
    else if (strstr(line, "union") != NULL)
        word = "union";
    else if (strstr(line, "enum") != NULL)
        word = "enum";
    else if (strstr(line, "typedef") != NULL)
        word = "typedef";

    if (word == NULL && symbol[0] != '\0')
    {
        const char* after = strstr(line, symbol);
        if (after != NULL)
        {
            after += strlen(symbol);
            while (*after == ' ' || *after == '\t')
                after++;
            if (*after == '(')
                word = "function";
        }
    }

    (void) snprintf(kind, kind_size, "%s", word != NULL ? word : "symbol");
}

struct Scan
{
    bool in_name;
    bool in_synopsis;
    long name_hits;
    long definition_hits;
    long synopsis_hits;
    long body_hits;
    int best_tier;
    char symbol[SYMBOL_SIZE];
    char kind[KIND_SIZE];
};

struct Finding
{
    long score;
    char symbol[SYMBOL_SIZE];
    char kind[KIND_SIZE];
};

static void scan_line(struct Scan* scan, const char* line, const char* query)
{
    if (is_section_header(line))
    {
        scan->in_name = strstr(line, "NAME") != NULL;
        scan->in_synopsis = strstr(line, "SYNOPSIS") != NULL;
        return;
    }

    long hits = count_occurrences(line, query);
    if (hits == 0)
        return;

    int tier;
    if (scan->in_name || strncmp(line, ".Nm", 3) == 0)
    {
        scan->name_hits += hits;
        tier = TIER_NAME;
    }
    else if (is_definition_line(line))
    {
        scan->definition_hits += hits;
        tier = TIER_DEFINITION;
    }
    else if (scan->in_synopsis)
    {
        scan->synopsis_hits += hits;
        tier = TIER_SYNOPSIS;
    }
    else
    {
        scan->body_hits += hits;
        tier = TIER_BODY;
    }

    if (tier > scan->best_tier)
    {
        scan->best_tier = tier;
        extract_symbol(line, query, scan->symbol, sizeof(scan->symbol));
        classify_kind(line, scan->symbol, scan->kind, sizeof(scan->kind));
    }
}

static long scan_score(const struct Scan* scan)
{
    return scan->name_hits * WEIGHT_NAME + scan->definition_hits * WEIGHT_DEFINITION
           + scan->synopsis_hits * WEIGHT_SYNOPSIS + scan->body_hits * WEIGHT_BODY;
}

static void plain_page_scan(const char* path, const char* query, struct Scan* scan)
{
    FILE* page = fopen(path, "r");
    if (page == NULL)
        return;

    char line[LINE_SIZE];
    while (fgets(line, sizeof(line), page) != NULL)
        scan_line(scan, line, query);

    (void) fclose(page);
}

static void gzip_page_scan(const char* path, const char* query, struct Scan* scan)
{
    gzFile page = gzopen(path, "r");
    if (page == NULL)
        return;

    char line[LINE_SIZE];
    while (gzgets(page, line, LINE_SIZE) != NULL)
        scan_line(scan, line, query);

    (void) gzclose(page);
}

static void page_find(const char* path, const char* query, struct Finding* finding)
{
    struct Scan scan = {0};
    if (ends_with(path, ".gz"))
        gzip_page_scan(path, query, &scan);
    else
        plain_page_scan(path, query, &scan);

    finding->score = scan_score(&scan);
    (void) snprintf(finding->symbol, sizeof(finding->symbol), "%s", scan.symbol);
    (void) snprintf(finding->kind, sizeof(finding->kind), "%s", scan.kind);
}

static long section_weight(const char* section)
{
    const char* name = strrchr(section, '/');
    name = name != NULL ? name + 1 : section;
    if (strncmp(name, "man", 3) == 0)
        name += 3;

    switch (name[0])
    {
        case '0':
        case '2':
        case '4':
        case '5':
        case '7':
        case '9':
            return 8;
        default:
            return 1;
    }
}

static bool filename_matches(const char* name, const char* query)
{
    char base[256];
    (void) snprintf(base, sizeof(base), "%s", name);

    size_t length = strlen(base);
    if (length > 3 && strcmp(base + length - 3, ".gz") == 0)
        base[length - 3] = '\0';

    char* section_dot = strrchr(base, '.');
    if (section_dot != NULL)
        *section_dot = '\0';

    return strcmp(base, query) == 0;
}

static struct termios original_termios;

static void disable_raw_mode(void)
{
    (void) tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

static bool enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1)
        return false;

    struct termios raw = original_termios;
    raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        return false;

    return true;
}

static void terminal_size(size_t* rows, size_t* cols)
{
    struct winsize window;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 && window.ws_row > 0)
    {
        *rows = window.ws_row;
        *cols = window.ws_col > 0 ? window.ws_col : 80;
    }
    else
    {
        *rows = 24;
        *cols = 80;
    }
}

static void page_label(const char* path, char* label, size_t label_size)
{
    const char* slash = strrchr(path, '/');
    const char* name = slash != NULL ? slash + 1 : path;

    char base[256];
    (void) snprintf(base, sizeof(base), "%s", name);

    size_t length = strlen(base);
    if (length > 3 && strcmp(base + length - 3, ".gz") == 0)
        base[length - 3] = '\0';

    char* section_dot = strrchr(base, '.');
    if (section_dot != NULL)
    {
        *section_dot = '\0';
        (void) snprintf(label, label_size, "%s(%s)", base, section_dot + 1);
    }
    else
        (void) snprintf(label, label_size, "%s", base);
}

static size_t render_menu(const struct MatchList* matches, size_t highlight,
                          size_t top, size_t visible, size_t cols,
                          size_t previous_height)
{
    if (previous_height > 0)
        (void) printf("\x1b[%zuA", previous_height);

    size_t height = 0;

    (void) printf("\r\x1b[2K  \x1b[1;36mjafm\x1b[0m \x1b[2m%zu matches\x1b[0m\r\n",
                  matches->count);
    height++;

    (void) printf("\x1b[2K\r\n");
    height++;

    size_t end = top + visible;
    if (end > matches->count)
        end = matches->count;

    for (size_t index = top; index < end; index++)
    {
        char label[256];
        page_label(matches->items[index].path, label, sizeof(label));

        const char* symbol = matches->items[index].symbol;
        const char* kind = matches->items[index].kind;

        char detail[SYMBOL_SIZE + KIND_SIZE + 32];
        if (symbol[0] != '\0' && kind[0] != '\0')
            (void) snprintf(detail, sizeof(detail),
                            "  \x1b[33m%s\x1b[0m \x1b[2m%s\x1b[0m", symbol, kind);
        else if (symbol[0] != '\0')
            (void) snprintf(detail, sizeof(detail), "  \x1b[33m%s\x1b[0m", symbol);
        else
            detail[0] = '\0';

        if (index == highlight)
            (void) printf("\x1b[2K  \x1b[36m\xe2\x9d\xaf\x1b[0m \x1b[1m%s\x1b[0m%s\r\n",
                          label, detail);
        else
            (void) printf("\x1b[2K    \x1b[2m%s\x1b[0m%s\r\n", label, detail);

        height++;
    }

    const char* path = matches->items[highlight].path;
    size_t path_length = strlen(path);
    size_t budget = cols > 4 ? cols - 4 : 0;
    bool truncated = path_length > budget;
    const char* shown = truncated ? path + (path_length - budget + 1) : path;

    (void) printf("\x1b[2K\r\n");
    height++;

    (void) printf("\x1b[2K  \x1b[2m%s%s\x1b[0m\r\n", truncated ? "\xe2\x80\xa6" : "", shown);
    height++;

    (void) printf("\x1b[2K  \x1b[2mup/down move  enter open  q quit  [%zu/%zu]\x1b[0m\r\n",
                  highlight + 1, matches->count);
    height++;

    (void) printf("\x1b[J");
    return height;
}

static bool select_interactive(const struct MatchList* matches, size_t* selected)
{
    if (!enable_raw_mode())
        return false;

    size_t highlight = 0;
    size_t top = 0;
    size_t previous_height = 0;
    bool chosen = false;
    bool done = false;

    while (!done)
    {
        size_t rows = 0;
        size_t cols = 0;
        terminal_size(&rows, &cols);

        size_t reserved = 5;
        size_t visible = rows > reserved ? rows - reserved : 1;
        if (visible > MAX_VISIBLE)
            visible = MAX_VISIBLE;

        if (highlight < top)
            top = highlight;
        else if (highlight >= top + visible)
            top = highlight - visible + 1;

        previous_height = render_menu(matches, highlight, top, visible, cols,
                                      previous_height);

        char key = 0;
        if (read(STDIN_FILENO, &key, 1) != 1)
            break;

        if (key == '\r' || key == '\n')
        {
            chosen = true;
            done = true;
        }
        else if (key == 'q' || key == 3)
            done = true;
        else if (key == 'j')
        {
            if (highlight + 1 < matches->count)
                highlight++;
        }
        else if (key == 'k')
        {
            if (highlight > 0)
                highlight--;
        }
        else if (key == '\x1b')
        {
            char sequence[2];
            if (read(STDIN_FILENO, &sequence[0], 1) == 1
                && read(STDIN_FILENO, &sequence[1], 1) == 1
                && sequence[0] == '[')
            {
                if (sequence[1] == 'B' && highlight + 1 < matches->count)
                    highlight++;
                else if (sequence[1] == 'A' && highlight > 0)
                    highlight--;
            }
        }
    }

    if (previous_height > 0)
        (void) printf("\x1b[%zuA\r\x1b[J", previous_height);

    disable_raw_mode();

    if (chosen)
        *selected = highlight;

    return chosen;
}

static bool read_selection(size_t count, size_t* selected)
{
    (void) printf("select [1-%zu]: ", count);

    char input[64];
    if (fgets(input, sizeof(input), stdin) == NULL)
        return false;

    char* end = NULL;
    long value = strtol(input, &end, 10);
    if (end == input || value < 1 || (size_t) value > count)
        return false;

    *selected = (size_t) value - 1;
    return true;
}

static void list_pages(const char* section, const char* query, struct MatchList* matches)
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

            struct Finding finding = {0};
            page_find(path, query, &finding);

            long score = finding.score * section_weight(section);
            if (filename_matches(entry->d_name, query))
            {
                score += WEIGHT_FILENAME;
                if (finding.symbol[0] == '\0')
                    (void) snprintf(finding.symbol, sizeof(finding.symbol), "%s",
                                    query);
            }

            if (score > 0)
                (void) match_list_add(matches, path, score, finding.symbol,
                                      finding.kind);
        }

    (void) closedir(section_stream);
}

static void list_sections(const char* directory, const char* query, struct MatchList* matches)
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
            list_pages(section, query, matches);
        }

    (void) closedir(directory_stream);
}

static void dedupe_matches(struct MatchList* matches)
{
    if (matches->count == 0)
        return;

    char (*labels)[256] = malloc(matches->count * sizeof(*labels));
    if (labels == NULL)
        return;

    size_t write = 0;
    for (size_t read = 0; read < matches->count; read++)
    {
        char label[256];
        page_label(matches->items[read].path, label, sizeof(label));

        bool seen = false;
        for (size_t index = 0; index < write; index++)
            if (strcmp(labels[index], label) == 0)
            {
                seen = true;
                break;
            }

        if (seen)
            free(matches->items[read].path);
        else
        {
            (void) snprintf(labels[write], sizeof(labels[write]), "%s", label);
            matches->items[write] = matches->items[read];
            write++;
        }
    }

    free(labels);
    matches->count = write;
}

static int open_page(const char* path)
{
    char work[4096];
    (void) snprintf(work, sizeof(work), "%s", path);

    size_t length = strlen(work);
    if (length > 3 && strcmp(work + length - 3, ".gz") == 0)
        work[length - 3] = '\0';

    char* name_slash = strrchr(work, '/');
    if (name_slash == NULL)
        return 1;

    *name_slash = '\0';
    char* name = name_slash + 1;

    char* section_dot = strrchr(name, '.');
    if (section_dot != NULL)
        *section_dot = '\0';

    char* root_slash = strrchr(work, '/');
    if (root_slash == NULL)
        return 1;

    *root_slash = '\0';
    char* root = work;
    char* section = root_slash + 1;
    if (strncmp(section, "man", 3) == 0)
        section += 3;

    (void) execlp("man", "man", "-M", root, section, name, (char*) NULL);

    (void) fprintf(stderr, "jafm: failed to run man\n");
    return 1;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        (void) fprintf(stderr, "usage: %s <symbol>\n", argv[0]);
        return 1;
    }

    const char* query = argv[1];

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

    struct MatchList matches = {0};

    char* save_pointer = NULL;
    for (const char* directory = strtok_r(buffer, ":", &save_pointer);
         directory != NULL;
         directory = strtok_r(NULL, ":", &save_pointer))
        list_sections(directory, query, &matches);

    if (matches.count == 0)
    {
        (void) fprintf(stderr, "jafm: no matches for %s\n", query);
        match_list_free(&matches);
        return 1;
    }

    qsort(matches.items, matches.count, sizeof(struct Match), compare_matches);
    dedupe_matches(&matches);

    size_t selected = 0;
    bool has_selection;
    if (isatty(STDIN_FILENO))
        has_selection = select_interactive(&matches, &selected);
    else
    {
        for (size_t index = 0; index < matches.count; index++)
        {
            char label[256];
            page_label(matches.items[index].path, label, sizeof(label));

            const char* symbol = matches.items[index].symbol;
            const char* kind = matches.items[index].kind;
            if (symbol[0] != '\0')
                (void) printf("%zu) %s  %s (%s)\n", index + 1, label, symbol, kind);
            else
                (void) printf("%zu) %s\n", index + 1, label);
        }
        has_selection = read_selection(matches.count, &selected);
    }

    if (!has_selection)
    {
        (void) fprintf(stderr, "jafm: no selection\n");
        match_list_free(&matches);
        return 1;
    }

    char chosen[4096];
    (void) snprintf(chosen, sizeof(chosen), "%s", matches.items[selected].path);

    match_list_free(&matches);
    return open_page(chosen);
}
