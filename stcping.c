#include <netdb.h>
/*
    getaddrinfo()
    struct addrinfo
*/
#include <string.h>
/*
    memset()
*/
#include <arpa/inet.h>
/*
    inet_ntop()
*/
#include <stdio.h>
/*
    perror()
*/
#include <fcntl.h>
/*
	fcntl()
*/
#include <sys/time.h>
/*
    struct timeval
*/
#include <errno.h>
/*
    errno
*/
#include <sys/select.h>
/*
    select()
*/
#include <stdlib.h>
/*
    atoi()
*/
#include <unistd.h>
/*
    close()
*/

#define THOUSAND 1000U

struct addrinfo*
get_resolve_list(char* domain, int version_flag)
{
    struct addrinfo* result;
    struct addrinfo  hints;

    if (domain == NULL)
        return NULL;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_CANONNAME;
    if (version_flag == AF_INET)
        hints.ai_family = AF_INET;
    else if (version_flag == AF_INET6)
        hints.ai_family = AF_INET6;
    else
        hints.ai_family = AF_UNSPEC;

    if (getaddrinfo(domain, NULL, &hints, &result) != 0) {
        perror("getaddrinfo");
        return NULL;
    }

    return result;
}

int
set_non_block(int sockfd)
{
	int flags;

	if ((flags = fcntl(sockfd, F_GETFL, 0)) < 0) {
		perror("fcntl get flags");
		return -1;
	}
	flags |= O_NONBLOCK;
	if (fcntl(sockfd, F_SETFL, flags) < 0) {
		perror("fcntl set non_block");
		return -1;
	}
	return 0;
}

int
tcping6(struct sockaddr* servaddr)
{
    int                 n;
    int                 error;
    int                 sockfd;
    char                addr[INET6_ADDRSTRLEN];
    socklen_t           len;
    fd_set              rset;
    fd_set              wset;
    struct timeval      timeout;
    struct timeval      before;
    struct timeval      after;
    struct sockaddr_in6*    servaddr_in6;

    servaddr_in6 = (struct sockaddr_in6*)servaddr;

    if ((sockfd = socket(AF_INET6, SOCK_STREAM, 0)) <= 0) {
        perror("tcping6.socket");
        return 1;
    }

    if (set_non_block(sockfd) != 0) {
        close(sockfd);
        return 1;
    }

    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    wset = rset;

    memset(addr, 0, INET6_ADDRSTRLEN);

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if (gettimeofday(&before, NULL)) {
        perror("tcping6.gettimeofday(&before, NULL)");
        close(sockfd);
        return 1;
    }

    if ((n = connect(sockfd, servaddr, sizeof(struct sockaddr_in6))) < 0) {
        if (errno != EINPROGRESS) {
            perror("connect");
            return 1;
        }
    }

    // connect immediately
    if (n == 0) {
        if (gettimeofday(&after, NULL)) {
            perror("tcping6.gettimeofday(&after, NULL)");
            close(sockfd);
            return 1;
        }
        fprintf(stderr, "response from %s:%d %ld ms\n", inet_ntop(AF_INET6, &servaddr_in6->sin6_addr, addr, INET6_ADDRSTRLEN),
                ntohs(servaddr_in6->sin6_port), THOUSAND * (after.tv_sec - before.tv_sec) + (after.tv_usec - before.tv_usec));
    }
    // not yet connect
    else {
        // timeout
        if ((n = select(sockfd + 1, &rset, &wset, NULL, &timeout)) == 0) {
            close(sockfd);
            if (gettimeofday(&after, NULL)) {
                perror("tcping6.gettimeofday(&after, NULL)");
                fprintf(stderr, "Timeout %ld ms\n", THOUSAND * timeout.tv_sec);
                return 1;
            }
            fprintf(stderr, "Timeout %ld ms\n", THOUSAND * (after.tv_sec - before.tv_sec)
                    + (after.tv_usec - before.tv_usec));
            return 1;
        }
        // connect or error
        if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
            // get time first
            if (gettimeofday(&after, NULL)) {
                perror("tcping4.gettimeofday(&after, NULL)");
                close(sockfd);
                return 1;
            }

            // check if error occurs
            len = sizeof(int);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                perror("tcping6.getsockopt()");
                close(sockfd);
                return 1;
            }

            // error occurs
            if (error) {
                fprintf(stderr, "error %s\n", strerror(error));
                close(sockfd);
            }
            // no error, connect successfully
            else {
                if (gettimeofday(&after, NULL)) {
                    perror("tcping6.gettimeofday(&after, NULL)");
                    close(sockfd);
                    return 1;
                }

                fprintf(stderr, "response from %s:%d %.2f ms\n", inet_ntop(AF_INET6, &servaddr_in6->sin6_addr, addr, INET6_ADDRSTRLEN),
                        ntohs(servaddr_in6->sin6_port), THOUSAND * (after.tv_sec - before.tv_sec) + (float)(after.tv_usec - before.tv_usec) / THOUSAND);
            }
        }
        else {
            fprintf(stderr, "unknow error\n");
            close(sockfd);
            return 1;
        }
    }
    return 0;
}


