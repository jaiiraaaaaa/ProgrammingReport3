#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 9001

void check_prime_range(int start, int end, std::vector<int> &primes, int socket, std::mutex &prime_mutex);

void handle_connection(int serverSocket);

void slave_server()
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    while (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        perror("Binding failed");
        sleep(1); // Wait for 1 second before retrying
    }

    if (listen(serverSocket, 1) == -1)
    {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Slave Server is listening on port " << PORT << std::endl;

    handle_connection(serverSocket);

    // Clean up
    close(serverSocket);
}

void handle_connection(int serverSocket)
{
    sockaddr_in masterAddress;
    socklen_t masterLength = sizeof(masterAddress);
    int masterSocket = accept(serverSocket, (struct sockaddr *)&masterAddress, &masterLength);

    if (masterSocket == -1)
    {
        perror("Acceptance failed");
        exit(EXIT_FAILURE);
    }

    int start, end;
    recv(masterSocket, &start, sizeof(start), 0);
    recv(masterSocket, &end, sizeof(end), 0);

    std::vector<int> primes;
    std::mutex prime_mutex; // create a local mutex for this thread
    check_prime_range(start, end, primes, masterSocket, prime_mutex);

    int primeCount = primes.size();
    send(masterSocket, &primeCount, sizeof(primeCount), 0);
    send(masterSocket, primes.data(), primeCount * sizeof(int), 0);

    // Clean up
    close(masterSocket);
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
            std::lock_guard<std::mutex> guard(prime_mutex);
            primes.push_back(n);
        }
    }
}

int main()
{
    slave_server();
    return 0;
}
