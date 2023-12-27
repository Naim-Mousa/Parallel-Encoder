#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <pthread.h>
#include <getopt.h>

int TASK_ID = 1; // Global task identifier
unsigned char GLOBAL_CHAR = 0; // Global character for encoding across files
unsigned char GLOBAL_COUNT = 0; // Global count for encoding across files

// Generic error handling function
void handle_error(){
    perror("Error");
    exit(1);
}

struct Task;

// Task structure for encoding tasks
typedef struct Task{
    pthread_mutex_t mutex; // Mutex for synchronization
    pthread_cond_t encoded_cond; // Condition variable to signal encoding completion
    int id; // Unique task ID
    unsigned char firstChar; // First character of this task's range
    unsigned char firstCount; // Count of the first character
    unsigned char lastChar; // Last character of this task's range
    unsigned char lastCount; // Count of the last character
    int encodedElements; // Number of elements in the encoded array
    unsigned char *encodedArray; // Array to store encoded data
    char *inputFile; // Pointer to the input file data
    off_t range; // Byte range of the task
    int buffer; // Offset in the input file
    struct Task *nextTask; // Pointer to the next task
    struct Task *prevTask; // Pointer to the previous task
    bool encoded; // Flag indicating if the task is encoded
} Task;

// ThreadPool structure to manage threads and tasks
typedef struct{
    int free_threads; // Number of free threads
    pthread_t *threads; // Array of thread identifiers
    pthread_mutex_t mutex; // Mutex for synchronization
    pthread_cond_t new_cond; // Condition variable for new tasks
    Task **task_queue; // Queue of tasks for encoding
    int top_queue; // Index of the top task in the queue
    int task_selector; // Index to select the next task
    Task *first_task; // Pointer to the first task
    int queue_size; // Size of the task queue
} ThreadPool;

// Function prototypes for thread pool and task management
void add_file(ThreadPool *thread_pool, char *inputFile, size_t fileSize);
void add_task(ThreadPool *thread_pool, char *inputFile, size_t range, int buffer);
void *execute_task(void *arg);
void encode_t(Task *task);
void stitcher(Task *task);


// Function to create a thread pool
ThreadPool *create_threadpool(int num_threads, int inputSize){
    
    // Allocate memory for the thread pool and initialize its fields
    ThreadPool *thread_pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    thread_pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
    pthread_mutex_init(&thread_pool->mutex, NULL);
    pthread_cond_init(&thread_pool->new_cond, NULL);
    thread_pool->task_queue = (Task**)malloc(sizeof(Task**) * inputSize);
    thread_pool->task_selector = 0;
    thread_pool->top_queue = 0;
    thread_pool->first_task = NULL;
    thread_pool->queue_size = 0;

    // Create worker threads and assign them to execute tasks
    for (int i = 0; i < num_threads; i++){
        pthread_create(&thread_pool->threads[i], NULL, execute_task, (void*)thread_pool);
    }

    return thread_pool;
}

// Function to add a file to the thread pool for encoding
// The file is divided into segments and each segment is assigned to a task
void add_file(ThreadPool *thread_pool, char *inputFile, size_t bytesLeft){
    
    int range, buffer = 4096;
    int i = 0;

    // Determine the size of each segment and create a task for each segment
    while (bytesLeft > 0){
        
        if (bytesLeft > buffer) range = buffer;
        
        else range = bytesLeft;

        // Add each task to the thread pool's queue
        add_task(thread_pool, inputFile, range, buffer * i);
        bytesLeft -= range;
        i++;
    }
}

