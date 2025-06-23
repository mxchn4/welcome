#include <iostream>    // For input/output operations (e.g., std::cout, std::endl)
#include <string>      // For std::string manipulation
#include <cstring>     // For C-style string functions like memset
#include <unistd.h>    // For POSIX API functions, specifically close() for sockets
#include <sys/socket.h> // For socket API functions (socket, bind, listen, accept)
#include <netinet/in.h> // For internet address family structures (sockaddr_in, INADDR_ANY)
#include <cstdlib>     // For exit() and EXIT_FAILURE
#include <sstream>     // For std::ostringstream to build HTTP response
#include <algorithm>   // For std::min

// Define the port number the C++ backend server will listen on
const int PORT = 8080;
// Define the buffer size for receiving data (HTTP requests can be larger than simple names)
const int BUFFER_SIZE = 4096;

// Function to extract the 'name' value from a URL query string
// Example: "GET /welcome?name=Alice&city=NY HTTP/1.1" -> "Alice"
std::string extract_name_from_query(const std::string& request_line) {
    std::string name_prefix = "name=";

    // Find the start of the actual path/query string (after "GET ")
    size_t path_start_pos = request_line.find(" "); // Find the first space (after "GET")
    if (path_start_pos == std::string::npos) {
        return "Guest"; // Malformed request line
    }
    path_start_pos++; // Move past the space to the start of the path

    // Find the end of the path/query string (before " HTTP/X.X")
    size_t path_end_pos = request_line.find(" HTTP/", path_start_pos);
    std::string full_path;
    if (path_end_pos == std::string::npos) {
        // If " HTTP/" not found, take until the end of the line (unlikely for valid HTTP)
        full_path = request_line.substr(path_start_pos);
    } else {
        // Extract the path and query string part, excluding the " HTTP/X.X"
        full_path = request_line.substr(path_start_pos, path_end_pos - path_start_pos);
    }

    // Now, parse the query string from the extracted full_path
    size_t query_start = full_path.find("?");
    if (query_start != std::string::npos) {
        std::string query_string = full_path.substr(query_start + 1);
        size_t name_pos = query_string.find(name_prefix);
        if (name_pos != std::string::npos) {
            size_t name_value_start = name_pos + name_prefix.length();
            size_t name_value_end = query_string.find("&", name_value_start);
            std::string encoded_name;
            if (name_value_end == std::string::npos) {
                // If no '&' found, it means 'name' is the last or only parameter
                encoded_name = query_string.substr(name_value_start);
            } else {
                // Extract the name value between "name=" and the next "&"
                encoded_name = query_string.substr(name_value_start, name_value_end - name_value_start);
            }

            // Basic URL decode: replace '+' with ' ' (for spaces)
            size_t plus_pos = encoded_name.find('+');
            while (plus_pos != std::string::npos) {
                encoded_name.replace(plus_pos, 1, " ");
                plus_pos = encoded_name.find('+', plus_pos + 1);
            }
            return encoded_name;
        }
    }
    return "Guest"; // Default name if not found in query
}

