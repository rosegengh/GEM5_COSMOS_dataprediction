#include <iostream>
#include <vector>
#include <algorithm> // For std::max
#include <cstring>   // For memcpy

// Define a simple matrix class for our operations
class Matrix {
public:
    int rows, cols;
    std::vector<float> data;

    Matrix(int r, int c) : rows(r), cols(c), data(r * c) {}

    float& operator()(int r, int c) {
        return data[r * cols + c];
    }

    const float& operator()(int r, int c) const {
        return data[r * cols + c];
    }
};

// Perform matrix-vector multiplication
std::vector<float> matVecMultiply(const Matrix& mat, const std::vector<float>& vec) {
    std::vector<float> result(mat.rows, 0.0f);
    for (int i = 0; i < mat.rows; ++i) {
        for (int j = 0; j < mat.cols; ++j) {
            result[i] += mat(i, j) * vec[j];
        }
    }
    return result;
}

// ReLU activation function
void relu(std::vector<float>& vec) {
    std::transform(vec.begin(), vec.end(), vec.begin(),
                   [](float x) { return std::max(0.0f, x); });
}

// Fake ResNet20 Inference
void fakeResNet20Inference(std::vector<float>& input, int numIterations) {
    const int numLayers = 20; // Number of layers in ResNet20

    // Process through layers
    for (int iter = 0; iter < numIterations; ++iter) {
        for (int i = 0; i < numLayers; ++i) {
            // Creating a fake weight matrix for each layer
            Matrix weights(input.size(), input.size()); // Square matrix for simplicity

            // Initialize weights with fake data
            std::fill(weights.data.begin(), weights.data.end(), 0.1f); // Using 0.1 as fake data

            // Perform matrix-vector multiplication (MVM)
            input = matVecMultiply(weights, input);

            // Apply ReLU activation
            relu(input);
        }
    }
}

int main() {
    // Assuming an input size of 64 for simplicity
    const int inputSize = 64;
    const int numIterations = 20;

    // Allocate memory for fake data
    float* fakeData = new float[inputSize];
    std::fill_n(fakeData, inputSize, 1.0f); // Initialize fake data with 1.0

    // Copy data from allocated memory to vector for processing
    std::vector<float> input(fakeData, fakeData + inputSize);

    // Run fake ResNet20 inference 20 times
    fakeResNet20Inference(input, numIterations);

    // Copy processed data back to allocated memory
    std::memcpy(fakeData, input.data(), inputSize * sizeof(float));

    // Output the final result (fake, for demonstration)
    std::cout << "Output of fake ResNet20 inference after " << numIterations << " iterations:" << std::endl;
    for (int i = 0; i < inputSize; ++i) {
        std::cout << fakeData[i] << " ";
    }
    std::cout << std::endl;

    // Free the allocated memory
    delete[] fakeData;

    return 0;
}

