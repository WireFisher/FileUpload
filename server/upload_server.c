#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../client/protocol.h"


#define FILETAIL_TEMPLATE       ("UID:%10d,CHUNKID:%8x")
#define LEN_FILETAIL_TEMPLATE   (13+10+8)

struct Fileinfo
{
    unsigned int uid;
    long filesize;
};


static inline int send_ack(int sock, unsigned int chunk_id)
{
    char buf[LEN_RESUME_TEMPLATE_ACK+1];

    snprintf(buf, LEN_RESUME_TEMPLATE_ACK, RESUME_TEMPLATE_ACK, chunk_id);
    if(write(sock, buf, LEN_RESUME_TEMPLATE_ACK) < 0)
        return -1;
    return 0;
}


static inline void write_filetail(FILE *fp, unsigned int uid,  unsigned int chunk_id)
{
    char buf[LEN_FILETAIL_TEMPLATE+1];

    snprintf(buf, LEN_FILETAIL_TEMPLATE, FILETAIL_TEMPLATE, uid, chunk_id);
    fwrite(buf, 1, LEN_FILETAIL_TEMPLATE, fp);
}


int recv_uploadings(int sock)
{
    char buf[LEN_RESUME_TEMPLATE+1] = "";
    unsigned int uid, chunk_id, file_uid, total_chunk_num;
    char checksum[33] = "";
    long file_size;
    char filepath[128], filepath2[128];
    FILE *fp;
    unsigned int i;
    char *accepted_content;

    if(read(sock, buf, LEN_RESUME_TEMPLATE) != LEN_RESUME_TEMPLATE)
        return -1;

    if(sscanf(buf, RESUME_TEMPLATE, &uid, checksum, &file_size) != 3)
        return -1;

    if(uid == 0)
        return -1;

    snprintf(filepath, 128, "cache/%s", checksum);
    snprintf(filepath2, 128, "uploading/%s", checksum);
    if(access(filepath, F_OK) == 0) {
        char buf[LEN_FILETAIL_TEMPLATE+1];

        rename(filepath, filepath2);
        fp = fopen(filepath2, "r");
        fseek(fp, -LEN_FILETAIL_TEMPLATE, SEEK_END);
        fread(buf, 1, LEN_FILETAIL_TEMPLATE, fp);
        fclose(fp);
        sscanf(buf, FILETAIL_TEMPLATE, &file_uid, &chunk_id);
        if(file_uid != uid)
            chunk_id = 0;
        fp = fopen(filepath2, "a");
        fseek(fp, -LEN_FILETAIL_TEMPLATE, SEEK_END);
    }
    else {
        chunk_id = 0;
        fp = fopen(filepath2, "a");
    }

    if(send_ack(sock, chunk_id) < 0)
        return -1;

    accepted_content = malloc(UPLOAD_CHUNK_SIZE);

    total_chunk_num = (file_size + UPLOAD_CHUNK_SIZE - 1) / UPLOAD_CHUNK_SIZE;
    for(i = chunk_id; i < total_chunk_num; i++) {
        char excepted_head[LEN_CHUNK_HEAD_TEMPLATE+1] = "";
        int real_accepted_size = 0;

        snprintf(excepted_head, LEN_CHUNK_HEAD_TEMPLATE, CHUNK_HEAD_TEMPLATE, i);
        if(read(sock, buf, LEN_CHUNK_HEAD_TEMPLATE) != LEN_CHUNK_HEAD_TEMPLATE)
            break;
        if(strncmp(excepted_head, buf, LEN_CHUNK_HEAD_TEMPLATE) != 0)
            break;
        real_accepted_size = read(sock, accepted_content, UPLOAD_CHUNK_SIZE);
        if(real_accepted_size < 0)
            break;
        if(i != total_chunk_num - 1 && real_accepted_size != UPLOAD_CHUNK_SIZE)
            break;
        fwrite(accepted_content, 1, real_accepted_size, fp);
    }

    if(i < total_chunk_num)
        write_filetail(fp, uid, i);
    else
        snprintf(filepath, 128, "finish/%s", checksum);

    fclose(fp);
    free(accepted_content);
    rename(filepath2, filepath);

    if(i < total_chunk_num)
        return -1;
    else
        return 0;
}

int run_server(const char *base_dir, const char *listening_ip, int port)
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
        server_addr.sin_addr.s_addr = inet_addr(listening_ip);
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

int main()
{
    run_server("", "0.0.0.0", 4399);
}
