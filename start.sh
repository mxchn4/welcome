#!/bin/bash

# This script starts both the C++ backend server and Nginx.
# It's used as the entrypoint for the Docker container.

echo "Starting C++ backend server..."
# Start the C++ backend server in the background
# The '&' sends the process to the background, allowing the script to continue.
./server_app &

echo "Starting Nginx frontend..."
# Start Nginx in the foreground.
# 'daemon off;' ensures Nginx runs in the foreground,
# which is necessary for Docker containers to remain running.
nginx -g 'daemon off;'

# Note: If server_app crashes, Nginx will still run.
# If Nginx crashes, the container will exit.
# For more robust multi-process management, consider using 'supervisord'.