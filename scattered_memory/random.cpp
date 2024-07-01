#include <iostream>
#include <random>

int main() {
    // Allocate memory for 10,000 integers
    int* a = new int[100000];

    // Initialize the memory with some values
    for(int i = 0; i < 100000; ++i) {
        a[i] = i;
    }

    // Create a random number generator
    std::random_device rd;  // Obtain a random number from hardware
    std::mt19937 gen(rd()); // Seed the generator
    std::uniform_int_distribution<> distr(0, 99999); // Define the range

    // Perform pure random access
    for(int i = 0; i < 100000; ++i) {
        // Generate a random index
        int randomIndex = distr(gen);

        // Access the element at the random index
        int value = a[randomIndex];
        
        // Optionally, perform some operation with the value
        // For demonstration, let's print out the value for the first few accesses
        if (i < 10) {
            std::cout << "a[" << randomIndex << "] = " << value << std::endl;
        }
    }

    // Don't forget to free the allocated memory
    delete[] a;

    return 0;
}