// Function to create and add a new task to the thread pool
void add_task(ThreadPool *thread_pool, char *inputFile, size_t range, int buffer){
    
    // Allocate and initialize a new task structure
    Task* task = (Task*)malloc(sizeof(Task));
    pthread_mutex_init(&task->mutex, NULL);
    pthread_cond_init(&task->encoded_cond, NULL);
    task->id = TASK_ID++;
    task->inputFile = inputFile;
    task->range = range;
    task->buffer = buffer;
    task->encodedElements = 0;
    task->encodedArray = (unsigned char*)malloc(sizeof(unsigned char) * 8192);
    task->nextTask = NULL;
    task->encoded = 0;

    // enter critical section
    pthread_mutex_lock(&thread_pool->mutex);

    // Add the task to the thread pool's queue
    // if queue is empty
    if (task->id == 1){
        thread_pool->task_queue[thread_pool->task_selector++] = task;
        thread_pool->first_task = task;
        task->prevTask = NULL;
    }

    else{
        thread_pool->task_queue[thread_pool->task_selector] = task;
        task->prevTask = thread_pool->task_queue[thread_pool->task_selector - 1];
        thread_pool->task_queue[thread_pool->task_selector - 1]->nextTask = task;
        thread_pool->task_selector++;
    }

    thread_pool->queue_size++;
    
    // Signal a condition variable to indicate that a new task is available
    // Wake up a worker thread
    pthread_cond_signal(&thread_pool->new_cond);
    
    // exit critical section
    pthread_mutex_unlock(&thread_pool->mutex);
}

// Worker thread function to execute tasks
void *execute_task(void *arg){

    ThreadPool *thread_pool = (ThreadPool*)arg;
    
    // Continuously fetch and execute tasks from the queue
    while (1){
            
        // enter critical section
        pthread_mutex_lock(&thread_pool->mutex);

        // wait for task to enter
        while (thread_pool->queue_size == 0){
            pthread_cond_wait(&thread_pool->new_cond, &thread_pool->mutex);
        }
        
        // get task from top of queue
        Task *task = thread_pool->task_queue[thread_pool->top_queue++];
        thread_pool->queue_size--;
        
        // exit critical section
        pthread_mutex_unlock(&thread_pool->mutex);

        // Encode each task using the encode_t function
        encode_t(task);
    }
}

// Function to encode a chunk of data (task) using threads
void encode_t(Task *task){
    
    unsigned char current, prev = task->inputFile[task->buffer];
    task->firstChar = prev;
    unsigned char counter = 1;

    // Run-length encode the data in the task's range
    for (off_t i = 1; i < task->range; i++){
        
        current = task->inputFile[i + task->buffer];

        if (current != prev){
            task->encodedArray[task->encodedElements++] = prev; // HERE
            task->encodedArray[task->encodedElements++] = counter;
            prev = current;
            counter = 1;
        }

        else counter++;
    }

    task->encodedArray[task->encodedElements++] = prev;
    task->encodedArray[task->encodedElements++] = counter;
    task->lastChar = current;
    task->lastCount = counter;
    task->firstCount = task->encodedArray[1];

    pthread_mutex_lock(&task->mutex);
    
    // After encoding, update the task's status and signal any waiting threads
    task->encoded = 1;
    pthread_cond_signal(&task->encoded_cond);
    
    // exit critical seciton
    pthread_mutex_unlock(&task->mutex);
}

// Function to stitch together encoded chunks
void stitcher(Task *task){
    
    // Sequentially combine the output of adjacent tasks
    while(task->nextTask != NULL){

        Task *nextTask = task->nextTask;

        pthread_mutex_lock(&nextTask->mutex);

        while(!nextTask->encoded){
            pthread_cond_wait(&nextTask->encoded_cond, &nextTask->mutex);
        }

        pthread_mutex_unlock(&nextTask->mutex);

        if (task->encodedElements == 2 && nextTask->encodedElements == 2){
            if (task->encodedArray[0] == nextTask->encodedArray[0]){
                nextTask->encodedArray[1] += task->encodedArray[1];
            }
        }

        else{
            // checking if characters are equal
            if (task->lastChar == nextTask->firstChar){
                
                // Adjust the count of repeated characters at the boundaries
                nextTask->encodedArray[1] = task->lastCount + nextTask->firstCount;

                // write everything of current file excluding last encoded char
                fwrite(&task->encodedArray[0], sizeof(unsigned char), task->encodedElements - 2, stdout);
                fflush(stdout);
            }

            else{
                // write everything of current file
                fwrite(&task->encodedArray[0], sizeof(unsigned char), task->encodedElements, stdout);
                fflush(stdout);
            }
        }

        task = task->nextTask;
    }

    fwrite(&task->encodedArray[0], sizeof(unsigned char), task->encodedElements, stdout); // HERE
    fflush(stdout);
}

