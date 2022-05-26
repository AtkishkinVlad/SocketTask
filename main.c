#include<stdio.h>

#include<stdlib.h>

#include<string.h>

#include<unistd.h>

#include<sys/socket.h>

#include<sys/un.h>

#include<sys/time.h>

#include<time.h>

#include<math.h>

#include<fcntl.h>

#include<errno.h>

#include<poll.h>

#include<signal.h>

#include <sys/types.h>

#define SERVER_LOG "server_log.txt"

#define SOCKET_FILENAME "/tmp/my_server_socket"

#define BLOCK 10

#define BUF_LEN 20

struct timeval startTime;
struct timeval stopTime;

double timeDiff(struct timeval t1, struct timeval t2) {
    double res = 0.0;
    res += t2.tv_sec - t1.tv_sec;
    res += (t2.tv_usec - t1.tv_usec) / 1e6;
    return res;
}

long int innerState = 0;

struct ClientData {
    int answerIsReady;
    long int readyNumber;
    long int num;
    size_t len;
    char buf[BUF_LEN + 1];
};

struct FDArray {
    int len;
    int capacity;
    struct pollfd * pollArray;
    struct ClientData * clientArray;
};

struct FDArray * fdArray = NULL;

struct FDArray * getFDArray() {
    struct FDArray * a =
        (struct FDArray * ) malloc(sizeof(struct FDArray));
    if (a == NULL) {
        fprintf(stderr, "_____Not enough memory\n");
        exit(1);
    }
    a -> len = 0;
    a -> capacity = 0;
    a -> pollArray = NULL;
    a -> clientArray = NULL;
    return a;
}

void freeFDArray(struct FDArray * a) {
    for (int i = 0; i < a -> len; i++) {
        close(a -> pollArray[i].fd);
    }
    if (a -> pollArray != NULL)
        free(a -> pollArray);
    if (a -> clientArray != NULL)
        free(a -> clientArray);
    free(a);
}

void addToFDArray(struct FDArray * a,
    struct pollfd pf, struct ClientData cd) {
    if (a -> len == a -> capacity) {
        struct pollfd * newPollArray =
            (struct pollfd * ) realloc(a -> pollArray,
                (a -> capacity + BLOCK) *
                sizeof(struct pollfd));
        struct ClientData * newClientArray =
            (struct ClientData * ) realloc(a -> clientArray,
                (a -> capacity + BLOCK) *
                sizeof(struct ClientData));
        if ((newPollArray == NULL) || (newClientArray == NULL)) {
            fprintf(stderr, "____Not enough memory\n");
            exit(1);
        }
        a -> pollArray = newPollArray;
        a -> clientArray = newClientArray;
        a -> capacity += BLOCK;
    }
    memcpy( & a -> pollArray[a -> len], & pf, sizeof(struct pollfd));
    memcpy( & a -> clientArray[a -> len], & cd, sizeof(struct ClientData));
    a -> len++;
}

void delFromFDArray(struct FDArray * a, int index) {
    if ((index < 0) || (index > a -> len - 1)) {
        fprintf(stderr, "____delFromFDArray with invalid index\n");
        exit(1);
    }
    close(a -> pollArray[index].fd);
    for (int i = index + 1; i < a -> len; i++) {
        memcpy( & a -> pollArray[i - 1], & a -> pollArray[i],
            sizeof(struct pollfd));
        memcpy( & a -> clientArray[i - 1], & a -> clientArray[i],
            sizeof(struct ClientData));
    }
    a -> len--;
}

void usage() {
    fprintf(stdout, "Command line:\n");
    fprintf(stdout, "As server:\n");
    fprintf(stdout, "result -s\n");
    fprintf(stdout, "As client with number num and real positive real delay:\n");
    fprintf(stdout, "result -c <delay> <num>\n");
    fprintf(stdout, "Print number of server inner state:\n");
    fprintf(stdout, "result -t\n");
}

void precWait(double t) {
    if (t <= 0.0)
        return;
    struct timespec req;
    struct timespec rem;
    memset( & rem, 0, sizeof(struct timespec));
    req.tv_sec = (long int) floor(t);
    req.tv_nsec = (long int) floor(1000000000L * (t - floor(t)));

    while (1) {
        int res = nanosleep( & req, & rem);
        if (res == 0)
            break;
        if ((res == -1) && (errno != EINTR)) {
            perror("____nanosleep error\n");
            exit(1);
        }
        memcpy( & rem, & req, sizeof(struct timespec));
    }
}

