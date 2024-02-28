#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT_MASTER 9000
#define PORT_SLAVE 9001
#define LIMIT 10000000
std::mutex prime_mutex;  // mutex lock for mutual exclusion and thread safety

void check_prime_range(int start, int end, std::vector<int> &primes, int socket, std::mutex &prime_mutex);

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

    if (useSlave == 'y') {
        // Create socket for communication with slave
        int slave_socket;
        if ((slave_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in slave_address;
        slave_address.sin_family = AF_INET;
        slave_address.sin_addr.s_addr = INADDR_ANY;
        slave_address.sin_port = htons(PORT_SLAVE);

        // Connect to the slave server
        if (connect(slave_socket, (struct sockaddr*)&slave_address, sizeof(slave_address)) < 0) {
            perror("Connection to slave failed");
            exit(EXIT_FAILURE);
        }

        // Send the range to the slave
        int start = 2;
        int end = upperBound / 2;
        send(slave_socket, &start, sizeof(start), 0);
        send(slave_socket, &end, sizeof(end), 0);

        // Close the socket to the slave
        close(slave_socket);
    }

    // Master's task division
    std::mutex &local_mutex = prime_mutex; // capture the reference locally
    int local_numThreads = numThreads;    // capture the value locally

    for (int i = 0; i < numThreads; ++i) {
        threads.push_back(std::thread([&primes, chunk, upperBound, local_numThreads, &local_mutex, i, useSlave]() {
            check_prime_range(i * chunk + (i == 0 ? 2 : 1) + (useSlave == 'y' ? upperBound / 2 : 0), (i == local_numThreads - 1) ? upperBound : (i + 1) * chunk + (useSlave == 'y' ? upperBound / 2 : 0), primes, -1, local_mutex);
        }));
    }

    // Wait for threads to complete
    for (auto &th : threads) {
        th.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << primes.size() << " primes were found." << std::endl;
    std::cout << "Time taken: " << duration << " milliseconds." << std::endl;

    return 0;
}

void check_prime_range(int start, int end, std::vector<int> &primes, int socket, std::mutex &prime_mutex) {
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
                std::lock_guard<std::mutex> guard(prime_mutex); // locks the mutex for thread safety
                primes.push_back(n);                            // critical section
            } else {
                // Local computation by the master
                primes.push_back(n);
            }
        }
    }
}
