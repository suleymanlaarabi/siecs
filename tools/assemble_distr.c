#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_file(FILE *out, const char *path)
{
    FILE *in = fopen(path, "rb");
    if (!in) {
        perror(path);
        exit(EXIT_FAILURE);
    }

    char buffer[16384];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), in))) {
        if (fwrite(buffer, 1, count, out) != count) {
            perror("write");
            fclose(in);
            exit(EXIT_FAILURE);
        }
    }

    if (ferror(in)) {
        perror(path);
        fclose(in);
        exit(EXIT_FAILURE);
    }
    fclose(in);
}

static int is_dependency_include(const char *line)
{
    const char *headers[] = {
        "sireflect.h",
        "sijson.h",
        "sihttp.h"
    };

    for (size_t i = 0; i < sizeof(headers) / sizeof(headers[0]); i++) {
        if (strstr(line, "#include") && strstr(line, headers[i])) {
            return 1;
        }
    }
    return 0;
}

static void copy_dependency_source(FILE *out, const char *path)
{
    FILE *in = fopen(path, "rb");
    if (!in) {
        perror(path);
        exit(EXIT_FAILURE);
    }

    char line[16384];
    while (fgets(line, sizeof(line), in)) {
        if (!is_dependency_include(line)) {
            fputs(line, out);
        }
    }

    if (ferror(in)) {
        perror(path);
        fclose(in);
        exit(EXIT_FAILURE);
    }
    fclose(in);
}

static FILE *open_output(const char *path)
{
    FILE *out = fopen(path, "wb");
    if (!out) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    return out;
}

int main(int argc, char *argv[])
{
    if (argc != 10) {
        fprintf(stderr,
            "usage: %s config main.h meta.h rest.h main.c meta.c rest.c "
            "output.h output.c\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    FILE *header = open_output(argv[8]);
    fputs("#ifndef _POSIX_C_SOURCE\n"
          "#define _POSIX_C_SOURCE 200809L\n"
          "#endif\n\n",
        header);
    copy_file(header, argv[1]);
    fputs("\n#define SIECS_DISTR\n\n#if SIECS_HAS_REST\n", header);
    copy_file(header, argv[4]);
    fputs("\n#elif SIECS_HAS_META\n", header);
    copy_file(header, argv[3]);
    fputs("\n#endif\n\n", header);
    copy_file(header, argv[2]);
    fclose(header);

    FILE *source = open_output(argv[9]);
    fputs("#include \"siecs.h\"\n\n#if SIECS_HAS_REST\n", source);
    copy_dependency_source(source, argv[7]);
    fputs("\n#elif SIECS_HAS_META\n", source);
    copy_dependency_source(source, argv[6]);
    fputs("\n#endif\n\n", source);
    copy_file(source, argv[5]);
    fclose(source);

    return EXIT_SUCCESS;
}
