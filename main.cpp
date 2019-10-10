#include<iostream>
#include<algorithm> // for sorting
#include<set>

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include <string.h> // for strerror()
#include<errno.h> // for errno

#define MAX_EVENTS 32
#define PORT 6666

using namespace std;

//set a socket to nonblocking state
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
//this union is for separation long int to 4 chars
union four_bytes{
    long l;
    char c[4];
};

//this function uses union "four_bytes", and prints IP:port
void print_IPv4_and_port(long ip_in_hl, uint16_t port_in_hs) {
    int array[4];
    array[0] = ip_in_hl & 0xFF;
    array[1] = (ip_in_hl >> 8) & 0xFF;
    array[2] = (ip_in_hl >> 16) & 0xFF;
    array[3] = (ip_in_hl >> 24) & 0xFF;

    cout
     << array[3] << "."
     << array[2] << "."
     << array[1] << "."
     << array[0] << ":"
     << port_in_hs << endl;
}


int main()
{
//creating socket
    int master_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (master_socket == -1) {
        cerr << "Unable to open socket: " << strerror( errno ) << endl;
        return 1;
    }
    cout << "Created Master socket with filedescriptor: "<< master_socket << endl;
//creating sockaddr struct
    socklen_t client_sock_addr_len = 0;
    struct sockaddr_in sock_addr, /**/client_sock_addr;
    sock_addr.sin_family = AF_INET; //IPv4
    sock_addr.sin_port = htons(PORT); //port
    sock_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // IP addr
//binding
    if (bind(master_socket, (struct sockaddr *) (&sock_addr), sizeof (sock_addr))) {
        cerr << "Unable to bind address or port: " << strerror( errno ) << endl;
        return 1;
    }
    cout << "Bind socket on ";
    print_IPv4_and_port(ntohl(sock_addr.sin_addr.s_addr), ntohs(sock_addr.sin_port));
//setting nonblock
    set_nonblock(master_socket);
//listening
    if (listen(master_socket, SOMAXCONN)){
        cerr << "Unable to open socket for listening: " << strerror( errno ) << endl;
        return 1;
    }

// Beggining EPoll stuff
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
//accepting
                int slave_socket = accept(master_socket, //0, 0);
                                          (struct sockaddr *) (&client_sock_addr),
                                          &client_sock_addr_len);// accepted connection to server
                set_nonblock(slave_socket); // made slave socket nonblocking
// print clients address
                cout << "Client connected on ";
                print_IPv4_and_port(ntohl(sock_addr.sin_addr.s_addr), ntohs(sock_addr.sin_port));


                // question: why we create another struct instead of using Events[];
                // answer: we creates another temporary epoll_event struct to register new slavesocket in epoll by epoll_ctl()
                struct epoll_event Event; // struct that registers happened events on master socket
                Event.data.fd = slave_socket; // add slave socket fd to struct
                Event.events = // type of event that epoll fd will return
                        EPOLLIN; // level triggered (notification when we have unread data)
                epoll_ctl(EPoll, EPOLL_CTL_ADD, slave_socket, &Event); // add socket to epoll (in kernel)
                cout << "Created slave socket with filedescriptor: "<< slave_socket << endl;

            }else { // event occured on slave socket
                static char buffer[1024];
                int recv_size = recv(Events[i].data.fd, //recive some data
                                     buffer, 1024, MSG_NOSIGNAL);
                if ((recv_size == 0)&&(errno != EAGAIN)) { /* no data read, and error is different from
                                              "there is no data available right now, try again later"*/
                    // client closed connection, let's shutdown too
                    shutdown(Events[i].data.fd, SHUT_RDWR);
                    close(Events[i].data.fd);
                cout << "Client closed connection" << endl;
                } else if (recv_size > 0){
                    send(Events[i].data.fd, buffer, 1024, MSG_NOSIGNAL);
                }

            }
        }
    }
}
