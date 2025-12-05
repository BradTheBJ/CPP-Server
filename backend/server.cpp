// Standard C++ libraries for I/O, strings, and file operations
#include <iostream>      // For std::cout, std::cerr (console output)
#include <string>         // For std::string (string manipulation)
#include <cstring>        // For std::memset (memory operations)
#include <fstream>        // For file reading (std::ifstream)
#include <iterator>       // For std::istreambuf_iterator (file reading)
#include <sstream>        // For std::istringstream (string parsing)
#include <algorithm>      // For string algorithms (currently unused but available)

// POSIX socket libraries for network programming
#include <netinet/in.h>   // For sockaddr_in structure (IPv4 addresses)
#include <sys/socket.h>   // For socket(), bind(), listen(), accept(), send(), recv()
#include <unistd.h>       // For close() (close file descriptors)

/**
 * Determines the appropriate Content-Type HTTP header based on file extension.
 * This is crucial for browsers to correctly interpret and display the content.
 * 
 * @param path The file path (e.g., "index.html", "style.css")
 * @return The Content-Type string for the HTTP response header
 * 
 * How it works:
 * - Checks the file extension by examining the last N characters of the path
 * - Returns the appropriate MIME type for each supported file type
 * - Defaults to "text/plain" for unknown file types
 */
std::string getContentType(const std::string& path) {
    // Check for HTML files (5 characters: ".html")
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") {
        return "text/html; charset=UTF-8";
    } 
    // Check for JavaScript files (3 characters: ".js")
    else if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") {
        return "application/javascript";
    } 
    // Check for CSS files (4 characters: ".css")
    else if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") {
        return "text/css";
    } 
    // Check for PNG images (4 characters: ".png")
    else if (path.size() >= 4 && path.substr(path.size() - 4) == ".png") {
        return "image/png";
    } 
    // Check for JPEG images (4 characters: ".jpg" or 5: ".jpeg")
    else if ((path.size() >= 4 && path.substr(path.size() - 4) == ".jpg") ||
             (path.size() >= 5 && path.substr(path.size() - 5) == ".jpeg")) {
        return "image/jpeg";
    }
    // Default: treat as plain text for unknown file types
    return "text/plain";
}

/**
 * Parses the HTTP request string to extract the requested file path.
 * 
 * HTTP requests have the format: "METHOD /path HTTP/VERSION"
 * Example: "GET /index.html HTTP/1.1"
 * 
 * @param request The raw HTTP request string received from the client
 * @return The requested path (e.g., "/index.html" or "/")
 * 
 * How it works:
 * - Uses string stream to split the request into tokens
 * - Extracts the method (GET), path (/index.html), and version (HTTP/1.1)
 * - Returns the path component, or "/" if parsing fails
 */
std::string parseRequestPath(const std::string& request) {
    std::istringstream iss(request);  // Create a stream from the request string
    std::string method, path, version;
    
    // Parse the first line: "GET /path HTTP/1.1"
    // The >> operator splits on whitespace
    if (iss >> method >> path >> version) {
        return path;  // Return the requested path
    }
    
    // If parsing fails, default to root path
    return "/";
}

/**
 * Main function: Sets up and runs the HTTP server
 * 
 * Server lifecycle:
 * 1. Validate command-line arguments (port number)
 * 2. Create a socket (communication endpoint)
 * 3. Configure socket options (allow address reuse)
 * 4. Bind socket to a specific IP address and port
 * 5. Start listening for incoming connections
 * 6. Accept connections in a loop and serve files
 */
