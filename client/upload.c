#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/md5.h>
//#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include "protocol.h"
#include "ikcp.h"


int create_udp_connect(const char *server_ip, int server_port, int udp_port_bind)
{
    // servaddr_
	struct sockaddr_in servaddr_;
    {
        servaddr_.sin_family = AF_INET;
        servaddr_.sin_port = htons(server_port);
        int ret = inet_pton(AF_INET, server_ip, &servaddr_.sin_addr);
        if (ret <= 0)
        {
            if (ret < 0) // errno set
                perror("inet_pton error return < 0, with errno: ");
            else
                perror("inet_pton error return 0");
            return -1;
        }
    }

    // create udp_socket_
    int udp_socket_;
    {
        udp_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket_ < 0)
        {
            perror("socket error return with errno: ");
            return -1;
        }
    }

    // set socket recv timeout
    /*{
        struct timeval recv_timeo;
        recv_timeo.tv_sec = 0;
        recv_timeo.tv_usec = 2 * 1000; // 2 milliseconds
        int ret = setsockopt(udp_socket_, SOL_SOCKET, SO_RCVTIMEO, &recv_timeo, sizeof(recv_timeo));
        if (ret < 0)
        {
            perror("setsockopt error return with errno: ");
        }
    }*/

    // set socket non-blocking
    {
        int flags = fcntl(udp_socket_, F_GETFL, 0);
        if (flags == -1)
        {
            perror("get socket non-blocking: fcntl error return with errno: ");
            return -1;
        }
        int ret = fcntl(udp_socket_, F_SETFL, flags | O_NONBLOCK);
        if (ret == -1)
        {
            perror("set socket non-blocking: fcntl error return with errno: ");
            return -1;
        }
    }

    // set recv buf bigger

    // bind
    if (udp_port_bind != 0)
    {
        struct sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(udp_port_bind);
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        int ret_bind = bind(udp_socket_, (const struct sockaddr*)(&bind_addr), sizeof(bind_addr));
        if (ret_bind < 0)
            perror("setsockopt error return with errno: ");
    }

    // udp connect
    {
        int ret = connect(udp_socket_, (const struct sockaddr*)(&servaddr_), sizeof(servaddr_));
        if (ret < 0)
        {
            perror("connect error return with errno: ");
            return -1;
        }
    }

    return udp_socket_;
}


void send_udp_package(int udp_socket_, const char *buf, int len)
{
    const ssize_t send_ret = send(udp_socket_, buf, len, 0);
    if (send_ret < 0)
    {
        perror("send_udp_package error with errno: ");
    }
    else if (send_ret != len)
    {
        perror("send_udp_package error: not all packet send. ");
    }
}


int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    send_udp_package(*(int*)user,buf, len);
	return 0;
}


void init_kcp(kcp_conv_t conv)
{
    p_kcp_ = ikcp_create(conv, (void*)this);
    p_kcp_->output = &udp_output;

    // 启动快速模式
    // 第二个参数 nodelay-启用以后若干常规加速将启动
    // 第三个参数 interval为内部处理时钟，默认设置为 10ms
    // 第四个参数 resend为快速重传指标，设置为2
    // 第五个参数 为是否禁用常规流控，这里禁止
    //ikcp_nodelay(p_kcp_, 1, 10, 2, 1);
    ikcp_nodelay(p_kcp_, 1, 2, 1, 1); // 设置成1次ACK跨越直接重传, 这样反应速度会更快. 内部时钟5毫秒.
}


static inline int read_file_into_buf(const char *file, char **buf, long *filesize)
{
    FILE *fp;

    fp = fopen(file, "rb");
    fseek(fp, 0, SEEK_END);
    *filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    *buf = malloc(*filesize);
    if(fread(*buf, 1, *filesize, fp) < *filesize) {
        perror("fread error.\n");
        return -1;
    }
    fclose(fp);
    return 0;
}


static inline void md5checksum(const char *plain, long length, char checksum[33])
{
    unsigned char digest[16];
    MD5_CTX ctx;
    int i;

    MD5_Init(&ctx);
    MD5_Update(&ctx, plain, length);
    MD5_Final(digest, &ctx);

    for(i = 0; i < 16; i++)
        sprintf(&checksum[i*2], "%02x", (unsigned int)digest[i]);
    checksum[32] = '\0';
}


