#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "picture.h"
#include "version.h"
#define UTILS_NAME "opuspictotag"

const char *version = UTILS_NAME " version " OPUSCOMMENT_VERSION "\n" OPUSCOMMENT_URL "\n";

const char *usage = "Usage: " UTILS_NAME " [OPTIONS] FILE{.jpg,.png,.gif}\n";

const char *help =
    "Options:\n"
    "  -h, --help              print this help\n"
    "  -o, --output FILE       write tag to a file (default=stdout)\n";

struct option options[] = {
    {"help", no_argument, 0, 'h'},
    {"output", required_argument, 0, 'o'},
    {NULL, 0, 0, 0}
};

int main(int argc, char **argv)
{
    if(argc == 1)
    {
        fputs(version, stdout);
        fputs(usage, stdout);
        return EXIT_SUCCESS;
    }
    char *path_picture = NULL, *picture_data = NULL, *path_out = NULL;
    const char *error_message;
    int print_help = 0;
    int c;
    while((c = getopt_long(argc, argv, "ho:", options, NULL)) != -1)
    {
        switch(c){
            case 'h':
                print_help = 1;
                break;
            case 'o':
                path_out = optarg;
                break;
            default:
                return EXIT_FAILURE;
        }
    }
    if(print_help)
    {
        puts(version);
        puts(usage);
        puts(help);
        puts("See the man page for extensive documentation.");
        return EXIT_SUCCESS;
    }
    if(optind != argc - 1)
    {
        fputs("invalid arguments\n", stderr);
        return EXIT_FAILURE;
    }
    path_picture = argv[optind];
    if(path_picture != NULL)
    {
        int seen_file_icons=0;
        picture_data = opustags_picture_specification_parse(path_picture, &error_message, &seen_file_icons);
        if(picture_data == NULL)
        {
            fprintf(stderr,"Not read picture: %s\n", error_message);
        }
    }
    FILE *out = NULL;
    if(path_out != NULL)
    {
        char canon_picture[PATH_MAX+1], canon_out[PATH_MAX+1];
        if(realpath(path_picture, canon_picture) && realpath(path_out, canon_out))
        {
            if(strcmp(canon_picture, canon_out) == 0)
            {
                fputs("error: the input and output files are the same\n", stderr);
                return EXIT_FAILURE;
            }
        }
        out = fopen(path_out, "w");
        if(!out)
        {
            perror("fopen");
            return EXIT_FAILURE;
        }
    } else {
        out = stdout;
    }
    if(picture_data != NULL)
    {
        char *picture_tag = "METADATA_BLOCK_PICTURE";
        char *picture_meta = NULL;
        int picture_meta_len = strlen(picture_tag) + strlen(picture_data) + 1;
        picture_meta = malloc(picture_meta_len);
        if(picture_meta == NULL)
        {
            fprintf(stderr,"Bad picture size: %d\n", picture_meta_len);
            free(picture_meta);
        } else {
            strcpy(picture_meta, picture_tag);
            strcat(picture_meta, "=");
            strcat(picture_meta, picture_data);
            strcat(picture_meta, "\0");
        }
        fwrite(picture_meta, 1, picture_meta_len, out);
        free(picture_meta);
    }
    if(out)
        fclose(out);
    return EXIT_SUCCESS;
}