int
tcping4(struct sockaddr* servaddr)
{
    int                 n;
    int                 error;
    int                 sockfd;
    char                addr[INET_ADDRSTRLEN];
    socklen_t           len;
    fd_set              rset;
    fd_set              wset;
    struct timeval      timeout;
    struct timeval      before;
    struct timeval      after;
    struct sockaddr_in* servaddr_in;

    servaddr_in = (struct sockaddr_in*)servaddr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
        perror("tcping4.socket");
        return 1;
    }

    if (set_non_block(sockfd) != 0) {
        close(sockfd);
        return 1;
    }

    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    wset = rset;

    memset(addr, 0, INET_ADDRSTRLEN);

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if (gettimeofday(&before, NULL)) {
        perror("tcping4.gettimeofday(&before, NULL)");
        close(sockfd);
        return 1;
    }

    if ((n = connect(sockfd, servaddr, sizeof(struct sockaddr))) < 0) {
        if (errno != EINPROGRESS) {
            perror("connect");
            return 1;
        }
    }

    // connect immediately
    if (n == 0) {
        if (gettimeofday(&after, NULL)) {
            perror("tcping4.gettimeofday(&after, NULL)");
            close(sockfd);
            return 1;
        }
        fprintf(stderr, "response from %s:%d %ld ms\n", inet_ntop(AF_INET, &servaddr_in->sin_addr, addr, INET_ADDRSTRLEN),
                ntohs(servaddr_in->sin_port), THOUSAND * (after.tv_sec - before.tv_sec) + (after.tv_usec - before.tv_usec));
    }
    // not yet connect
    else {
        // timeout
        if ((n = select(sockfd + 1, &rset, &wset, NULL, &timeout)) == 0) {
            close(sockfd);
            if (gettimeofday(&after, NULL)) {
                perror("tcping4.gettimeofday(&after, NULL)");
                fprintf(stderr, "Timeout %ld ms\n", THOUSAND * timeout.tv_sec);
                return 1;
            }
            fprintf(stderr, "Timeout %ld ms\n", THOUSAND * (after.tv_sec - before.tv_sec)
                    + (after.tv_usec - before.tv_usec));
            return 1;
        }
        // connect or error
        if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
            // get time first
            if (gettimeofday(&after, NULL)) {
                perror("tcping4.gettimeofday(&after, NULL)");
                close(sockfd);
                return 1;
            }

            // check if error occurs
            len = sizeof(int);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                perror("tcping4.getsockopt()");
                close(sockfd);
                return 1;
            }

            // error occurs
            if (error) {
                fprintf(stderr, "error %s\n", strerror(error));
                close(sockfd);
            }
            // no error, connect successfully
            else {
                if (gettimeofday(&after, NULL)) {
                    perror("tcping4.gettimeofday(&after, NULL)");
                    close(sockfd);
                    return 1;
                }

                fprintf(stderr, "response from %s:%d %.2f ms\n", inet_ntop(AF_INET, &servaddr_in->sin_addr, addr, INET_ADDRSTRLEN),
                        ntohs(servaddr_in->sin_port), THOUSAND * (after.tv_sec - before.tv_sec) + (float)(after.tv_usec - before.tv_usec) / THOUSAND);
            }
        }
        else {
            fprintf(stderr, "unknow error\n");
            close(sockfd);
            return 1;
        }
    }
    return 0;
}

int
prepare_tcping(struct addrinfo* item, int port)
{
    struct sockaddr_in      sinaddr;
    struct sockaddr_in6     sinaddr6;

    memset(&sinaddr, 0, sizeof(struct sockaddr_in));
    memset(&sinaddr6, 0, sizeof(struct sockaddr_in6));

    if (item->ai_family == AF_INET) {
        memcpy(&sinaddr, item->ai_addr, sizeof(struct sockaddr_in));
        sinaddr.sin_port = htons(port);
        return tcping4((struct sockaddr*)&sinaddr);
    }
    else if (item->ai_family == AF_INET6) {
        memcpy(&sinaddr6, item->ai_addr, sizeof(struct sockaddr_in6));
        sinaddr6.sin6_port = htons(port);
        return tcping6((struct sockaddr*)&sinaddr6);
    }

    return 1;
}

int
main(int argc, char** argv)
{
    int port;
    int af_family;
    struct addrinfo* p;
    struct addrinfo* res;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s {domain} [port] [4/6]\n", argv[0]);
        return 1;
    }

    if (argv[2] != NULL)
        port = atoi(argv[2]);
    else
        port = 80;

    if (argv[3] != NULL) {
        af_family = atoi(argv[3]);
        if (af_family == 4)
            res = get_resolve_list(argv[1], AF_INET);
        else if (af_family == 6)
            res = get_resolve_list(argv[1], AF_INET6);
        else
            res = get_resolve_list(argv[1], AF_UNSPEC);
    }
    else
        res = get_resolve_list(argv[1], AF_UNSPEC);
    if (res == NULL) {
        return 1;
    }

    for (p = res; p != NULL; p = p->ai_next) {

        while (prepare_tcping(p, port) == 0)
            sleep(2);
    }

    freeaddrinfo(res);
    return 0;
}