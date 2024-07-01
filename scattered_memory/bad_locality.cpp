#include <iostream>
#include <vector>

int main() {
    // Allocate a large array
    std::vector<int> a(100000);

    // Initialize the array with some values
    for (int i = 0; i < a.size(); ++i) {
        a[i] = i;
    }

    // Access elements at the beginning and end of the array to demonstrate poor locality
    int sum = 0;
    for (int i = 0; i < 50000; ++i) { // Half the size for equal number of accesses
        sum += a[i]; // Access near the beginning
        sum += a[a.size() - 1 - i]; // Access far from the previous access, at the end
    }

    std::cout << "Sum: " << sum << std::endl;

    return 0;
}