int main() {
    // Declare variables for server socket, new client socket, and address structures
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1; // Option for setsockopt to allow reuse of address/port
    socklen_t addrlen = sizeof(address); // Size of the address structure
    char buffer[BUFFER_SIZE] = {0}; // Buffer to store incoming HTTP request data

    // 1. Create socket file descriptor
    // AF_INET: IPv4 address family
    // SOCK_STREAM: Stream socket (TCP)
    // 0: Default protocol for SOCK_STREAM (TCP)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error: Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Optional: Set socket options to reuse address and port immediately after closing
    // This is useful for development, allowing quick server restarts without "Address already in use" errors.
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Error: setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Configure the server address structure
    address.sin_family = AF_INET;           // Use IPv4
    address.sin_addr.s_addr = INADDR_ANY;   // Listen on all available network interfaces (0.0.0.0)
    address.sin_port = htons(PORT);         // Convert port number to network byte order

    // 2. Bind the socket to the specified IP address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Error: Bind failed");
        exit(EXIT_FAILURE);
    }

    // 3. Listen for incoming connections
    // The second argument (10) is the backlog queue size, defining how many pending connections
    // can be in the queue before the server starts refusing new connections.
    if (listen(server_fd, 10) < 0) {
        perror("Error: Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "C++ Backend Server listening on port " << PORT << std::endl;
    std::cout << "Waiting for incoming HTTP requests (e.g., from Nginx frontend)..." << std::endl;

    // Main loop to continuously accept and handle client connections (HTTP requests)
    while (true) {
        // 4. Accept a new connection
        // This call blocks until an incoming connection is present.
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("Error: Accept failed");
            continue; // Continue listening for other connections
        }

        std::cout << "\nConnection accepted from a client (likely Nginx)." << std::endl;

        // Clear the buffer to prevent data from previous requests from interfering
        memset(buffer, 0, BUFFER_SIZE);

        // 5. Read the entire HTTP request from the client (Nginx)
        // Read up to BUFFER_SIZE - 1 bytes to ensure space for null termination.
        ssize_t bytes_read = read(new_socket, buffer, BUFFER_SIZE - 1);
        if (bytes_read < 0) {
            perror("Error: Read failed");
            close(new_socket); // Close the client socket on error
            continue;
        }
        if (bytes_read == 0) {
            std::cout << "Client disconnected unexpectedly (read 0 bytes)." << std::endl;
            close(new_socket);
            continue;
        }

        buffer[bytes_read] = '\0'; // Null-terminate the received data
        std::string http_request(buffer); // Convert C-style string to std::string

        std::cout << "--- Received HTTP Request (first few lines) ---" << std::endl;
        // Print the first 500 characters of the request for debugging
        std::cout << http_request.substr(0, std::min((int)http_request.length(), 500)) << (http_request.length() > 500 ? "..." : "") << std::endl;
        std::cout << "-------------------------------------------------" << std::endl;

        std::string name = "Guest"; // Default name
        std::string request_line;
        std::istringstream request_stream(http_request);
        std::getline(request_stream, request_line); // Get the first line of the HTTP request (e.g., "GET /welcome?name=Alice HTTP/1.1")

        // Check if the request is a GET request to the /welcome path
        size_t get_welcome_pos = request_line.find("GET /welcome");
        if (get_welcome_pos != std::string::npos) {
            name = extract_name_from_query(request_line);
            // Basic sanitization: limit name length to prevent very long strings
            if (name.length() > 100) {
                name = name.substr(0, 100);
            }
        } else {
            std::cout << "Warning: Request not in expected 'GET /welcome?name=...' format. Using default name 'Guest'." << std::endl;
        }

        // Construct the HTTP response body
        std::string welcome_message_body = "Welcome, " + name + "! Glad to have you.";

        // Construct the full HTTP response with headers
        std::ostringstream http_response_stream;
        http_response_stream << "HTTP/1.1 200 OK\r\n"; // HTTP status line
        http_response_stream << "Content-Type: text/plain\r\n"; // Specify content type
        http_response_stream << "Content-Length: " << welcome_message_body.length() << "\r\n"; // Content length
        http_response_stream << "Connection: close\r\n"; // Instruct the client to close connection after response
        http_response_stream << "\r\n"; // Empty line signifies end of HTTP headers
        http_response_stream << welcome_message_body; // The actual response content

        std::string http_response = http_response_stream.str();

        // Send the HTTP response back to the client (Nginx)
        if (write(new_socket, http_response.c_str(), http_response.length()) < 0) {
            perror("Error: Write failed");
        } else {
            std::cout << "Sent HTTP response to Nginx for name: '" << name << "'." << std::endl;
        }

        // 8. Close the current client socket
        // Crucial to close the socket to free resources and allow new connections.
        close(new_socket);
        std::cout << "Client connection closed." << std::endl;
    }

    // This part is technically unreachable in the current infinite loop,
    // but included for completeness in case a graceful shutdown mechanism is added.
    close(server_fd); // Close the main listening server socket
    return 0; // Indicate successful execution
}