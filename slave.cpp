#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <winsock2.h>
#include <unistd.h>

#define PORT 49152

void check_prime_range(int start, int end, std::vector<int> &primes, int socket, std::mutex &prime_mutex);

void handle_connection(int serverSocket);

void initializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed with error: " << result << std::endl;
        exit(EXIT_FAILURE);
    }
}

void slave_server() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Binding loop
    while (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) {
        perror("Binding failed");
        sleep(1); // Wait for 1 second before retrying
    }

    std::cout << "Slave Server is listening on port " << PORT << std::endl;

    // Listen for incoming connections
    if (listen(serverSocket, SOMAXCONN) == -1) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    // Accept incoming connections
    handle_connection(serverSocket);

    // Clean up
    close(serverSocket);
}


void handle_connection(int serverSocket) {
    sockaddr_in masterAddress;
    int masterLength = sizeof(masterAddress);
    int masterSocket = accept(serverSocket, (struct sockaddr *)&masterAddress, &masterLength);
    if (masterSocket == -1) {
        perror("Acceptance failed");
        std::cerr << "Error: " << WSAGetLastError() << std::endl; // Print specific error
        exit(EXIT_FAILURE);
    }


    std::cout << "Successfully connected with the master server." << std::endl;

    int start, end;
    recv(masterSocket, (char*)&start, sizeof(start), 0);
    recv(masterSocket, (char*)&end, sizeof(end), 0);

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

    send(masterSocket, (char*)&primeCount, sizeof(primeCount), 0);
    send(masterSocket, (char*)primes.data(), primeCount * sizeof(int), 0);

    std::cout << "Sent the list/number of primes to the master server." << std::endl;

    // Shutdown and close the sockets
    shutdown(masterSocket, SD_BOTH);
    shutdown(serverSocket, SD_BOTH);
    
    // Clean up
    closesocket(masterSocket);
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
                // Communication with master
                std::lock_guard<std::mutex> guard(prime_mutex);
                primes.push_back(n);
            } else {
                // Local computation by the slave
                primes.push_back(n);
            }
        }
    }
}

int main() {
    initializeWinsock();
    slave_server();
    return 0;
}