// Function to encode a file or multiple files without using threads
void encode(char *inputFile, off_t fileSize, int fileCount){
    
    unsigned char current, prev = inputFile[0];

    unsigned char counter = 1;
    
    // Perform encoding for single-threaded operation
    if (fileCount != 1){

        if (prev == GLOBAL_CHAR){
            counter = GLOBAL_COUNT + 1;
        }

        else{
            fwrite(&GLOBAL_CHAR, 1, 1, stdout);
            fwrite(&GLOBAL_COUNT, 1, 1, stdout);
            prev = current;
            counter = 1;
        }
    }

    for (off_t i = 1; i < fileSize; i++){

        current = inputFile[i];

        if (current != prev){
            fwrite(&prev, 1, 1, stdout);
            fwrite(&counter, 1, 1, stdout);
            prev = current;
            counter = 1;
        }

        else counter++;
    }

    GLOBAL_CHAR = current;
    GLOBAL_COUNT = counter;
}

// Main function to set up and start the encoding process
int main(int argc, char *argv[]){
    int opt;

    int num_threads = 0;

    // Parse command line arguments and setup thread pool if necessary
    while ((opt = getopt(argc, argv, "j:")) != -1){
        switch (opt){
            case 'j':
                num_threads = atoi(optarg);
                break;

            default:
                break;
        }
    }

    if (num_threads == 0){
        
        if (optind >= argc){
            fprintf(stderr, "No input file specified.\n");
            exit(1);
        }

        for (size_t i = optind; i < argc; i++){

            // Open file
            int inputFD = open(argv[i], O_RDONLY);
            if (inputFD == -1){
                handle_error();
            }

            // Get file size
            struct stat sb;
            if (fstat(inputFD, &sb) == -1){
                handle_error();
            }

            // Map file into memory
            char *addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, inputFD, 0);

            close(inputFD);

            if (addr == MAP_FAILED){
                handle_error();
            }

            // Encoding
            encode(addr, sb.st_size, i);
        }

        fwrite(&GLOBAL_CHAR, 1, 1, stdout);
        fwrite(&GLOBAL_COUNT, 1, 1, stdout);
    }

    else{
        int inputSize = 0;
        
        for (size_t i = optind; i < argc; i++){

            // Open file
            int inputFD = open(argv[i], O_RDONLY);
            if (inputFD == -1){
                handle_error();
            }

            // Get file size
            struct stat sb;
            if (fstat(inputFD, &sb) == -1){
                handle_error();
            }

            if (sb.st_size % 4096 != 0){
                inputSize += (sb.st_size/4096) + 1;
            }

            else inputSize += sb.st_size/4096;

            close(inputFD);
        }

        // Initialize threadpool
        ThreadPool *thread_pool = create_threadpool(num_threads, inputSize);

        thread_pool->first_task = NULL;

        if (thread_pool == NULL){
            fprintf(stderr, "Failed to create thread pool.\n");
            exit(1);
        }

        if (optind >= argc){
            fprintf(stderr, "No input file specified.\n");
            exit(1);
        }

        // Iterating over input files
        for (size_t i = optind; i < argc; i++){

            // Open file
            int inputFD = open(argv[i], O_RDONLY);
            if (inputFD == -1){
                handle_error();
            }

            // Get file size
            struct stat sb;
            if (fstat(inputFD, &sb) == -1){
                handle_error();
            }

            if (sb.st_size == 0){
                continue;
            }

            // Map file into memory
            char *addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, inputFD, 0);

            close(inputFD);

            if (addr == MAP_FAILED){
                handle_error();
            }

            // Add file to thread pool for encoding
            add_file(thread_pool, addr, sb.st_size);
        }

        if (inputSize != 0){

            pthread_mutex_lock(&thread_pool->first_task->mutex);

            // Wait until next data chunk (task) in order is encoded
            while (!thread_pool->first_task->encoded){
                pthread_cond_wait(&thread_pool->first_task->encoded_cond, &thread_pool->first_task->mutex);
            }
            
            pthread_mutex_unlock(&thread_pool->first_task->mutex);

            // Stitch completed tasks together
            stitcher(thread_pool->first_task);
        }
    }
}