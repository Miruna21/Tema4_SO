Name: Orzata Miruna-Narcisa,
Group: 331CA

# Project 4 SO (Thread Scheduler)

Organization
-
* Idea: Implement a **thread-scheduler in user-space** in the form of a shared library.
* The solution is based on the development of a C program on **Unix and Windows** systems, which simulates a **preemptive process scheduler**, in a uniprocessor system, and uses a planning algorithm **Round Robin with priorities**.


Implementation
-
* The dynamic library provides a common interface to all threads that use it and are to be planned. Thus, the functions exported by the library are the following:
    * **so_init (quantum, io)** - which has the role of initializing the internal structure of the scheduler.
    * **so_end ()** - will release scheduler resources and wait for all threads to finish before leaving the system.
    * **so_fork (handler, priority)** - will create a new thread in kernel-space and add all the necessary information about it in the scheduler's priority queue. As soon as the kernel-space thread has started, it notifies the parent thread that it is ready for scheduling (it is ready to run the received handler as a parameter), and the parent thread wakes up the next time and will start the planning process by choosing the most appropriate thread to run on the processor. During this time, the newly created threads, which were not scheduled, will wait until the scheduler chooses them at a later stage.
    * **so_exec ()** - will simulate the execution of an instruction on the processor, which will decrease the amount for the calling thread.
    * **so_wait (event / io)** - will mark the current thread as locked to an I / O event and will not allow it to be scheduled on the processor until it receives a signal from another thread on the same event.
    * **so_signal (event / io)** - has the role of waking up all the threads waiting on the event mentioned as a parameter.
* The most complex function, **schedule**, is based on a priority queue with threads, which will always retain the best thread at the top, except in cases where the threads at the top wait after an event I /A.
* The scheduler will preempt the current thread running on the processor in the following cases:
    * a higher priority thread enters the system.
    * a higher priority thread is signaled by a signal operation.
    * The current thread has expired.
    * The current thread is waiting for an event after the wait operation.
    * The current thread no longer has instructions to execute.
* A special case encountered during the scheduling algorithm is when there are multiple threads in the system with the same priority, and these must be scheduled alternately through Round Robin, when their time is up. In this sense, if the current task has run out of time and a task with a higher priority has not entered the system, then I will choose the next task with the same priority or with a lower priority, if I do not find the same priority. Thus, I remove the current task from the queue and add it again to have the next task with the same priority.
* Other details:
    * Blocking the current thread was based on using the mutex-mutex-variable-condition construct, through which the thread calls **pthread_mutex_lock** / **EnterCriticalSection**, waits until a predicate becomes true through the **pthread_cond_wait** function / **SleepConditionVariableCS**, and then release the lock when the condition is met by **pthread_mutex_unlock** / **LeaveCriticalSection**.
    * Similarly, signaling the processor run command to a thread is done by setting a variable with the chosen thread id and calling the **pthread_cond_broadcast** / **WakeAllConditionVariable** function, also within a critical region.
    * After performing each function provided by the library, the amount of the calling thread will be decremented and the best thread will be scheduled.

How is it compiled and run?
-
* Following the make / nmake command, the dynamic library libscheduler.so / libscheduler.dll is generated.
* The "make -f Makefile.checker" / "nmake / F NMakefile.checker" command will compile the sources used by the checker.
* Each test is run separately as follows: "./_test/run_test init", followed by "./_test/run_test <nr_test>"
* All tests are run through: "./run_all.sh".

Resources used
-
* https://ocw.cs.pub.ro/courses/so/laboratoare/laborator-08
* https://ocw.cs.pub.ro/courses/so/laboratoare/laborator-09
* https://docs.microsoft.com/en-us/windows/win32/sync/condition-variables?redirectedfrom=MSDN
* https://docs.microsoft.com/en-us/windows/win32/sync/using-condition-variables
