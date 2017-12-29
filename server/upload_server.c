#include <stdio.h>
#include <sys/socket.h>
#include <sting.h>


void recv_uploadings(int fd);

int run_server(const char *save_dir, const char *listening_ip, const char *port)
{
    int listen_sock;
    struct sockaddr_in server_addr;

    if(listen_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP) < 0) {
        perror("socket error.\n");
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if(listening_ip)
        server_addr.sin_addr.s_addr = htonl(listening_ip);
    else
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int on = 1;
    if(setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        perror("set socket option error.\n");
        return -1;
    }

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind error");
        return -1;
    }

    if (listen(listen_sock, SOMAXCONN) < 0) {
        perror("listen error");
        return -1;
    }

    struct sockaddr_in peeraddr;
    socklen_t peerlen;
    int conn;
    pid_t pid;

    while (1)
    {
        peerlen = sizeof(peeraddr);
        if ((conn = accept(listen_sock, (struct sockaddr *)&peeraddr, &peerlen)) < 0) {
            perror("accept error");
            return -1;
        }
        printf("recv connect ip=%s port=%d\n", inet_ntoa(peeraddr.sin_addr),
               ntohs(peeraddr.sin_port));

        pid = fork();
        if (pid == -1) {
            perror("fork error");
            return -1;
        }
        if (pid == 0)
        {
            // 子进程
            //close(listen_sock);
            recv_uploadings(conn);
            close(conn);
            exit(EXIT_SUCCESS);
        }
        else
            close(conn); //父进程
    }

    return 0;
}

void recv_uploadings(int fd)
{
}
