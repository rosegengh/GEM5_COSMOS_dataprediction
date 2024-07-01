#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ARRAY_SIZE (512 * 512 * 512) // 1 GB
#define BUSY_WAIT_ITERATIONS 10000 // Adjust this value as needed
#define NUMBER_OF_ACCESSES 10000000 // Number of memory accesses to perform

// Function to create a delay.
void busy_wait() {
    volatile int i;
    for (i = 0; i < BUSY_WAIT_ITERATIONS; i++) {
        // Do nothing.
    }
}

int main() {
    // Allocate the array.
    int *array = malloc(ARRAY_SIZE);
    if (array == NULL) {
        printf("Failed to allocate memory!\n");
        return 1;
    }

    // Seed the random number generator.
    srand(time(NULL));

    // Perform a fixed number of memory accesses.
    for (int access_count = 0; access_count < NUMBER_OF_ACCESSES; access_count++) {
        int index = rand() % (ARRAY_SIZE / sizeof(int));
        array[index] = rand(); // Write to the array.
        int value = array[index]; // Read from the array.

       // printf("Access %d: index = %d, value = %d\n", access_count, index, value);

        // Delay.
        //busy_wait();
    }

    // Clean up and exit.
    free(array);
    return 0;
}
