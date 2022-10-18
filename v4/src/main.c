#include "conversion.h"
#include "copy.h"
#include "error.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


struct options
{
    char *file_name;
    char *ip_in;
    char *ip_out;
    in_port_t port_in;
    in_port_t port_out;
    int fd_in;
    int fd_in2;
    int fd_out;
};


static void options_init(struct options *opts);
static void parse_arguments(int argc, char *argv[], struct options *opts);
static void options_process(struct options *opts);
static void cleanup(const struct options *opts);


#define BUF_SIZE 1024
#define DEFAULT_PORT 5000
#define BACKLOG 5

int main(int argc, char *argv[])
{
    struct options opts;

    options_init(&opts);
    parse_arguments(argc, argv, &opts);
    options_process(&opts);
    copy(opts.fd_in, opts.fd_out, BUF_SIZE);
    cleanup(&opts);

    return EXIT_SUCCESS;
}


static void options_init(struct options *opts)
{
    memset(opts, 0, sizeof(struct options));
    opts->fd_in    = STDIN_FILENO;
    opts->fd_out   = STDOUT_FILENO;
    opts->port_in  = DEFAULT_PORT;
    opts->port_out = DEFAULT_PORT;
}


static void parse_arguments(int argc, char *argv[], struct options *opts)
{
    int c;

    while((c = getopt(argc, argv, ":i:o:p:P:")) != -1)   // NOLINT(concurrency-mt-unsafe)
    {
        switch(c)
        {
            case 'i':
            {
                opts->ip_in = optarg;
                break;
            }
            case 'o':
            {
                opts->ip_out = optarg;
                break;
            }
            case 'p':
            {
                opts->port_in = parse_port(optarg, 10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                break;
            }
            case 'P':
            {
                opts->port_out = parse_port(optarg, 10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                break;
            }
            case ':':
            {
                fatal_message(__FILE__, __func__ , __LINE__, "\"Option requires an operand\"", 5); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                break;
            }
            case '?':
            {
                fatal_message(__FILE__, __func__ , __LINE__, "Unknown", 6); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
            }
            default:
            {
                assert("should not get here");
            };
        }
    }

    if(optind < argc)
    {
        opts->file_name = argv[optind];
    }
}


static void options_process(struct options *opts)
{
    if(opts->file_name && opts->ip_in)
    {
        fatal_message(__FILE__, __func__ , __LINE__, "Can't pass -i and a filename", 2);
    }

    if(opts->file_name)
    {
        opts->fd_in = open(opts->file_name, O_RDONLY);

        if(opts->fd_in == -1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }
    }

    if(opts->ip_in)
    {
        struct sockaddr_in addr;
        int result;
        int option;

        opts->fd_in2 = socket(AF_INET, SOCK_STREAM, 0);

        if(opts->fd_in2 == -1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons(opts->port_in);
        addr.sin_addr.s_addr = inet_addr(opts->ip_in);

        if(addr.sin_addr.s_addr ==  (in_addr_t)-1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }

        option = 1;
        setsockopt(opts->fd_in2, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

        result = bind(opts->fd_in2, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

        if(result == -1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }

        result = listen(opts->fd_in2, BACKLOG);

        if(result == -1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }

        opts->fd_in = accept(opts->fd_in2, NULL, 0);

        if(opts->fd_in == -1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }
    }

    if(opts->ip_out)
    {
        int result;
        struct sockaddr_in addr;

        opts->fd_out = socket(AF_INET, SOCK_STREAM, 0);

        if(opts->fd_out == -1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons(opts->port_out);
        addr.sin_addr.s_addr = inet_addr(opts->ip_out);

        if(addr.sin_addr.s_addr ==  (in_addr_t)-1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }

        result = connect(opts->fd_out, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

        if(result == -1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }
    }
}


static void cleanup(const struct options *opts)
{
    if(opts->file_name)
    {
        close(opts->fd_in);
    }
    else if(opts->ip_in)
    {
        close(opts->fd_in);
        close(opts->fd_in2);
    }

    if(opts->ip_out)
    {
        close(opts->fd_out);
    }
}
