# COMP 3430 - Assignment 1
# ANUSHA MAJUMDER
# 7924313

## Compiling

Run `make` in the assignment directory to build all three programs.

```
make
```

To clean up all compiled files:

```
make clean
```

## Question 1 - ELF Reader (a1q1)

Reads a 64-bit ELF binary file and prints the ELF header, all program headers, and all section headers with their raw data.

### Running

```
./a1q1 <elf-file>
```

### Examples

```
./a1q1 hello.out64
./a1q1 a1q1
./a1q1 /usr/bin/cat
```

The program will print an error and exit if the file is not a valid 64-bit ELF file.

## Question 2 - Process Herder (a1procs)

Manages a set of worker processes. Reads the number of workers from a file called `config`. On SIGHUP, re-reads the config and adjusts the number of running workers. On SIGINT, cleanly shuts down all workers and exits.

### Setup

Create a config file with the number of workers you want:

```
echo "3" > config
```

### Running

I used two terminal to run. You need it connected to the same aviary machine.

**Terminal 1** - start the program:
```
./a1procs
```

Note the supervisor PID printed on the first line.

**Terminal 2** - send signals to control it:
```
# increase workers to 5
echo "5" > config
kill -1 <supervisor pid>

# decrease workers to 2
echo "2" > config
kill -1 <supervisor pid>

# shut everything down
kill -2 <supervisor pid>
```

Signal numbers: `-1` is SIGHUP (reload config), `-2` is SIGINT (shutdown).

## Question 2 - Thread Herder (a1threads)

Same behaviour as the process herder but uses threads instead of processes. Workers share memory with the supervisor so no signals are needed to stop individual workers — the supervisor just sets a stop flag.

### Setup

```
echo "3" > config
```

### Running

You need two terminal windows connected to the same aviary machine.

**Terminal 1** - start the program:
```
./a1threads
```

Note the supervisor PID printed on the first line.

**Terminal 2** - send signals to control it:
```
# increase workers to 5
echo "5" > config
kill -1 <supervisor pid>

# decrease workers to 2
echo "2" > config
kill -1 <supervisor pid>

# shut everything down
kill -2 <supervisor pid>
```