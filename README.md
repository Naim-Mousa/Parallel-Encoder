# Parallel-Encoder

## Description
This project is an implementation of Run-Length Encoding (RLE), a simple form of lossless data compression, with support for multithreading. The encoder can process large files efficiently by dividing the workload across multiple threads, significantly speeding up the encoding process.

## Features


## Features
- **Run-Length Encoding**: Efficiently encodes files using the RLE algorithm.
- **Multithreading Support**: Leverages multiple threads to accelerate the encoding process.
- **Memory Mapping**: Utilizes memory mapping for efficient file processing.
- **Thread Synchronization**: Implements mutexes and condition variables for thread synchronization.

## Requirements
- A C compiler (e.g., GCC)
- POSIX-compliant environment (Linux/Unix)
- Pthread library for threading support

## Installation
1. **Clone the Repository**:
`git clone https://github.com/yourusername/Parallel-Encoder.git`
2. **Navigate to the Directory**:
`cd Parallel-Encoder`
3. **Compile the Program**:
`Make`

## Usage
- To encode one or more files **without** threads, simply run:
  `./nyuenc file.txt > encoded.enc`
  or
  `./nyuenc file1.txt file2.txt > encoded.enc`
- To encode one or more files **with** threads, simply run:
  `./nyuenc -j 3 file.txt > encoded.enc`
  or
  `./nyuenc -j 3 file.txt file2.txt> encoded.enc`

  where the argument following `j` is the number of threads to be created in the thread pool.

## Important Specifications
- The program is limited to encoding characters that appear no more than 255 times in a row. In other words, the count for an encoded characters can be at most one byte in size.
- The program assumes the task queue is unbounded, thus allowing for all tasks to be submitted at once without being blocked.
