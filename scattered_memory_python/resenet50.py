import time
import random

def fake_convolution(input_size, filter_size, output_size, num_filters):
    # Simulate a convolution operation computationally
    return [[random.random() for _ in range(output_size)] for _ in range(num_filters)]

def fake_fully_connected(input_size, output_size):
    # Simulate a fully connected layer operation
    return [random.random() for _ in range(output_size)]

def fake_activation(input_data):
    # Simulate an activation function (like ReLU)
    if isinstance(input_data[0], list):  # Check if input_data is a list of lists
        return [max(0, item) for sublist in input_data for item in sublist]
    else:
        return [max(0, x) for x in input_data]

def simulate_resnet50_inference(num_classes=1000):
    # Simulate an inference through the ResNet-50 architecture
    input_data = [random.random() for _ in range(224 * 224 * 3)]  # Example input size for ImageNet
    
    # Simulate convolutional layers
    for _ in range(50):  # Simplified loop, real ResNet-50 has a specific structure
        input_data = fake_convolution(224, 3, 224, 64)
        # Flatten the list of lists into a single list before activation
        input_data = [item for sublist in input_data for item in sublist]
        input_data = fake_activation(input_data)

    # Simulate global average pooling (GAP)
    pooled_data = fake_fully_connected(224*224*64, 2048)
    
    # Simulate fully connected layers
    output_data = fake_fully_connected(2048, num_classes)

    return fake_activation(output_data)  # Simulate softmax activation

# Measure execution time
start_time = time.time()
output = simulate_resnet50_inference()
end_time = time.time()

# Output execution time
print(f"Fake ResNet-50 inference took {end_time - start_time} seconds.")

