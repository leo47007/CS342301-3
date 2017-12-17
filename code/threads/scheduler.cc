// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------
int SJF_compare(Thread* thread1, Thread* thread2)
{
    if(thread1->getBurstTime() == thread2->getBurstTime())
    {
        if(thread1->getID() > thread2->getID())
            return -1;
        else return 1;
    }
    else if(thread1->getBurstTime() < thread2->getBurstTime()) 
        return -1;
    else 
        return 1;
}

int Priority_compare(Thread* thread1, Thread* thread2)
{
    if(thread1->getPriority() == thread2->getPriority())
    {
        if(thread1->getID() > thread2->getID())
            return -1;
        else return 1;        
    }
    else if(thread1->getPriority() > thread2->getPriority()) 
        return -1;
    else 
        return 1;
}

Scheduler::Scheduler()
{ 
    readyList = new List<Thread *>; 

    L1_SJF = new SortedList<Thread *>(SJF_compare);
    L2_Priority = new SortedList<Thread *>(Priority_compare);
    L3_RR = new List<Thread *>;

    toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList; 
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);

    thread->setArrivalTime(kernel->stats->totalTicks);
    //readyList->Append(thread);  leo comment
    if(thread->getPriority() >= 100)
    {
        L1_SJF->Insert(thread);
        cout << "Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[1]" << endl;
        if(thread->getBurstTime() < kernel->currentThread->getBurstTime())
        {
            cout<<"Burst Time of Thread [" << thread->getID() << "]:"<<thread->getBurstTime()<<endl;
            cout<<"Burst Time of Thread [" << kernel->currentThread->getID() << "]:"<<kernel->currentThread->getBurstTime()<<endl;
            cout<<"preempt"<<endl;
            kernel->currentThread->Yield();
            kernel->currentThread->setTmpburstTime(kernel->currentThread->getTmpburstTime()+(kernel->stats->totalTicks - kernel->currentThread->getStartExeTime()));
        }
    }
    else if(thread->getPriority() >= 50)
    {
        L2_Priority->Insert(thread);
        cout << "Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[2]" << endl;
    }
    else
    {
        L3_RR->Append(thread);
        cout << "Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[3]" << endl;
    }
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

/*    if (readyList->IsEmpty()) {
		return NULL;
    } else {
    	return readyList->RemoveFront();
    }
*/ //leo comment
    Thread* threadToRun;
    if(!(L1_SJF->IsEmpty()))
    {
        threadToRun = L1_SJF->RemoveFront();
        cout<<"Tick ["<< kernel->stats->totalTicks << "]: Thread [" << threadToRun->getID() <<"] is removed from queue L[1]"<<endl;
    }
    else if(!(L2_Priority->IsEmpty()))
    {
        threadToRun = L2_Priority->RemoveFront();
        cout<<"Tick ["<< kernel->stats->totalTicks << "]: Thread [" << threadToRun->getID() <<"] is removed from queue L[2]"<<endl;        
    }
    else if(!(L3_RR->IsEmpty()))
    {
        threadToRun = L3_RR->RemoveFront();
        cout<<"Tick ["<< kernel->stats->totalTicks << "]: Thread [" << threadToRun->getID() <<"] is removed from queue L[3]"<<endl;       
    }
    else 
        return NULL;

    return threadToRun;
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previouFy running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow
    nextThread->setStartExeTime(kernel->stats->totalTicks);  //leo 
    nextThread->setLastburstTime(kernel->stats->totalTicks - oldThread->getStartExeTime());  //leo 

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
//leo(
    cout<< "Tick ["<<kernel->stats->totalTicks<<"]: Thread ["<<nextThread->getID()<<"] is now selected for execution"<<endl;  
    cout<< "Tick ["<<kernel->stats->totalTicks<<"]: Thread ["<<oldThread->getID()<<"] is replaced, and it has executed ["<<kernel->stats->totalTicks - oldThread->getStartExeTime()<<"] ticks"<<endl;   
//)leo
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
}

void
Scheduler::Aging(List<Thread*>* list)
{
    ListIterator<Thread* > *iterator = new ListIterator<Thread* > (list);
    for( ; !iterator->IsDone() ; iterator->Next())
    {
        Thread* threadForAging = iterator->Item();
        if(kernel->stats->totalTicks - threadForAging->getArrivalTime() >= 1500)
        {
            int oldPriority = threadForAging->getPriority();
            threadForAging->Aging();
            int newPriority = threadForAging->getPriority();
            threadForAging->setArrivalTime(kernel->stats->totalTicks);
            cout<<"Tick ["<<kernel->stats->totalTicks<<"]: Thread ["<<threadForAging->getID()<<"] changes its priority from ["<<oldPriority<<"] to ["<<newPriority<<"]"<<endl;
            if(newPriority>=100 && newPriority<=109)
            {
                list->Remove(threadForAging);
                cout<<"Tick ["<<kernel->stats->totalTicks<<"]: Thread ["<<threadForAging->getID()<<"] is removed from queue L[2]"<<endl;
                ReadyToRun(threadForAging);
            }
            else if(newPriority>=50 && newPriority<=59)
            {
                list->Remove(threadForAging);
                cout<<"Tick ["<<kernel->stats->totalTicks<<"]: Thread ["<<threadForAging->getID()<<"] is removed from queue L[3]"<<endl;
                ReadyToRun(threadForAging);
            }
        }
    }
}

void
Scheduler::UpdateBurstTime(Thread* thread)
{
    //still no idea
    thread->setBurstTime(0.5*(thread->getBurstTime()+thread->getTmpburstTime()));
    cout <<"Tick ["<<kernel->stats->totalTicks<<"]: Thread ["<<thread->getID()<<"] UpdateBurstTime to ["<<thread->getBurstTime()<<"]"<<endl; 
    //burstTime =(kernel->stats->totalTicks - startTime)- readyTime;
}
