#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <cmath>

#define PORT 9001

bool is_prime(int n) {
    if (n <= 1) {
        return false;
    }

    for (int i = 2; i <= std::sqrt(n); ++i) {
        if (n % i == 0) {
            return false;
        }
    }

    return true;
}

void check_prime_range(int start, int end, std::vector<int> &primes, int socket, std::mutex &prime_mutex);

void handle_connection(int serverSocket);

void print_and_verify_primes(int start, int end, int primeCount) {
    std::vector<int> primesLocally;
    for (int n = start; n <= end; n++) {
        if (is_prime(n)) {
            primesLocally.push_back(n);
        }
    }

    // Print the number of primes computed locally
    std::cout << "\nNumber of primes computed locally: " << primesLocally.size() << std::endl;

    // Print the number of primes computed through check_prime_range
    std::cout << "Number of primes computed through check_prime_range: " << primeCount << std::endl;

    // Print TRUE if the counts match, otherwise print FALSE
    if (primesLocally.size() == static_cast<size_t>(primeCount)) {
        std::cout << "SANITY CHECK: TRUE" << std::endl;
    } else {
        std::cout << "SANITY CHECK: FALSE" << std::endl;
    }
}


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

    std::cout << "\nSuccessfully connected with the master server." << std::endl;

    // Receive the number of threads from the master
    int numThreads;
    recv(masterSocket, &numThreads, sizeof(numThreads), 0);
    std::cout << "Received number of threads from master: " << numThreads << std::endl;

    int start, end;
    recv(masterSocket, &start, sizeof(start), 0);
    recv(masterSocket, &end, sizeof(end), 0);

    std::cout << "Received range of values from master: [" << start << ", " << end << "]" << std::endl;

    std::vector<int> primes;
    std::mutex prime_mutex; // create a local mutex for this thread

    auto start_time = std::chrono::high_resolution_clock::now();

    // Create threads for prime checking
    std::vector<std::thread> threads;
    int chunk = (end - start + 1) / numThreads;

    for (int i = 0; i < numThreads; ++i)
    {
        threads.push_back(std::thread([&, i]() {
            int thread_start = start + i * chunk;
            int thread_end = (i == numThreads - 1) ? end : thread_start + chunk - 1;

            try
            {
                // Use std::lock_guard for thread safety
                std::lock_guard<std::mutex> guard(prime_mutex);
                check_prime_range(thread_start, thread_end, primes, -1, prime_mutex);
            }
            catch (const std::exception &ex)
            {
                // Handle exceptions that might occur during thread execution
                std::cerr << "Thread exception: " << ex.what() << std::endl;
            }
        }));
    }

    // Wait for threads to complete
    for (auto &th : threads)
    {
        th.join();
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Completed checking primes. Time taken: " << duration << " milliseconds." << std::endl;

    int primeCount = primes.size();

    // Print and verify primes locally
    print_and_verify_primes(start, end, primeCount);

    send(masterSocket, &primeCount, sizeof(primeCount), 0);
    send(masterSocket, primes.data(), primeCount * sizeof(int), 0);

    std::cout << "\nSent the list/number of primes to the master server." << std::endl;

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
