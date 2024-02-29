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

    std::cout << "Successfully connected with the master server." << std::endl;

    int start, end;
    recv(masterSocket, &start, sizeof(start), 0);
    recv(masterSocket, &end, sizeof(end), 0);

    std::cout << "Received range of values from master: [" << start << ", " << end << "]" << std::endl;

    std::vector<int> primes;
    std::mutex prime_mutex; // create a local mutex for this thread

    auto start_time = std::chrono::high_resolution_clock::now();

    check_prime_range(start, end, primes, masterSocket, prime_mutex);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Completed checking primes. Time taken: " << duration << " milliseconds." << std::endl;

    int primeCount = primes.size();
    std::cout << "Number of primes computed by the slave: " << primeCount << std::endl;

    send(masterSocket, &primeCount, sizeof(primeCount), 0);
    send(masterSocket, primes.data(), primeCount * sizeof(int), 0);

    std::cout << "Sent the list/number of primes to the master server." << std::endl;

    // Shutdown and close the sockets
    shutdown(masterSocket, SHUT_RDWR);
    shutdown(serverSocket, SHUT_RDWR);
    
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
            if (socket != -1)
            {
                // Communication with master
                std::lock_guard<std::mutex> guard(prime_mutex); // locks the mutex for thread safety
                primes.push_back(n);                            // critical section
            }
            else
            {
                // Local computation by the slave
                primes.push_back(n);
            }
        }
    }
}

int main()
{
    slave_server();
    return 0;
}