int receiveNumber(int fd, long int * num) {

    char buf[BUF_LEN + 2];
    memset(buf, '\0', BUF_LEN + 2);

    ssize_t rec;

    rec = recv(fd, & buf, BUF_LEN + 1, 0);

    if (rec == -1) {
        perror("___recv faield\n");
        return 0;
    } else if (rec == BUF_LEN + 1) {
        if (sscanf(buf, "%ld", num) != 1) {
            fprintf(stderr, "____received bad number '%s'\n", buf);
            return 0;
        }
    } else {
        fprintf(stderr, "____received %ld bytes of number\n", rec);
        return 0;
    }
    return 1;
}

int sendNumber(int fd, long int num) {
    char buf[BUF_LEN + 1];
    memset(buf, '\0', BUF_LEN + 1);
    sprintf(buf, "%ld", num);
    ssize_t sent = send(fd, buf, BUF_LEN + 1, MSG_NOSIGNAL);

    if (sent == -1) {
        perror("send number failed");
        return 0;
    } else if (sent != BUF_LEN + 1) {
        fprintf(stderr, "___sent too short string\n");
        return 0;
    }
    return 1;
}
int receiveChar(int fd, char * ch) {
    ssize_t rec = 0;
    rec = recv(fd, ch, 1, 0);
    if (rec == -1) {
        perror("___recv failed\n");
        return 0;
    } else if (rec != 1) {
        fprintf(stderr, "____recv received 0 bytes\n");
        return 0;
    }
    return 1;
}

int sendChar(int fd, char ch) {
    ssize_t sent = send(fd, & ch, 1, MSG_NOSIGNAL);
    if (sent == -1) {
        perror("___send char failed");
        return 0;
    } else if (sent == 0) {
        fprintf(stderr, "___Sent 0 bytes\n");
        return 0;
    }
    return 1;
}

void signalHandler1(int s) {
    freeFDArray(fdArray);

    unlink(SOCKET_FILENAME);

    fprintf(stderr, "Server stopped with inner state %ld\n", innerState);

    exit(0);
}

