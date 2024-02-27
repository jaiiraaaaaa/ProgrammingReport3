#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8081

void check_prime_range(int start, int end, std::vector<int> &primes, int socket, std::mutex &prime_mutex);

void slave_server()
{
    // Create a socket for the server
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    // Set SO_REUSEADDR option
    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Attempt to bind the socket with retry
    while (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        perror("Binding failed");
        sleep(1); // Wait for 1 second before retrying
    }

    // Listen for incoming connections
    if (listen(serverSocket, 1) == -1)
    {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    // Accept connection from the master
    sockaddr_in masterAddress;
    socklen_t masterLength = sizeof(masterAddress);
    int masterSocket = accept(serverSocket, (struct sockaddr *)&masterAddress, &masterLength);

    if (masterSocket == -1)
    {
        perror("Acceptance failed");
        exit(EXIT_FAILURE);
    }

    // Receive the range from the master
    int start, end;
    recv(masterSocket, &start, sizeof(start), 0);
    recv(masterSocket, &end, sizeof(end), 0);

    // Perform prime checking on the assigned range
    std::vector<int> primes;
    std::mutex prime_mutex; // create a local mutex for this thread
    check_prime_range(start, end, primes, masterSocket, prime_mutex);

    // Send the results back to the master
    int primeCount = primes.size();
    send(masterSocket, &primeCount, sizeof(primeCount), 0);
    send(masterSocket, primes.data(), primeCount * sizeof(int), 0);

    // Clean up
    close(masterSocket);
    close(serverSocket);
}

void check_prime_range(int start, int end, std::vector<int> &primes, int socket, std::mutex &prime_mutex)
{
    for (int n = start; n <= end; n++)
    {
        bool is_prime = true;
        for (int i = 2; i * i <= n; i++)
        {
            if (n % i == 0)
            {
                is_prime = false;
                break;
            }
        }
        if (is_prime)
        {
            std::lock_guard<std::mutex> guard(prime_mutex); // locks the mutex for thread safety
            primes.push_back(n);                            // critical section
        }
    }
}

int main()
{
    slave_server();
    return 0;
}
