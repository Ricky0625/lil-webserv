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

int main(int ac, char **av)
{

    if (ac != 2)
    {
        std::cerr << "./lil_webserv [port]"
                  << "\n";
        exit(EXIT_FAILURE);
    }

    const std::string PORT = av[1];

    // GETADDRINFO START ================
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo; // will point to the results

    // ensure that the hints is empty
    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // call getaddrinfo
    status = getaddrinfo("localhost", PORT.c_str(), &hints, &servinfo);
    if (status != 0)
    {
        std::cerr << "getaddrinfo error: " << std::strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);
    }
    // ================ GETADDRINFO END

    // SOCKET START ================
    int serverfd; // a socket fd

    serverfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (serverfd == -1)
    {
        std::cerr << std::strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);
    }
    // std::cout << "socket init done! severfd (socket): " << serverfd << std::endl;
    // fcntl(serverfd, F_SETFL, O_NONBLOCK); // uncomment this to make socket non blocking
    // verify_socketinfo(servinfo);
    // ================ SOCKET END

    // BIND START ================
    const int reuse = 1;

    // FOR TESTING DEV PURPOSE START ================
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
    {
        std::cerr << "setsockopt error: " << std::strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);
    }
    // ================ FOR TESTING DEV PURPOSE END
    if (bind(serverfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        std::cerr << "bind error: " << std::strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);
    }
    // ================ BIND END

    // free the linked-list, no use starting from this point onward
    freeaddrinfo(servinfo);

    // LISTEN START ================
    if (listen(serverfd, 10) == -1)
    {
        std::cerr << "listen error: " << strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);
    }
    // std::cout << "listen done!" << std::endl;
    // ================ LISTEN END

    std::cout << "server is listening for incoming connections on port " << PORT << " ..."
              << "\n";

    // ACCEPT START ================
    // this part needs to be in an infinite loop
    struct sockaddr_storage incoming_addr;
    socklen_t addr_size = sizeof(incoming_addr);
    int incomingfd;

    incomingfd = accept(serverfd, (struct sockaddr *)&incoming_addr, &addr_size);

    char buf[1024 + 1];
    recv(incomingfd, buf, 1024, 0);
    std::cout << buf << std::endl;

    const std::string msg = "HTTP1.1 200 ok\r\n\r\nHatsune Miku is cute";
    send(incomingfd, msg.c_str(), msg.size(), 0);
    // ================ ACCEPT END

    // CLOSE ================
    close(incomingfd);
    close(serverfd);
    // ================ CLOSE END

    system("leaks -q lil_webserv");
}