void server() {
    if (freopen(SERVER_LOG, "w", stderr) == NULL) {
        perror("___cannot change the file associated with stderr");
        exit(1);
    }

    struct sigaction sa1;
    sa1.sa_handler = signalHandler1;
    sigemptyset( & sa1.sa_mask);
    sigaddset( & sa1.sa_mask, SIGINT);
    sigaddset( & sa1.sa_mask, SIGTERM);
    sa1.sa_flags = 0;

    if (sigaction(SIGINT, & sa1, NULL) == -1) {
        perror("___Cannot set signal handler for SIGINT\n");
        exit(1);
    }

    if (sigaction(SIGTERM, & sa1, NULL) == -1) {
        perror("___Cannot set signal handler for SIGTERM\n");
        exit(1);
    }

    fdArray = getFDArray();

    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    memset( & addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_FILENAME, sizeof(addr.sun_path) - 1);

    if (bind(sfd, (struct sockaddr * ) & addr,
            sizeof(struct sockaddr_un)) == -1) {
        fprintf(stderr, "___Cannot bind unix socket with path '%s'\n",
            SOCKET_FILENAME);
        exit(1);
    }

    struct pollfd s_pollfd;
    s_pollfd.fd = sfd;
    s_pollfd.events = POLLIN;
    s_pollfd.revents = 0;
    struct ClientData sd;
    sd.num = -1;
    sd.len = 0;
    sd.answerIsReady = 0;

    addToFDArray(fdArray, s_pollfd, sd);

    if (listen(sfd, 100) == -1) {
        perror("___listen failed");
        exit(1);
    }

    while (1) {

        int events = 0;

        if ((events = poll(fdArray -> pollArray, fdArray -> len, 100)) > 0) {

            if (fdArray -> pollArray[0].revents & POLLIN) {
                int cfd = 0;

                struct sockaddr_storage claddr;
                socklen_t addrlen = sizeof(struct sockaddr_storage);

                if ((cfd = accept(sfd, (struct sockaddr * ) & claddr, &
                        addrlen)) == -1) {
                    fprintf(stderr, "____accept error\n");
                    continue;
                }
                struct ClientData cd;
                cd.num = -1;
                cd.len = 0;
                cd.answerIsReady = 0;
                struct pollfd c_pollfd;
                c_pollfd.fd = cfd;
                c_pollfd.events = POLLIN;
                c_pollfd.revents = 0;

                addToFDArray(fdArray, c_pollfd, cd);

                fprintf(stderr, "====using fd %d for new incoming connection\n", cfd);

                fprintf(stderr, "@@@@heap position is %p\n", sbrk(0));

                if (fdArray -> len == 2) {
                    gettimeofday( & startTime, NULL);
                }
            }
            for (int i = fdArray -> len - 1; i > 0; i--) {
                int revents = fdArray -> pollArray[i].revents;
                int fd = fdArray -> pollArray[i].fd;
                fdArray -> pollArray[i].revents = 0;

                if (revents == 0) {
                    continue;
                }
                if (revents & POLLHUP) {
                    fprintf(stderr, "Connection with number %ld is closed\n",
                        fdArray -> clientArray[i].num);
                    close(fdArray -> pollArray[i].fd);
                    delFromFDArray(fdArray, i);

                    if (fdArray -> len == 1) {
                        gettimeofday( & stopTime, NULL);
                        double t = timeDiff(startTime, stopTime);
                        fprintf(stderr, "::::Last active period of server "
                            "was %lf seconds long\n", t);
                        fflush(stderr);
                    }

                    continue;
                }
                if (revents & POLLERR) {
                    fprintf(stderr, "____Connection with number %ld failed\n",
                        fdArray -> clientArray[i].num);
                    close(fdArray -> pollArray[i].fd);
                    delFromFDArray(fdArray, i);
                    continue;
                }
                if (revents & POLLNVAL) {
                    fprintf(stderr, "____fd for connection with index %d is not opened\n", i);

                    close(fdArray -> pollArray[i].fd);
                    delFromFDArray(fdArray, i);
                    continue;
                }
                if (revents & POLLIN) {

                    if (fdArray -> clientArray[i].num == -1) {

                        long int num = 0;
                        if (!receiveNumber(fd, & num)) {
                            fprintf(stderr, "Cannot receive connection number from client "
                                "with index %d\n", i);
                        } else {
                            fdArray -> clientArray[i].num = num;
                        }
                    } else if (!fdArray -> clientArray[i].answerIsReady) {
                        char ch;
                        if (!receiveChar(fd, & ch)) {
                            fprintf(stderr, "___Server cannot receive char from "
                                "client with number %ld\n", fdArray -> clientArray[i].num);
                        } else {
                            long int num;
                            fprintf(stderr, "Received from client with number %ld symbol '%c'\n",
                                fdArray -> clientArray[i].num, ch != '\0' ? ch : ' ');
                            fdArray -> clientArray[i].buf[fdArray -> clientArray[i].len++] = ch;
                            if (ch == '\0') {
                                if (sscanf(fdArray -> clientArray[i].buf, "%ld", & num) != 1) {
                                    fprintf(stderr, "___Received from client with number "
                                        "%ld bad number '%s'\n",
                                        fdArray -> clientArray[i].num,
                                        fdArray -> clientArray[i].buf);
                                } else {
                                    innerState += num;
                                    fprintf(stderr, "New inner state is %ld\n", innerState);
                                    fdArray -> clientArray[i].readyNumber = innerState;
                                    fdArray -> clientArray[i].answerIsReady = 1;

                                    fdArray -> pollArray[i].events = POLLOUT;
                                }
                                memset(fdArray -> clientArray[i].buf, '\0', BUF_LEN + 1);
                                fdArray -> clientArray[i].len = 0;
                            }
                        }
                    }
                }
                if (revents & POLLOUT) {
                    if (fdArray -> clientArray[i].answerIsReady) {
                        if (!sendNumber(fd, fdArray -> clientArray[i].readyNumber)) {
                            fprintf(stderr, "Server cannot send innerState to client\n");
                        } else {
                            fdArray -> clientArray[i].answerIsReady = 0;
                            fprintf(stderr, "Server sent number %ld to client "
                                "with number %ld\n", fdArray -> clientArray[i].readyNumber,
                                fdArray -> clientArray[i].num);
                            fdArray -> pollArray[i].events = POLLIN;
                        }
                    }
                }
            }
        }
    }
}

