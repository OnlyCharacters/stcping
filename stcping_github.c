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
    getopt()
    close()
*/
#include <sys/types.h>
/*
    uint64_t
*/
#include <signal.h>
/*
    signal()
*/

#define THOUSAND 1000U

uint64_t ping_times         = 0;
uint64_t successful_times   = 0;

double avg             = 0;
double minimum         = 0;
double maximum         = 0;

int     port                = 80;
int     stop_times          = -1;
char    destination[INET6_ADDRSTRLEN];

struct addrinfo* res;

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

void
count(double* delay)
{
    successful_times++;
    if (ping_times == 0)
        minimum = avg = *delay;
    else
        avg = (avg + *delay) / 2;

    ping_times++;
    if (*delay < minimum)
        minimum = *delay;
    else if (*delay > maximum)
        maximum = *delay;

}

void
print_statistics()
{
    fprintf(stderr, "\nPing statistics for %s:%d\n", destination, port);
    fprintf(stderr, "\t%lu sent.\n", ping_times);
    // fprintf(stderr, "\t%lu successful, %lu failed. (%.2f%% fail)\n",
    //         successful_times, ping_times - successful_times,
    //         ((float)(ping_times - successful_times) / ping_times) * 100);
    fprintf(stderr, "\t%lu successful, %lu failed.\n",
            successful_times, ping_times - successful_times);
    fprintf(stderr, "\t%.2f%% fail.\n",
            ((float)(ping_times - successful_times) / ping_times) * 100);
    fprintf(stderr, "\tMinimum = %.2fms, Maximum = %.2fms, Average = %.2fms\n\n",
            minimum, maximum, avg);

    freeaddrinfo(res);
    exit(0);
}

