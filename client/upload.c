#include <stdio.h>
#include <sys/socket.h>
#include <string.h>


void send_file(int fd);

int upload(const char *filename, const char *dest_ip, int port)
{
    int sock;
    struct sockaddr_in dest_addr;

    if(sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP) < 0) {
        QUIT("socket error.\n");
        return -1;
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(dest_ipv);
    dest_addr.sin_port = htons(port);

    if(connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("connect error.\n");
        return -1;
    }

    send_file(sock);

    close(sock);
    return 0;
}


void send_file(int fd)
{
}
