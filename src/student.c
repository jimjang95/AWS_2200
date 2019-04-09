
/*
 * student.c
 * Multithreaded OS Simulation for CS 2200
 *
 * This file contains the CPU scheduler for the simulation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include "os-sim.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/** Function prototypes **/
extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);


/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
static pcb_t **current;
static pthread_mutex_t current_mutex;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// pthread
static pthread_cond_t empty_queue = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t queue_lock;

// 0 = FIFO
// 1 = Round Robin
// 2 = Priority
static int schedule_scheme = 0;
static int prempt_time = -1;
static unsigned int cpus = 0;

// Queue functions
static void push_back(pcb_t* pcb);
static void fifo(pcb_t* pcb);
static void priority_push(pcb_t* pcb);
static pcb_t* pop();


static pcb_t* head;

static void push_back(pcb_t* pcb) {
    pthread_mutex_lock(&queue_lock);
    switch(schedule_scheme) {
        case 0:
        case 1:
            fifo(pcb);
            break;
        case 2:
            priority_push(pcb);
            break;
    }
    pthread_cond_signal(&empty_queue);
    pthread_mutex_unlock(&queue_lock);
}

static void fifo(pcb_t* pcb) {
    if (head == NULL) {
        head = pcb;
    } else {
        pcb_t* tmp = head;
        while(tmp->next != NULL) {
            tmp = tmp->next;
        }
        tmp->next = pcb;
    }
    pcb->next = NULL;
}

static void priority_push(pcb_t* pcb) {
    if (head == NULL) {
        head = pcb;
    } else if (head->priority > pcb->priority) {
        pcb->next = head;
        head = pcb;
    } else {
        pcb_t* tmp = head;
        while(tmp->next != NULL && tmp->next->priority < pcb->priority) {
            tmp = tmp->next;
        }
        if (tmp->next == NULL) {
            tmp->next = pcb;
            pcb->next = NULL;
        } else {
            pcb->next = tmp->next;
            tmp->next = pcb->next;
        }
    }
}

static pcb_t* pop() {
    if (head == NULL) {
        return NULL;
    }
    pcb_t* tmp = head;
    head = head->next;
    return tmp;
}



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Set the currently running process using the current array
 *
 *   4. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *
 *	The current array (see above) is how you access the currently running process indexed by the cpu id.
 *	See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information
 *	about it and its parameters.
 */
static void schedule(unsigned int cpu_id)
{
    pcb_t* tmp = pop();
    if (tmp != NULL) {
        tmp->state = PROCESS_RUNNING;
        current[cpu_id] = tmp;
    }
    context_switch(cpu_id, tmp, prempt_time);
}


/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{
    /* FIX ME */
    // schedule(0);

    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
    // pthread_mutex_lock(&current_mutex);
    while (head == NULL) {
        pthread_cond_wait(&empty_queue, &current_mutex);
    }
    schedule(cpu_id);
    pthread_mutex_unlock(&current_mutex);
    // schedule(cpu_id);
    // mt_safe_usleep(1000000);
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 *
 * Remember to set the status of the process to the proper value.
 */
extern void preempt(unsigned int cpu_id)
{
    /* FIX ME */
    current[cpu_id]->state = PROCESS_READY;
    push_back(current[cpu_id]);
    schedule(cpu_id);
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{
    current[cpu_id]->state = PROCESS_WAITING;
    schedule(cpu_id);
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{
    /* FIX ME */
    current[cpu_id]->state = PROCESS_TERMINATED;
    schedule(cpu_id);
}


/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is Priority, wake_up() may need
 *      to preempt the CPU with the lowest priority to allow it to
 *      execute the process which just woke up.  However, if any CPU is
 *      currently running idle, or all of the CPUs are running processes
 *      with higher priority than the one which just woke up, wake_up()
 *      should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for
 * 	its prototype and the parameters it takes in.
 *
 *  NOTE: A high priority corresponds to a low number.
 *  i.e. 0 is the highest possible priority.
 */
extern void wake_up(pcb_t *process)
{
    /* FIX ME */
    if (schedule_scheme == 2) {
        // Priority scheduling being used.

        if (head == NULL) {
            // The only case in which IDLE processors can exist
            process->state = PROCESS_READY;
            push_back(process); // this locks AND signals. we good.
        } else {
            // now we need to actually look for the worst processor to replace
            pthread_mutex_lock(&current_mutex);
            unsigned int min_cpu = 0;
            unsigned int worst_priority = 0;
            for (unsigned int i = 0; i < cpus; i++) {
                if (current[i]->priority > worst_priority) {
                    worst_priority = current[i]->priority;
                    min_cpu = i;
                }
            }

            if (current[min_cpu]->priority > process->priority) {
                // Found a CPU which we can actually force_prempt.
                force_preempt(min_cpu);

                // Before we context_switch, clean up previous process
                current[min_cpu]->state = PROCESS_READY;
                push_back(current[min_cpu]);

                // now prepare the new process to step in
                process->state = PROCESS_RUNNING;
                current[min_cpu] = process;
                context_switch(min_cpu, process, -1);
            } else {
                process->state = PROCESS_READY;
                push_back(process);
            }
            pthread_mutex_unlock(&current_mutex);
        }

    } else {
        process->state = PROCESS_READY;
        push_back(process);
    }
}


/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -r and -p command-line parameters.
 */
int main(int argc, char *argv[])
{
    unsigned int cpu_count;

    /*
     * Check here if the number of arguments provided is valid.
     * You will need to modify this when you add more arguments.
     */
    // if (argc != 2)
    // {
    //     fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
    //         "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
    //         "    Default : FIFO Scheduler\n"
    //         "         -r : Round-Robin Scheduler\n"
    //         "         -p : Priority Scheduler\n\n");
    //     return -1;
    // }

    /* Parse the command line arguments */
    cpu_count = strtoul(argv[1], NULL, 0);
    cpus = cpu_count;

    int opt;
    /* FIX ME - Add support for -r and -p parameters*/
     while((opt = getopt(argc, argv, ":r:sp")) != -1)
    {
        switch(opt)
        {
            case 'r':
                printf("-r case activated\n\n");
                schedule_scheme = 1;
                prempt_time = atoi(optarg);
                break;
            case 's':
            case 'p':
                printf("-p case activated\n\n");
                schedule_scheme = 2;
                break;
            case ':':
                printf("option needs a value\n");
                break;
            case '?':
                fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
                    "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
                    "    Default : FIFO Scheduler\n"
                    "         -r : Round-Robin Scheduler\n"
                    "         -p : Priority Scheduler\n\n");
                return -1;
                break;
        }
    }
    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t*) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);

    /* Start the simulator in the library */
    start_simulator(cpu_count);

    return 0;
}


#pragma GCC diagnostic pop
