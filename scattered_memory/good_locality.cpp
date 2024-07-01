#include <iostream>
#include <vector>

int main() {
    // Allocate a large array
    std::vector<int> a(1000000000);

    // Initialize the array with some values
    for (int i = 0; i < a.size(); ++i) {
        a[i] = i;
    }

    // Access a small subset of the array multiple times to demonstrate good locality
    int sum = 0;
    for (int repeat = 0; repeat < 100; ++repeat) { // Repeat to improve temporal locality
        for (int i = 0; i < 100; ++i) { // Access only the first 100 elements to improve spatial locality
            sum += a[i];
        }
    }

    std::cout << "Sum: " << sum << std::endl;

    return 0;
}

