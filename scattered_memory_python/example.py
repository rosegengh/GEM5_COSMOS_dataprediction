# simple_gem5_workload.py

import time

def compute_factorial(n):
    # A simple function to compute factorial
    if n == 0:
        return 1
    else:
        return n * compute_factorial(n-1)

def write_to_file(filename, content):
    # Simulates file I/O
    with open(filename, 'w') as file:
        file.write(content)

def main():
    start_time = time.time()

    # Perform some computations
    number = 10
    fact = compute_factorial(number)
    print(f"The factorial of {number} is {fact}")

    # Write results to a file
    write_to_file('output.txt', f'Factorial: {fact}\n')

    end_time = time.time()
    print(f"Execution time: {end_time - start_time} seconds")

if __name__ == "__main__":
    main()