int send_file_info(int sock, unsigned int uid, char checksum[32], long filesize, unsigned int *resume_id)
{
    char buf[LEN_RESUME_TEMPLATE+1] = "";

    snprintf(buf, LEN_RESUME_TEMPLATE+1, RESUME_TEMPLATE, uid, checksum, filesize);
    //printf("===================\n");
    //write(1, buf, LEN_RESUME_TEMPLATE);
    //printf("\n===================\n");
    if(write(sock, buf, LEN_RESUME_TEMPLATE) < 0)
        return -1;
    if(read(sock, buf, LEN_RESUME_TEMPLATE_ACK) < 0)
        return -1;
    if(sscanf(buf, RESUME_TEMPLATE_ACK, resume_id) != 1)
        return -1;
    return 0;
}


int send_chunk_head(int sock, unsigned int chunk_id)
{
    char buf[LEN_CHUNK_HEAD_TEMPLATE+1] = "";

    snprintf(buf, LEN_CHUNK_HEAD_TEMPLATE+1, CHUNK_HEAD_TEMPLATE, chunk_id);
    if(write(sock, buf, LEN_CHUNK_HEAD_TEMPLATE) < 0)
        return -1;
    return 0;
}


int send_chunk_body(int sock, char *file_buf, long file_size, unsigned i)
{
    if(i == (file_size + UPLOAD_CHUNK_SIZE - 1) / UPLOAD_CHUNK_SIZE - 1) {
        if(write(sock, file_buf + i * UPLOAD_CHUNK_SIZE, file_size%UPLOAD_CHUNK_SIZE) != file_size % UPLOAD_CHUNK_SIZE)
            return -1;
        else
            return 0;
    }

    if(write(sock, file_buf + i * UPLOAD_CHUNK_SIZE, UPLOAD_CHUNK_SIZE) < 0)
        return -1;
    else
        return 0;
}


int upload(const char *file_name, const char *dest_ip, int port, unsigned int uid)
{
    int sock;
    struct sockaddr_in dest_addr;
    long file_size;
    char *file_buf;
    char checksum[33];
    

    read_file_into_buf(file_name, &file_buf, &file_size);
    md5checksum(file_buf, file_size, checksum);
    printf("File MD5: %s\n", checksum);
    printf("File Size: %ld\n", file_size);

    unsigned int i, total_chunk_num, reconnect_count, resume_id;
    total_chunk_num = (file_size + UPLOAD_CHUNK_SIZE - 1) / UPLOAD_CHUNK_SIZE;
    //printf("Total_chunk_num: %u\n", total_chunk_num);


    i = reconnect_count = 0;
    while(i < total_chunk_num && reconnect_count < 20) {
        sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(sock < 0) {
            perror("socket error.\n");
            return -1;
        }

        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = inet_addr(dest_ip);
        dest_addr.sin_port = htons(port);
        /* connect */
        reconnect_count ++;
        //gets(checksum);
        if(connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
            perror("Connecting to remote server failed, waiting for reconnecting.\n");
            if(reconnect_count > 10) {
                perror("Failed to connect to remote server. Please check the network.\n");
                break;
            }
            sleep(5);
            continue;
        }

        /* ask where to start */
        if(send_file_info(sock, uid, checksum, file_size, &resume_id) < 0) {
            close(sock);
            continue;
        }

        //printf("starting from %uth chunk\n", resume_id);

        /* upload */
        for(i = resume_id; i < total_chunk_num; i++) {
            //printf("sending %uth chunk head\n", i);
            if(send_chunk_head(sock, i) < 0)
                break;
            //printf("sending %uth chunk body\n", i);
            if(send_chunk_body(sock, file_buf, file_size, i) < 0)
                break;
        }
        close(sock);
    }

    free(file_buf);

    if(i < total_chunk_num) {
        printf("Upload failed, %u/%u\n", i, total_chunk_num);
        return -1;
    }

    printf("Upload done, %u/%u\n", i, total_chunk_num);
    return 0;
}

//int main(int argc, char* argv[])
//{
//    upload(argv[1], "127.0.0.1", 4399, 1);
//}

int main()
{
    int udp_sock;
    udp_sock = create_udp_connect("127.0.0.1", 4399, 0);
    init_kcp(1);
}
