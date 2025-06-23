# Use an official Ubuntu image as the base for our container
FROM ubuntu:latest

# Set environment variables for non-interactive installations
ENV DEBIAN_FRONTEND=noninteractive

# Update the package list and install necessary packages:
# build-essential: Includes g++ and other tools for compiling C++ applications.
# nginx: The web server that will act as our frontend.
# rm -rf /var/lib/apt/lists/*: Cleans up apt cache to reduce image size.
RUN apt-get update && \
    apt-get install -y build-essential nginx && \
    rm -rf /var/lib/apt/lists/*

# Set the working directory inside the container to /app
WORKDIR /app

# Copy the C++ source file into the container at /app
COPY server_app.cpp .

# Compile the C++ application
# -o server_app: Specifies the output executable name.
# -Wall: Enables all common warning messages.
RUN g++ -o server_app server_app.cpp -Wall

# Copy the Nginx configuration file into the standard Nginx configuration directory
COPY nginx.conf /etc/nginx/nginx.conf

# Copy the static HTML file into Nginx's default web root directory
# This ensures Nginx can find and serve index.html
COPY index.html /usr/share/nginx/html/index.html

# Copy the startup script into the working directory
COPY start.sh .

# Expose port 80. This tells Docker that the container listens on the specified network ports
# at runtime.
EXPOSE 80

# Make the startup script executable
RUN chmod +x start.sh

# Define the command that will be executed when the container starts
CMD ["./start.sh"]