int
tcping6(struct sockaddr* servaddr)
{
    int                 n;
    int                 error;
    int                 sockfd;
    double              delay;
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
            close(sockfd);
            return 1;
        }
    }

    // connect immediately
    if (n == 0) {
        close(sockfd);
        if (gettimeofday(&after, NULL)) {
            perror("tcping6.gettimeofday(&after, NULL)");
            return 1;
        }
        delay = THOUSAND * (after.tv_sec - before.tv_sec) + (float)(after.tv_usec - before.tv_usec) / THOUSAND;
        fprintf(stderr, "response from %s:%d %.2f ms\n", inet_ntop(AF_INET6, &servaddr_in6->sin6_addr, addr, INET6_ADDRSTRLEN),
                ntohs(servaddr_in6->sin6_port), delay);
        count(&delay);
    }
    // not yet connect
    else {
        // timeout
        if ((n = select(sockfd + 1, &rset, &wset, NULL, &timeout)) == 0) {
            close(sockfd);
            if (gettimeofday(&after, NULL)) {
                perror("tcping6.gettimeofday(&after, NULL)");
                fprintf(stderr, "Timeout %.2ld ms\n", THOUSAND * timeout.tv_sec);
                ping_times++;
                return 1;
            }
            fprintf(stderr, "Timeout %.2f ms\n", (THOUSAND * (after.tv_sec - before.tv_sec)
                    + (float)(after.tv_usec - before.tv_usec) / THOUSAND));
            ping_times++;
            return 0;
        }
        // connect or error
        if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
            // get time first
            if (gettimeofday(&after, NULL)) {
                close(sockfd);
                perror("tcping6.gettimeofday(&after, NULL)");
                ping_times++;
                return 1;
            }

            // check if error occurs
            len = sizeof(int);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                close(sockfd);
                perror("tcping6.getsockopt()");
                ping_times++;
                return 1;
            }

            // error occurs
            if (error) {
                close(sockfd);
                fprintf(stderr, "error %s\n", strerror(error));
                ping_times++;
                return 1;
            }
            // no error, connect successfully
            else {
                close(sockfd);
                if (gettimeofday(&after, NULL)) {
                    perror("tcping6.gettimeofday(&after, NULL)");
                    ping_times++;
                    return 1;
                }
                delay = THOUSAND * (after.tv_sec - before.tv_sec) + (float)(after.tv_usec - before.tv_usec) / THOUSAND;
                fprintf(stderr, "response from %s:%d %.2f ms\n", inet_ntop(AF_INET6, &servaddr_in6->sin6_addr, addr, INET6_ADDRSTRLEN),
                        ntohs(servaddr_in6->sin6_port), delay);
                count(&delay);
            }
        }
        else {
            close(sockfd);
            fprintf(stderr, "unknow error\n");
            ping_times++;
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
    double              delay;
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
            close(sockfd);
            return 1;
        }
    }

    // connect immediately
    if (n == 0) {
        close(sockfd);
        if (gettimeofday(&after, NULL)) {
            perror("tcping4.gettimeofday(&after, NULL)");
            ping_times++;
            return 1;
        }
        delay = THOUSAND * (after.tv_sec - before.tv_sec) + (float)(after.tv_usec - before.tv_usec) / THOUSAND;
        fprintf(stderr, "response from %s:%d %.2f ms\n", inet_ntop(AF_INET, &servaddr_in->sin_addr, addr, INET_ADDRSTRLEN),
                ntohs(servaddr_in->sin_port), delay);
        count(&delay);
    }
    // not yet connect
    else {
        // timeout
        if ((n = select(sockfd + 1, &rset, &wset, NULL, &timeout)) == 0) {
            close(sockfd);
            if (gettimeofday(&after, NULL)) {
                perror("tcping4.gettimeofday(&after, NULL)");
                fprintf(stderr, "Timeout %ld ms\n", THOUSAND * timeout.tv_sec);
                ping_times++;
                return 1;
            }
            fprintf(stderr, "Timeout %.2f ms\n", THOUSAND * (after.tv_sec - before.tv_sec)
                    + (float)(after.tv_usec - before.tv_usec) / THOUSAND);
            ping_times++;
            return 0;
        }
        // connect or error
        if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
            // get time first
            if (gettimeofday(&after, NULL)) {
                close(sockfd);
                perror("tcping4.gettimeofday(&after, NULL)");
                ping_times++;
                return 1;
            }

            // check if error occurs
            len = sizeof(int);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                close(sockfd);
                perror("tcping4.getsockopt()");
                ping_times++;
                return 1;
            }

            // error occurs
            if (error) {
                close(sockfd);
                fprintf(stderr, "error %s\n", strerror(error));
                ping_times++;
                return 1;
            }
            // no error, connect successfully
            else {
                close(sockfd);
                if (gettimeofday(&after, NULL)) {
                    perror("tcping4.gettimeofday(&after, NULL)");
                    ping_times++;
                    return 1;
                }
                delay = THOUSAND * (after.tv_sec - before.tv_sec) + (float)(after.tv_usec - before.tv_usec) / THOUSAND;
                fprintf(stderr, "response from %s:%d %.2f ms\n", inet_ntop(AF_INET, &servaddr_in->sin_addr, addr, INET_ADDRSTRLEN),
                        ntohs(servaddr_in->sin_port), delay);
                count(&delay);
            }
        }
        else {
            close(sockfd);
            fprintf(stderr, "unknow error\n");
            ping_times++;
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
    memset(&destination, 0, INET6_ADDRSTRLEN);

    signal(SIGINT, print_statistics);

    if (item->ai_family == AF_INET) {
        memcpy(&sinaddr, item->ai_addr, sizeof(struct sockaddr_in));
        sinaddr.sin_port = htons(port);
        inet_ntop(AF_INET, &sinaddr.sin_addr, destination, INET_ADDRSTRLEN);
        if (stop_times == -1) {
            for ( ; ; ) {
                tcping4((struct sockaddr*)&sinaddr);
                sleep(2);
            }
        }
        else {
            for (int i = 0; i < stop_times; i++) {
                tcping4((struct sockaddr*)&sinaddr);
                if (i < stop_times - 1)
                    sleep(2);
            }
            print_statistics();
        }
    }
    else if (item->ai_family == AF_INET6) {
        memcpy(&sinaddr6, item->ai_addr, sizeof(struct sockaddr_in6));
        sinaddr6.sin6_port = htons(port);
        inet_ntop(AF_INET6, &sinaddr6.sin6_addr, destination, INET6_ADDRSTRLEN);
        if (stop_times == -1) {
            for ( ; ; ) {
                tcping6((struct sockaddr*)&sinaddr6);
                sleep(2);
            }
        }
        else {
            for (int i = 0; i < stop_times; i++) {
                tcping6((struct sockaddr*)&sinaddr6);
                if (i < stop_times - 1)
                    sleep(2);
            }
            print_statistics();
        }
    }

    return 1;
}

void
print_help()
{
    fprintf(stderr, "\nUsage\n  stcping [options] IP/Domain Port\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -c <count>\tstop after <count> replies\n");
    fprintf(stderr, "  -4\t\tuse IPv4\n");
    fprintf(stderr, "  -6\t\tuse IPv6\n");
    fprintf(stderr, "  -h\t\tprint help and exit\n\n");
}

int
main(int argc, char** argv)
{
    int opt;
    int af_family = 0;
    struct addrinfo* p;

    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    while ((opt = getopt(argc, argv, "c:46h")) != -1) {
        switch (opt)
        {
        case 'c':
            if ((stop_times = atol(optarg)) > 0)
                ;
            else
                stop_times = -1;
            break;
        case '4':
            af_family = 4;
            break;
        case '6':
            af_family = 6;
            break;
        case 'h':
            print_help();
            return 0;
        default:
            break;
        }
    }

    if (argv[optind + 1] != NULL)
        port = atoi(argv[optind + 1]);

    if (af_family == 4)
        res = get_resolve_list(argv[optind], AF_INET);
    else if (af_family == 6)
        res = get_resolve_list(argv[optind], AF_INET6);
    else
        res = get_resolve_list(argv[optind], AF_UNSPEC);

    p = res;
    prepare_tcping(p, port);

    freeaddrinfo(res);
    return 0;
}
