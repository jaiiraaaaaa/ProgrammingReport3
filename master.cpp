#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8081
#define LIMIT 10000000
std::mutex prime_mutex;  // mutex lock for mutual exclusion and thread safety

void check_prime_range(int start, int end, std::vector<int> &primes, int socket, std::mutex &prime_mutex);

int main() {
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

    int chunk = upperBound / numThreads;

    // Create socket
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Attempt to bind the socket with retry
    while (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("Binding failed");
        sleep(1); // Wait for 1 second before retrying
    }

    if (listen(server_fd, numThreads) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Master Server is listening on port " << PORT << std::endl;

    for (int i = 0; i < numThreads; ++i) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        std::mutex &local_mutex = prime_mutex; // capture the reference locally
        int local_numThreads = numThreads;    // capture the value locally
        threads.push_back(std::thread([i, &primes, new_socket, chunk, upperBound, local_numThreads, &local_mutex]() {
            check_prime_range(i * chunk + (i == 0 ? 2 : 1), (i == local_numThreads - 1) ? upperBound : (i + 1) * chunk, primes, new_socket, local_mutex);
        }));
    }

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
            std::lock_guard<std::mutex> guard(prime_mutex); // locks the mutex for thread safety
            primes.push_back(n);                            // critical section
        }
    }
}
