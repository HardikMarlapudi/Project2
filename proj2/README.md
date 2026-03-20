# CSCE 311 – Project 2
## Concurrent SHA256 Hash Server

Author: Hardik Marlapudi  
Course: CSCE 311 – Operating Systems  
University: University of South Carolina  
Semester: Spring 2026

---

## Project Overview

This project implements a concurrent SHA256 hashing server that processes file hashing requests from multiple clients.  
Clients send file paths to the server, and the server reads the files, computes SHA256 hashes, and returns the results.

The system uses Unix domain sockets and multithreading to support many client requests simultaneously.  
The goal of the project is to demonstrate concepts from operating systems such as concurrency, synchronization, and client/server communication.

---

## System Architecture

The server uses a producer-consumer design with two types of worker threads:

File Reader Threads
- Responsible for reading file contents from disk.
- Acts as the producer of hashing tasks.

SHA Solver Threads
- Responsible for computing SHA256 hashes.
- Acts as the consumer of hashing tasks.

Workflow:

Client → Server Socket → File Reader Threads → SHA Solver Threads → Client Response

This architecture allows efficient parallel processing of requests and improves performance under heavy load.

---

## Key Features

- Multithreaded concurrent server
- Unix domain socket communication
- Parallel file processing
- Thread-safe resource management
- Deadlock-safe design
- Handles multiple client requests simultaneously
- Supports high throughput under heavy workloads

---

## Project Structure

proj2/

bin/
- proj2-server
- proj2-client

dat/
- high1.dat
- high2.dat
- low1.dat

lib/
- domain_socket
- file_reader
- sha_solver

src/
- server source files

Makefile  
README.md

---

## Build Instructions

Compile the project using:

make

This generates the following executables:

bin/proj2-server  
bin/proj2-client

---

## Running the Server

Start the server using the following command:

./bin/proj2-server /tmp/proj2.sock 4 32

Parameters:

/tmp/proj2.sock → Unix socket path used for communication  
4 → Number of file reader threads  
32 → Number of SHA solver threads

---

## Running the Client

Example command:

./bin/proj2-client /tmp/reply.sock /tmp/proj2.sock dat/high1.dat dat/high2.dat dat/low1.dat

Example output:

File            Hash
dat/high1.dat   D44C54F0DBF83B5E4549572F81A81001
dat/high2.dat   4C1DE86FBCB142402005F1B9504D5EAC
dat/low1.dat    C015115D302A58CB861AB23004917533

---

## Concurrency Performance Test

The server was tested under concurrent load to verify its ability to handle multiple client requests.

Benchmark command used:

time bash -c '
for batch in {1..20}; do
  for i in {1..10}; do
    ./bin/proj2-client /tmp/r_${batch}_${i}.sock /tmp/proj2.sock dat/high1.dat dat/high2.dat dat/low1.dat > /dev/null &
  done
  wait
done
'

Example result:

real    0m3.10s

Throughput calculation:

200 requests / 3.10 seconds ≈ 64 requests per second

This satisfies the 50+ requests per second requirement for the assignment.

---

## Deadlock Prevention

The server prevents deadlocks by managing thread access to shared resources carefully.  
Reader and solver threads coordinate through controlled checkout and check-in mechanisms to ensure that resources are used safely and efficiently.

---

## Technologies Used

C++  
POSIX Threads (pthreads)  
Unix Domain Sockets  
Concurrent Programming  
Producer–Consumer Pattern

---

## Learning Outcomes

This project demonstrates the following operating system concepts:

- Multithreaded server design
- Client/server communication
- Synchronization and thread coordination
- Resource management in concurrent systems
- Deadlock avoidance strategies

---

## Conclusion

The concurrent SHA256 hashing server successfully processes multiple client requests simultaneously while maintaining stable performance.  
The design effectively uses multithreading and socket communication to demonstrate important operating system principles including concurrency and synchronization.
