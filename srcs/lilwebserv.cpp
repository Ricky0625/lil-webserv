#include "lilwebserv.h"

void verify_getaddrinfo(struct addrinfo *servinfo)
{
    char ipstr[INET6_ADDRSTRLEN];
    for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next)
    {
        void *addr;
        std::string ipver;

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (p->ai_family == AF_INET)
        { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        }
        else
        { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        // convert the IP to a string and print it:
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        std::cout << "  " << ipver << ": " << ipstr << "\n";
    }
}

void verify_socketinfo(struct addrinfo *servinfo)
{
    std::cout << "Init socket with these: "
              << "\n"
              << "Family: " << (servinfo->ai_family == PF_INET ? "" : "NOT ") << "PF_INET\n"
              << "Socktype: " << (servinfo->ai_socktype == SOCK_STREAM ? "" : "NOT ") << "SOCK_STREAM\n"
              << "Protocol: " << (servinfo->ai_protocol == IPPROTO_TCP ? "TCP" : (servinfo->ai_protocol == IPPROTO_UDP ? "UDP" : "Unknown")) << "\n";
}

int setupServerSocket(const char *port)
{
    int serverfd;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;

    // obtain the information about the local address to which the server will bind
    if (getaddrinfo("localhost", port, &hints, &servinfo) != 0)
    {
        std::cerr << "getaddrinfo error: " << gai_strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // creates a socket
    if ((serverfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        std::cerr << "socket error: " << strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // FOR TESTING PURPOSE: to reuse socket fd
    const int reuse = 1;
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
    {
        std::cerr << "setsockopt error: " << strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // bind socket
    if (bind(serverfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        std::cerr << "bing error: " << strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // free the linked-list, no use from this point onward
    freeaddrinfo(servinfo);

    return serverfd;
}

void listenIncoming(int serverfd, const std::string &port)
{
    // tell a socket to listen for incoming connections
    if (listen(serverfd, 10) == -1)
    {
        std::cerr << "listen error: " << strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    }
    std::cout << "server is listening for incoming connections on port " << port << " ..." << std::endl;
}

int acceptConnection(int serverfd)
{
    struct sockaddr_storage incoming_addr;
    socklen_t addr_size = sizeof(incoming_addr);
    int clientfd;

    if ((clientfd = accept(serverfd, (struct sockaddr *)&incoming_addr, &addr_size)) == -1)
    {
        std::cerr << "accept error: " << strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    }
    return clientfd;
}

int processClientRequest(int clientfd)
{
    // need to use other way for this. now only can handle small request
    char buf[1024];
    int byte_count;

    byte_count = recv(clientfd, &buf, sizeof(buf), 0);
    if (byte_count == 0)
    {
        std::cerr << "Client: " << clientfd << " closed connection on server!" << std::endl;
        close(clientfd);
        return 0;
    }
    else if (byte_count == -1)
    {
        std::cerr << "recv error: " << strerror(errno) << std::endl;
        close(clientfd);
        return 0;
    }
    // std::cout << "received this:\n"
    //           << buf << std::endl;
    return 1;
}

int main(int ac, char **av)
{

    if (ac != 2)
    {
        std::cerr << "./lil_webserv [port]" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    const std::string PORT = av[1];
    int serverfd;

    serverfd = setupServerSocket(PORT.c_str());
    listenIncoming(serverfd, PORT);

    const int NUM_OF_FD_TO_MONITOR = 1;
    struct pollfd pfds[1]; // we only have to monitor one port

    pfds[0].fd = serverfd;   // monitor serverfd
    pfds[0].events = POLLIN; // tell me when ready to read. server only needs to know when to read

    while (true) // infinite loop
    {
        // polling
        int num_events = poll(pfds, NUM_OF_FD_TO_MONITOR, -1); // set to -1 means monitor until the end of the world

        // no need to check if poll will return 0, since it won't timeout...
        if (num_events == -1)
        {
            std::cerr << "poll error: " << strerror(errno) << std::endl;
            break;
        }

        // check if the events we are expecting occured. in this case, we are hoping for POLLIN (ready to read)
        int pollin_occurs = pfds[0].revents & POLLIN;

        if (pollin_occurs)
        {
            int clientfd = acceptConnection(serverfd);
            int status = processClientRequest(clientfd);
            const std::string msg = "HTTP1.1 200 ok\r\n\r\nComparison is the thief of joy.\n";
            send(clientfd, msg.c_str(), msg.size(), 0);
            if (status)
                close(clientfd);
        }
        else
            std::cout << "Hmmm... something went wrong... Not ready yet." << std::endl;
    }
    close(serverfd);
    system("leaks -q lil_webserv");
}
