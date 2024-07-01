#include <iostream>

int main() {
    // Dynamically allocate memory for an integer
    int* value = new int(42); // Example variable

    for (int i = 0; i < 100000000; ++i) {
        // Read the value from the allocated memory
        int readValue = *value;

        // Optionally, print the read value
        //std::cout << "Read value: " << readValue << std::endl;
    }

    //std::cout << "Finished reading memory address 10 times." << std::endl;

    // Free the allocated memory
    delete value;

    return 0;
}