int main(int argc, char const *argv[]) {
    // ============================================
    // STEP 1: Validate command-line arguments
    // ============================================
    // The server requires a port number as a command-line argument
    // argc[0] is the program name, argc[1] should be the port
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    // ============================================
    // STEP 2: Create a socket
    // ============================================
    // socket() creates an endpoint for communication
    // AF_INET: IPv4 protocol family
    // SOCK_STREAM: TCP (reliable, connection-oriented)
    // 0: Let the system choose the protocol (TCP for SOCK_STREAM)
    // Returns a file descriptor (server_fd) or -1 on error
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // ============================================
    // STEP 3: Configure socket options
    // ============================================
    // SO_REUSEADDR allows the socket to reuse the address/port immediately
    // after the server is closed, without waiting for the OS timeout.
    // This is useful during development to avoid "Address already in use" errors.
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    // ============================================
    // STEP 4: Set up server address structure
    // ============================================
    // sockaddr_in is a structure that holds IPv4 address and port information
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));  // Zero out the structure
    
    address.sin_family = AF_INET;                // IPv4 protocol family
    address.sin_addr.s_addr = INADDR_ANY;        // Listen on all available network interfaces
                                                 // (0.0.0.0 means accept connections from anywhere)

    // Convert the port argument from string to integer
    // We use try-catch to handle invalid input gracefully
    int port = 0;
    try {
        port = std::stoi(argv[1]);  // Convert string to integer
    } catch (...) {
        std::cerr << "Invalid port number.\n";
        close(server_fd);
        return 1;
    }
    
    // htons() converts the port from host byte order to network byte order
    // (big-endian). This is required for network communication to ensure
    // port numbers are interpreted correctly across different systems.
    address.sin_port = htons(port);

    // ============================================
    // STEP 5: Bind socket to address and port
    // ============================================
    // bind() associates the socket with a specific IP address and port.
    // This tells the OS: "When someone connects to this IP:port, give me the connection"
    // We cast sockaddr_in* to sockaddr* because bind() expects the generic sockaddr type
    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    // ============================================
    // STEP 6: Start listening for connections
    // ============================================
    // listen() marks the socket as a passive socket (one that will accept connections)
    // The second argument (8) is the backlog: maximum number of pending connections
    // that can be queued while waiting for accept()
    if (listen(server_fd, 8) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "Server is running on port " << port << "\n";

    // ============================================
    // STEP 7: Main server loop - Accept and handle connections
    // ============================================
    // The server runs indefinitely, accepting one connection at a time
    // In a production server, you'd typically use threads or async I/O for concurrency
    
    socklen_t addrlen = sizeof(address);  // Size of address structure (required for accept())
    
    while (true) {
        std::cout << "Waiting for connections...\n";
        
        // ============================================
        // STEP 7a: Accept incoming connection
        // ============================================
        // accept() blocks until a client connects to the server
        // When a connection arrives, it creates a NEW socket (new_socket) for that connection
        // The original server_fd continues listening for more connections
        // Returns a new file descriptor for the client connection, or -1 on error
        int new_socket = accept(server_fd, reinterpret_cast<struct sockaddr*>(&address), &addrlen);
        if (new_socket < 0) {
            perror("accept");
            // Continue accepting after transient errors (don't crash the server)
            continue;
        }

        // ============================================
        // STEP 7b: Read HTTP request from client
        // ============================================
        // recv() reads data sent by the client over the connection
        // We allocate a 4KB buffer to hold the HTTP request
        // HTTP requests are typically small (just headers and maybe a path)
        char buffer[4096] = {0};  // Initialize buffer with zeros
        ssize_t bytes_read = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read < 0) {
            perror("recv");
            close(new_socket);  // Close this connection on error
            continue;            // Continue to next connection
        }

        // Convert the received bytes into a C++ string for easier manipulation
        std::string request(buffer, bytes_read);
        
        // Parse the HTTP request to extract the requested file path
        // Example: "GET /index.html HTTP/1.1" -> "/index.html"
        std::string request_path = parseRequestPath(request);

        // ============================================
        // STEP 7c: Determine file path to serve
        // ============================================
        // Map the HTTP request path to an actual file on disk
        std::string file_path;
        
        if (request_path == "/" || request_path == "/index.html") {
            // Root path or explicit index.html request -> serve the default HTML file
            file_path = "backend/index.html";
        } else {
            // For other paths (e.g., "/style.css"), remove the leading "/"
            // and serve from the project root directory
            // Example: "/style.css" -> "style.css"
            file_path = request_path.substr(1);
        }

        // ============================================
        // STEP 7d: Read the requested file
        // ============================================
        // Attempt to open the file in binary mode (important for images, etc.)
        // std::ios::in: Open for reading
        // std::ios::binary: Don't translate line endings (important for binary files)
        std::ifstream file(file_path, std::ios::in | std::ios::binary);
        std::string response;  // Will hold the complete HTTP response
        
        if (!file.is_open()) {
            // ============================================
            // File not found - Send 404 Not Found response
            // ============================================
            // HTTP response format:
            // Status line: "HTTP/1.1 404 Not Found\r\n"
            // Headers: "Content-Type: ...\r\n", "Content-Length: ...\r\n", etc.
            // Empty line: "\r\n" (separates headers from body)
            // Body: The actual content
            response = "HTTP/1.1 404 Not Found\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: 13\r\n"      // Length of "404 Not Found"
                      "Connection: close\r\n"        // Close connection after response
                      "\r\n"                         // Empty line (end of headers)
                      "404 Not Found";               // Response body
        } else {
            // ============================================
            // File found - Read contents and build success response
            // ============================================
            // Read the entire file into a string using iterators
            // istreambuf_iterator reads raw bytes, which works for both text and binary files
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();  // Close the file when done reading

            // Determine the appropriate Content-Type based on file extension
            // This tells the browser how to interpret the file
            std::string content_type = getContentType(file_path);
            
            // Build the HTTP 200 OK response with proper headers
            // HTTP/1.1 200 OK: Request was successful
            // Content-Type: Tells browser what type of content this is
            // Content-Length: Size of the content in bytes (required for proper transfer)
            // Connection: close: Close connection after this response
            response = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: " + content_type + "\r\n"
                      "Content-Length: " + std::to_string(content.size()) + "\r\n"
                      "Connection: close\r\n"
                      "\r\n"           // Empty line separates headers from body
                      + content;        // Append the actual file content
        }

        // ============================================
        // STEP 7e: Send HTTP response to client
        // ============================================
        // send() transmits the response string over the network connection
        // The client (browser) will receive this and display/process the content
        ssize_t sent = send(new_socket, response.c_str(), response.size(), 0);
        if (sent < 0) {
            perror("send");
        }

        // ============================================
        // STEP 7f: Close the connection
        // ============================================
        // After sending the response, close this specific connection
        // The server_fd remains open and continues listening for new connections
        close(new_socket);
    }

    // This code is never reached in the current implementation
    // (the while loop runs forever), but it's good practice to include cleanup
    close(server_fd);
    return 0;
}
