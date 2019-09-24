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
    int master_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (master_socket == -1) {
        cerr << "Unable to open socket" << endl;
        return 1;
    }


    struct sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(12345);
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(master_socket, (struct sockaddr *) (&sock_addr), sizeof (sock_addr))) {
        cerr << "Unable to bind address or port" << endl;
        return 1;
    }

    set_nonblock(master_socket);
    listen(master_socket, SOMAXCONN);

    int EPoll = // epoll filedescriptor
            epoll_create1(0);

    struct epoll_event Event; // struct that registers happened events on master socket
    Event.data.fd = master_socket; // add Master socket fd
    Event.events = // type of event that epoll fd will return
            EPOLLIN; // level triggered (notification when we have unread data)
    epoll_ctl(EPoll, EPOLL_CTL_ADD, master_socket, &Event); // add socket to epoll (in kernel)

    while (true) {
        struct epoll_event Events[MAX_EVENTS]; // struct that registers happened events on slave socket
        int N = // number of events that have been returned from kernel
                epoll_wait(EPoll, Events, MAX_EVENTS, -1); // wait for occured events
        for (unsigned i = 0; i < (unsigned) N; i++) {
            if (Events[i].data.fd == master_socket){ // check if event occured on Mastes Socket (someone try to connect)
                int slave_socket = accept(master_socket, nullptr, nullptr); // accepted connection to server
                set_nonblock(slave_socket); // made slave socket nonblocking

                // question: why we create another struct instead of using Events[];
                // we creates another temporary epoll_event struct to register new slavesocket in epoll by epoll_ctl()
                struct epoll_event Event; // struct that registers happened events on master socket
                Event.data.fd = slave_socket; // add slave socket fd to struct
                Event.events = // type of event that epoll fd will return
                        EPOLLIN; // level triggered (notification when we have unread data)
                epoll_ctl(EPoll, EPOLL_CTL_ADD, slave_socket, &Event); // add socket to epoll (in kernel)

            }else { // event occured on slave socket
                static char buffer[1024];
                int recv_size = recv(Events[i].data.fd, //recive some data
                                     buffer, 1024, MSG_NOSIGNAL);
                if ((recv_size == 0)&&(errno != EAGAIN)) { /* no data read, and error is different from
                                              "there is no data available right now, try again later"*/
                    // client closed connection, let's shutdown too
                    shutdown(Events[i].data.fd, SHUT_RDWR);
                    close(Events[i].data.fd);
                } else if (recv_size > 0){
                    send(Events[i].data.fd, buffer, 1024, MSG_NOSIGNAL);
                }

            }
        }
    }
}