void client(double delay, long int num) {
    srand(time(NULL) + num);

    double delaysSum = 0.0;

    char filename[1024];
    sprintf(filename, "client_log_%06ld.txt", num);
    if (freopen(filename, "w", stderr) == NULL) {
        perror("___cannot change the file associated with stderr");
        exit(1);
    }
    fprintf(stderr, "Client with number %ld and delay parameter %lf started\n",
        num, delay);

    int cfd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    memset( & addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_FILENAME, sizeof(addr.sun_path) - 1);

    if (connect(cfd, (struct sockaddr * ) & addr,
            sizeof(struct sockaddr_un)) == -1) {
        perror("___connection with server failed");
        exit(1);
    }

    fprintf(stderr, "Connected with server\n");

    int numberSent = 0;

    struct pollfd c_pollfd;
    c_pollfd.fd = cfd;
    c_pollfd.events = POLLOUT;
    c_pollfd.revents = 0;

    int symbolsToDelay = 1 + (rand() % 255);
    int waitServerAnswer = 0;

    while (1) {

        int events = poll( & c_pollfd, 1, 100);

        if ((events == -1) && (errno != EINTR)) {
            perror("poll failed");
            break;
        }

        if (events > 0) {
            int revents = c_pollfd.revents;

            if (revents & POLLHUP) {
                fprintf(stderr, "Server closed connection\n");
                break;
            }
            if (revents & POLLERR) {
                fprintf(stderr, "__Connection with server failed\n");
                break;
            }
            if (revents & POLLNVAL) {
                fprintf(stderr, "____fd is not opened\n");
                break;
            }
            if ((revents & POLLIN) && (waitServerAnswer)) {
                long int num = 0;

                if (!receiveNumber(cfd, & num)) {
                    fprintf(stderr, "Cannot receive server's inner state\n");
                    close(cfd);
                    exit(1);
                } else {
                    fprintf(stderr, "Server's inner state is %ld\n", num);
                }

                c_pollfd.events = POLLOUT;
                waitServerAnswer = 0;
                continue;
            }
            if ((revents & POLLOUT) && (!waitServerAnswer)) {

                if (!numberSent) {
                    if (!sendNumber(cfd, num)) {
                        fprintf(stderr, "send number to server failed\n");
                        close(cfd);
                        exit(1);
                    } else {
                        fprintf(stderr, "send number %ld to server\n", num);
                    }
                    numberSent = 1;
                    continue;
                }

                int ch = fgetc(stdin);
                if (ch == EOF)
                    break;
                if (ch == '\n') {
                    ch = '\0';
                }

                if (!sendChar(cfd, ch)) {
                    fprintf(stderr, "Send to server failed\n");
                    close(cfd);
                    exit(1);
                } else {
                    fprintf(stderr, "Send to server char '%c'\n",
                        ch != '\0' ? (char) ch : ' ');
                }

                symbolsToDelay--;
                if (symbolsToDelay == 0) {
                    precWait(delay);
                    delaysSum += delay;

                    symbolsToDelay = 1 + (rand() % 255);
                }

                if (ch == '\0') {

                    c_pollfd.events = POLLIN;
                    waitServerAnswer = 1;
                }
                continue;
            }
        }
    }

    close(cfd);
    fprintf(stderr, "Sum of delays in seconds:\n");
    fprintf(stderr, "+++%lf\n", delaysSum);
    fprintf(stderr, "Connection with server closed\n");
}

void getServerInnerState() {
    int cfd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    memset( & addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_FILENAME, sizeof(addr.sun_path) - 1);

    if (connect(cfd, (struct sockaddr * ) & addr,
            sizeof(struct sockaddr_un)) == -1) {
        perror("___connection with server failed");
        exit(1);
    }

    int numberSent = 0;

    struct pollfd c_pollfd;
    c_pollfd.fd = cfd;
    c_pollfd.events = POLLOUT;
    c_pollfd.revents = 0;

    int waitServerAnswer = 0;

    while (1) {
        int events = poll( & c_pollfd, 1, 100);

        if ((events == -1) && (errno != EINTR)) {
            perror("poll failed");
            break;
        }

        if (events > 0) {
            int revents = c_pollfd.revents;

            if (revents & POLLHUP) {
                fprintf(stderr, "Server closed connection\n");
                break;
            }
            if (revents & POLLERR) {
                fprintf(stderr, "__Connection with server failed\n");
                break;
            }
            if (revents & POLLNVAL) {
                fprintf(stderr, "____fd is not opened\n");
                break;
            }
            if ((revents & POLLIN) && (waitServerAnswer)) {
                long int num = 0;

                if (!receiveNumber(cfd, & num)) {
                    fprintf(stderr, "Cannot receive server's inner state\n");
                    close(cfd);
                    exit(1);
                } else {
                    fprintf(stdout, "Server's inner state is %ld\n", num);
                }

                break;
            }
            if ((revents & POLLOUT) && (!waitServerAnswer)) {

                if (!numberSent) {
                    if (!sendNumber(cfd, 0)) {
                        fprintf(stderr, "send number to server failed\n");
                        close(cfd);
                        exit(1);
                    }
                    numberSent = 1;
                    continue;
                } else {
                    if (!sendNumber(cfd, 0)) {
                        fprintf(stderr, "send number to server failed\n");
                        close(cfd);
                        exit(1);
                    }
                    waitServerAnswer = 1;
                    c_pollfd.events = POLLIN;
                    continue;
                }
            }
        }
    }

    close(cfd);
}

int main(int argc, char * argv[]) {
    double delay = 0.0;
    long int num = 0;

    if ((argc == 2) && (strcmp(argv[1], "-s") == 0)) {
        server();
    } else if ((argc == 4) &&
        (strcmp(argv[1], "-c") == 0) &&
        (sscanf(argv[2], "%lf", & delay) == 1) &&
        (delay > 0.0) &&
        (sscanf(argv[3], "%ld", & num) == 1)) {

        client(delay, num);

    } else if ((argc == 2) &&
        (strcmp(argv[1], "-t") == 0)) {
        getServerInnerState();
    } else {
        fprintf(stderr, "Bad command line\n");
        usage();
        exit(1);
    }

    return 0;
}