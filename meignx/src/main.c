#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "begin_request.h"
#include "fcgi.h"
#include "send_params.h"
#include "send_stdin.h"
#include "process_stdout.h"

static void handle_send_params_result(const SendParamsResult send_params_result)
{
    switch (send_params_result.tag)
    {
    case SEND_PARAMS_PROTOCOL_ERR:
        {
            const AddParamResult add_param_error = send_params_result.payload.protocol_error.previous;

            switch (add_param_error.tag)
            {
            case ADD_PARAM_LENGTH_TOO_LARGE:
                {
                    const FCGI_ParamComponent component = add_param_error.payload.length_error.component;

                    const char* component_name = component == FCGI_COMPONENT_NAME
                                                     ? "NAME"
                                                     : "VALUE";

                    fprintf(
                        stderr,
                        "[PROTOCOL ERROR] Failed to add FastCGI parameter.\n"
                        "  -> Reason: The %s length exceeds the maximum allowed limit of %zu bytes.\n",
                        component_name,
                        add_param_error.payload.length_error.max_size
                    );
                    break;
                }
            case ADD_PARAM_BUFFER_TOO_SMALL:
                fprintf(
                    stderr,
                    "[PROTOCOL ERROR] Insufficient internal buffer capacity while serializing parameters.\n"
                    "  -> Required: %zu bytes | Buffer capacity: %zu bytes.\n",
                    add_param_error.payload.buffer_error.min_buf_size,
                    add_param_error.payload.buffer_error.given_capacity
                );
                break;
            case ADD_PARAM_OK:
                break;
            }

            break;
        }
    case SEND_PARAMS_NETWORK_ERR:
        {
            const int sys_errno = send_params_result.payload.network_error.sys_errno;

            fprintf(
                stderr,
                "[NETWORK ERROR] Failed to send parameters through the socket.\n"
                "  -> Error Code: %d\n"
                "  -> Description: %s\n",
                sys_errno,
                strerror(sys_errno)
            );

            break;
        }
    case SEND_PARAMS_BODY_TOO_LARGE:
        {
            const size_t calculated_size = send_params_result.payload.body_error.calculated_size;
            const size_t max_size = send_params_result.payload.body_error.max_size;

            fprintf(
                stderr,
                "[LIMIT ERROR] The FCGI_PARAMS record body size is too large.\n"
                "  -> Calculated size: %zu bytes | Protocol maximum limit: %zu bytes.\n",
                calculated_size,
                max_size
            );

            break;
        }
    case SEND_PARAMS_OK:
        break;
    }
}

static void handle_request(const int fd)
{
    begin_request(fd);

    const SendParamsResult send_params_result = send_params(fd);

    handle_send_params_result(send_params_result);

    send_stdin(fd);
}

static int handle_response(const int fd)
{
    FCGI_Header header = {0};

    char buf[4096] = {0};

    ssize_t recvd;

    StdoutReponse out = {0};

    while ((recvd = recv(fd, buf, sizeof(buf), 0)) > 0)
    {
        if (recvd == -1)
        {
            perror("recv");
            return -1;
        }

        printf("recv returned: %zd\n", recvd);

        size_t param_offset = 0;

        do
        {
            header = (FCGI_Header){
                .version = (uint8_t)buf[param_offset + 0],
                .type = (uint8_t)buf[param_offset + 1],
                .requestIdB1 = (uint8_t)buf[param_offset + 2],
                .requestIdB0 = (uint8_t)buf[param_offset + 3],
                .contentLengthB1 = (uint8_t)buf[param_offset + 4],
                .contentLengthB0 = (uint8_t)buf[param_offset + 5],
                .paddingLength = (uint8_t)buf[param_offset + 6],
                .reserved = (uint8_t)buf[param_offset + 7],
            };

            param_offset += sizeof(header)
                + (
                    (header.contentLengthB1 << 8)
                    | (header.contentLengthB0 & 0xFF)
                )
                + header.paddingLength;

            int exit = 0;

            switch (header.type)
            {
            case FCGI_STDOUT:
                {
                    char* start = buf;
                    char* body = start + sizeof(header);

                    process_stdout(body, &out);

                    break;
                }

            case FCGI_END_REQUEST:
                {
                    exit = 1;
                    break;
                }

            default:
                fprintf(stderr, "Error while trying to parse request");
                exit = 1;
                break;
            }

            if (exit == 1)
            {
                break;
            }
        }
        while (1);
    }

    return 0;
}

static unsigned int parse_ip_address(char* ip_address)
{
    int i;
    char* str;

    unsigned int ip = 0;

    // from 3 byte shift left to 0, concatenating the address
    for (i = 3, str = ip_address; i >= 0; --i, str = nullptr)
    {
        const uint8_t octet = strtol(strtok(str, "."), nullptr, 10);

        ip |= octet << (i * 8);
    }

    return htonl(ip);
}

static uint16_t parse_port(const char* port)
{
    return htons(strtol(port, nullptr, 10));
}

static int connect_fcgi(const unsigned int ip, const uint16_t port)
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = port,
        .sin_addr = {
            .s_addr = ip,
        },
    };

    const int error_connect = connect(fd, (struct sockaddr*)&addr, sizeof(addr));

    if (error_connect == -1)
    {
        perror("connect");
        return -1;
    }

    return fd;
}

int main(const int argc, char* argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s <fcgi-host> <fcgi-port>", argv[0]);
        return EXIT_FAILURE;
    }

    const unsigned int ip = parse_ip_address(argv[1]);

    const uint16_t port = parse_port(argv[2]);

    const int fd = connect_fcgi(ip, port);

    if (fd == -1)
    {
        return errno;
    }

    handle_request(fd);

    handle_response(fd);

    close(fd);

    return 0;
}
