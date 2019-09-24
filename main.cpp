#include<iostream>
#include<algorithm>
#include<set>

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/epoll.h>

#define MAX_EVENTS 32

using namespace std;

int set_nonblock (int fd) {
    int flags;
#if defined (O_NONBLOCK)
    if(-1 == (flags = fcntl(fd,F_GETFL, 0)))
        flags = 0;
    return  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}

int main()
{
    int MasterSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (MasterSocket == -1) {
        cerr << "Unable to open socket" << endl;
        return 1;
    }


    struct sockaddr_in SockAddr;
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(12345);
    SockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(MasterSocket, (struct sockaddr *) (&SockAddr), sizeof (SockAddr))) {
        cerr << "Unable to bind address or port" << endl;
        return 1;
    }

    set_nonblock(MasterSocket);
    listen(MasterSocket, SOMAXCONN);

    int EPoll = epoll_create1(0);

    struct epoll_event Event;
    Event.data.fd = MasterSocket;
    Event.events = EPOLLIN;
    epoll_ctl(EPoll, EPOLL_CTL_ADD, MasterSocket, &Event);

    while (true) {
        struct epoll_event Events[MAX_EVENTS];
        int N = epoll_wait(EPoll, Events, MAX_EVENTS, -1);
        for (unsigned i = 0; i < (unsigned) N; i++) {
            if (Events[i].data.fd == MasterSocket){

            }else {

            }
        }
    }

    return 0;
}
