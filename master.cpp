#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <limits>
#include <mutex>
#include <winsock2.h>

#define PORT_MASTER 49153
#define PORT_SLAVE 49152
#define LIMIT 10000000

std::mutex prime_mutex;

void check_prime_range(int start, int end, std::vector<int> &primes, int socket, std::mutex &prime_mutex);

void cleanupSocket(SOCKET socket) {
    shutdown(socket, SD_BOTH);
    closesocket(socket);
}

int main() {
    char useSlave;
    std::cout << "Do you want to use a slave server? (y/n): ";
    std::cin >> useSlave;

    int upperBound, numThreads;
    std::cout << "Enter the upper bound for checking primes (upper bound is " << LIMIT << "): ";
    while (!(std::cin >> upperBound) || upperBound < 2 || upperBound > LIMIT) {
        std::cout << "Invalid input. Please enter an integer greater than 1 and less than " << LIMIT << ": ";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    std::cout << "Enter the number of threads (1 or more): ";
    while (!(std::cin >> numThreads) || numThreads < 1) {
        std::cout << "Invalid input. Please enter an integer that is 1 or more: ";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<int> primes;
    std::vector<std::thread> threads;

    int chunk = upperBound / (useSlave == 'y' ? 2 : 1) / numThreads;  // Divide by 2 if using a slave
    SOCKET slave_socket = INVALID_SOCKET; // Initialize to invalid socket

    if (useSlave == 'y') {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return EXIT_FAILURE;
        }

        if ((slave_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            std::cerr << "Socket creation failed" << std::endl;
            WSACleanup();
            return EXIT_FAILURE;
        }

        int reuse = 1;
        if (setsockopt(slave_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse)) < 0) {
            std::cerr << "setsockopt failed" << std::endl;
            cleanupSocket(slave_socket);
            WSACleanup();
            return EXIT_FAILURE;
        }

        struct sockaddr_in slave_address;
        slave_address.sin_family = AF_INET;
        slave_address.sin_addr.s_addr = INADDR_LOOPBACK; // Use localhost address
        slave_address.sin_port = htons(PORT_SLAVE);

        // Connect to the slave server
        if (connect(slave_socket, (struct sockaddr*)&slave_address, sizeof(slave_address)) == SOCKET_ERROR) {
            int error_code = WSAGetLastError();
            std::cerr << "Connection to slave failed with error code: " << error_code << std::endl;
            switch (error_code) {
                case WSANOTINITIALISED:
                    std::cerr << "Winsock not initialized." << std::endl;
                    break;
                case WSAENETDOWN:
                    std::cerr << "Network subsystem failed." << std::endl;
                    break;
                // Add more cases to handle specific error codes if necessary
                default:
                    std::cerr << "Unknown error." << std::endl;
                    break;
            }
            cleanupSocket(slave_socket);
            WSACleanup();
            return EXIT_FAILURE;
        }

        // Send the range to the slave
        int start = 2;
        int end = upperBound / 2;
        if (send(slave_socket, reinterpret_cast<const char*>(&start), sizeof(start), 0) == SOCKET_ERROR ||
            send(slave_socket, reinterpret_cast<const char*>(&end), sizeof(end), 0) == SOCKET_ERROR) {
            std::cerr << "Failed to send data to slave" << std::endl;
            cleanupSocket(slave_socket);
            WSACleanup();
            return EXIT_FAILURE;
        }
    }

    // Master's task division
    std::mutex &local_mutex = prime_mutex; // capture the reference locally
    int local_numThreads = numThreads;    // capture the value locally

    for (int i = 0; i < numThreads; ++i) {
        threads.push_back(std::thread([&primes, chunk, upperBound, local_numThreads, &local_mutex, i, useSlave]() {
            check_prime_range(i * chunk + (i == 0 ? 2 : 1) + (useSlave == 'y' ? upperBound / 2 : 0), 
                              (i == local_numThreads - 1) ? upperBound : (i + 1) * chunk + (useSlave == 'y' ? upperBound / 2 : 0), 
                              primes, -1, local_mutex);
        }));
    }

    // Wait for threads to complete
    for (auto &th : threads) {
        th.join();
    }

    // Communication with slave (if used)
    if (useSlave == 'y') {
        // Receive the count and list of primes from the slave
        int slavePrimeCount;
        if (recv(slave_socket, reinterpret_cast<char*>(&slavePrimeCount), sizeof(slavePrimeCount), 0) == SOCKET_ERROR) {
            std::cerr << "Failed to receive prime count from slave" << std::endl;
            cleanupSocket(slave_socket);
            WSACleanup();
            return EXIT_FAILURE;
        }

        std::vector<int> slavePrimes(slavePrimeCount);
        if (recv(slave_socket, reinterpret_cast<char*>(slavePrimes.data()), slavePrimeCount * sizeof(int), 0) == SOCKET_ERROR) {
            std::cerr << "Failed to receive primes from slave" << std::endl;
            cleanupSocket(slave_socket);
            WSACleanup();
            return EXIT_FAILURE;
        }

        // Combine the results from the master and the slave
        primes.insert(primes.end(), slavePrimes.begin(), slavePrimes.end());

        std::cout << "Number of primes received from the slave: " << slavePrimeCount << std::endl;

        cleanupSocket(slave_socket);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << primes.size() << " primes were found." << std::endl;
    std::cout << "Time taken: " << duration << " milliseconds." << std::endl;

    WSACleanup(); // Clean up Winsock

    return 0;
}

void check_prime_range(int start, int end, std::vector<int> &primes, int socket, std::mutex &prime_mutex) {
    std::cout << "Checking range: [" << start << ", " << end << "]" << std::endl;

    for (int n = start; n <= end; n++) {
        bool is_prime = true;
        for (int i = 2; i * i <= n; i++) {
            if (n % i == 0) {
                is_prime = false;
                break;
            }
        }
        if (is_prime) {
            if (socket != -1) {
                // Communication with slave
                std::lock_guard<std::mutex> guard(prime_mutex);
                primes.push_back(n);
            } else {
                // Local computation by the master
                {
                    // Use a local lock guard to minimize the lock duration
                    std::lock_guard<std::mutex> guard(prime_mutex);
                    primes.push_back(n);
                }
            }
        }
    }

    std::cout << "Range checked: [" << start << ", " << end << "]" << std::endl;
}
