#include <iostream>
#include <vector>
#include <limits>
#include <thread>
#include <mutex>
#include <chrono>

#define LIMIT 10000000

/*
This function checks if an integer n is prime.

Parameters:
n : int - integer to check

Returns true if n is prime, and false otherwise.
*/
void check_prime_range(int start, int end, std::vector<int> &primes);

int main()
{
   int upperBound, numThreads;
   std::cout << "Enter the upper bound for checking primes (upper bound is " << LIMIT << "): ";
   while (!(std::cin >> upperBound) || upperBound < 2 || upperBound > LIMIT) // input validation, should be integer and > 1 && <= LIMIT
   {
      std::cout << "Invalid input. Please enter an integer greater than 1 and less than " << LIMIT << ": ";
      std::cin.clear();                                                   // clear error flag
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // clear input buffer
   }

   std::cout << "Enter the number of threads (1 or more): ";
   while (!(std::cin >> numThreads) || numThreads < 1) // input validation, should be integer and  > 0
   {
      std::cout << "Invalid input. Please enter an integer that is 1 or more: ";
      std::cin.clear();                                                   // clear error flag
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // clear input buffer
   }

   auto start_time = std::chrono::high_resolution_clock::now(); // start time after user input is collected

   std::vector<int> primes;
   std::vector<std::thread> threads;

   int chunk = upperBound / numThreads; // divide workload to chunks for each thread
   for (int i = 0; i < numThreads; ++i)
   {
      int start = i * chunk + (i == 0 ? 2 : 1);                                        // start at 2 for the first chunk
      int end = (i == numThreads - 1) ? upperBound : (i + 1) * chunk;                  // if last thread, end at upperBound; otherwise, just before start of next chunk
      threads.push_back(std::thread(check_prime_range, start, end, std::ref(primes))); // creating and starting threads with check_prime function
   }

   for (auto &th : threads)
   {
      th.join(); // blocking call to ensure all threads finish before continuing
   }

   auto end_time = std::chrono::high_resolution_clock::now(); // stop timer before printing the number of primes found

   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(); // calculating and converting the duration to milliseconds

   std::cout << primes.size() << " primes were found." << std::endl;
   std::cout << "Time taken: " << duration << " milliseconds." << std::endl;
}

std::mutex prime_mutex; // mutex lock for mutual exclusion and thread safety

void check_prime_range(int start, int end, std::vector<int> &primes)
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
