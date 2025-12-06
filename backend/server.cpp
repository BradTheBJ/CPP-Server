// ============================================
// Standard C++ libraries
// ============================================
#include <iostream>      // For std::cout, std::cerr (console output)
#include <string>         // For std::string (string manipulation)
#include <cstring>        // For std::memset (memory operations)
#include <fstream>        // For file reading (std::ifstream)
#include <iterator>       // For std::istreambuf_iterator (file reading)
#include <sstream>        // For std::istringstream (string parsing)
#include <algorithm>      // For string algorithms (unused but available)

// ============================================
// Cross-platform socket includes
// ============================================
#ifdef _WIN32
    // Windows requires Winsock2 for networking
    #include <winsock2.h>        // socket(), bind(), listen(), accept(), closesocket()
    #include <ws2tcpip.h>        // sockaddr_in, inet_pton(), etc.
    #pragma comment(lib, "ws2_32.lib") // Link Winsock library

    // Windows lacks POSIX close(), so we map it
    #define closeSocket closesocket
#else
    // POSIX (Linux, macOS)
    #include <netinet/in.h>      // sockaddr_in, htons, INADDR_ANY
    #include <sys/socket.h>      // socket(), bind(), listen(), accept(), send(), recv()
    #include <unistd.h>          // close()
    
    // On POSIX, close() is correct
    #define closeSocket close
#endif

// ============================================
// Content-Type resolver (unchanged)
// ============================================
std::string getContentType(const std::string& path) {
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html")
        return "text/html; charset=UTF-8";
    else if (path.size() >= 3 && path.substr(path.size() - 3) == ".js")
        return "application/javascript";
    else if (path.size() >= 4 && path.substr(path.size() - 4) == ".css")
        return "text/css";
    else if (path.size() >= 4 && path.substr(path.size() - 4) == ".png")
        return "image/png";
    else if ((path.size() >= 4 && path.substr(path.size() - 4) == ".jpg") ||
             (path.size() >= 5 && path.substr(path.size() - 5) == ".jpeg"))
        return "image/jpeg";

    return "text/plain";
}

// ============================================
// Parse HTTP request path (unchanged)
// ============================================
std::string parseRequestPath(const std::string& request) {
    std::istringstream iss(request);
    std::string method, path, version;

    if (iss >> method >> path >> version)
        return path;

    return "/";
}

// ============================================
// Main server code
// ============================================
int main(int argc, char const *argv[]) {

#ifdef _WIN32
    // ============================================
    // WINDOWS-ONLY: Initialize Winsock
    // ============================================
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }
#endif

    // ============================================
    // STEP 1: Validate command-line arguments
    // ============================================
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    // ============================================
    // STEP 2: Create socket
    // ============================================
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // ============================================
    // STEP 3: Set socket options
    // ============================================
    int opt = 1;

#ifdef _WIN32
    // Windows uses a different signature for setsockopt(), but same idea
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        closeSocket(server_fd);
        return 1;
    }
#else
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        closeSocket(server_fd);
        return 1;
    }
#endif

    // ============================================
    // STEP 4: Prepare server address
    // ============================================
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;

    int port = 0;
    try {
        port = std::stoi(argv[1]);
    } catch (...) {
        std::cerr << "Invalid port number.\n";
        closeSocket(server_fd);
        return 1;
    }

    address.sin_port = htons(port);

    // ============================================
    // STEP 5: Bind
    // ============================================
    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        perror("bind");
        closeSocket(server_fd);
        return 1;
    }

    // ============================================
    // STEP 6: Listen
    // ============================================
    if (listen(server_fd, 8) < 0) {
        perror("listen");
        closeSocket(server_fd);
        return 1;
    }

    std::cout << "Server running on port " << port << "\n";

    // ============================================
    // STEP 7: Accept loop
    // ============================================
    socklen_t addrlen = sizeof(address);

    while (true) {
        std::cout << "Waiting for connections...\n";

        int new_socket = accept(server_fd, reinterpret_cast<struct sockaddr*>(&address), &addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        // ============================================
        // Read HTTP request
        // ============================================
        char buffer[4096] = {0};

#ifdef _WIN32
        int bytes_read = recv(new_socket, buffer, sizeof(buffer)-1, 0);
#else
        ssize_t bytes_read = recv(new_socket, buffer, sizeof(buffer)-1, 0);
#endif

        if (bytes_read <= 0) {
            perror("recv");
            closeSocket(new_socket);
            continue;
        }

        std::string request(buffer, bytes_read);
        std::string request_path = parseRequestPath(request);

        // ============================================
        // Map request to file path
        // ============================================
        std::string file_path;

        if (request_path == "/" || request_path == "/index.html") {
            file_path = "backend/index.html";
        } else {
            file_path = request_path.substr(1);
        }

        // ============================================
        // File reading
        // ============================================
        std::ifstream file(file_path, std::ios::in | std::ios::binary);
        std::string response;

        if (!file.is_open()) {
            response =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 13\r\n"
                "Connection: close\r\n"
                "\r\n"
                "404 Not Found";
        } else {
            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
            file.close();

            std::string content_type = getContentType(file_path);

            response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: " + content_type + "\r\n"
                "Content-Length: " + std::to_string(content.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" +
                content;
        }

        // ============================================
        // Send response
        // ============================================
#ifdef _WIN32
        send(new_socket, response.c_str(), (int)response.size(), 0);
#else
        send(new_socket, response.c_str(), response.size(), 0);
#endif

        // Close this client's connection
        closeSocket(new_socket);
    }

    closeSocket(server_fd);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
