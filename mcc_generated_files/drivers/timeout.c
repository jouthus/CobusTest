/*
    (c) 2016 Microchip Technology Inc. and its subsidiaries. You may use this
    software and any derivatives exclusively with Microchip products.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
    PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
    WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
    BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
    FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
    ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
    THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

    MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
    TERMS.
 */

#ifdef __XC
#include <xc.h>
#endif
#include <stdio.h>
#include "timeout.h"
#include "../tmr1.h"

uint32_t dummyHandler(void *payload) {
    return 0;
};
void startTimerAtHead(void);
INLINE void enqueueCallback(timerStruct_t* timer);
INLINE void setTimerDuration(uint32_t duration);
INLINE uint32_t makeAbsolute(uint32_t timeout);
INLINE uint32_t rebaseList(void);
INLINE void printList(void);

timerStruct_t *listHead = NULL;
timerStruct_t * volatile executeQueueHead = NULL;

timerStruct_t dummy = {dummyHandler};
volatile uint32_t absoluteTimeofLastTimeout = 0;
volatile uint32_t lastTimerLoad = 0;
volatile bool isRunning = false;

bool timeout_hasPendingTimeouts(void) {
    return (listHead == NULL);
}

bool timeout_hasPendingCallbacks(void) {
    return (executeQueueHead == NULL);
}

// Disable all the timers without deleting them from any list. Timers can be 
//    restarted by calling startTimerAtHead

void stopTimeouts(void) {
    PIE1bits.TMR1IE = 0;
    absoluteTimeofLastTimeout = 0;
    lastTimerLoad = 0;
    isRunning = false;
}

inline void setTimerDuration(uint32_t duration) {
    lastTimerLoad = (65535) - duration;

    TMR1_WriteTimer(0);
    
    PIR1bits.TMR1IF = 0;
    TMR1_WriteTimer(lastTimerLoad);
}

// Convert the time provided from a "relative to now" time to a absolute time which
//    means ticks since the last timeout occurred or the timer module was started   

inline uint32_t makeAbsolute(uint32_t timeout) {
    timeout += absoluteTimeofLastTimeout;
    if (isRunning) {
        uint32_t timerVal = TMR1_ReadTimer();
        if (timerVal < lastTimerLoad) // Timer has wrapped while we were busy
        {
            timerVal += (65535);
        }

        timeout += timerVal - lastTimerLoad;
    }

    return timeout;
}

uint32_t timeout_getTimeRemaining(timerStruct_t *timer) {
    return timer->absoluteTime - makeAbsolute(0);
}

// Adjust the time base so that we can never wrap, saving us a lot of complications

inline uint32_t rebaseList(void) {
    timerStruct_t *basePoint = listHead;
    uint32_t baseTime = makeAbsolute(0);
    while (basePoint != NULL) {
        basePoint->absoluteTime -= baseTime;
        basePoint = basePoint->next;
    }
    absoluteTimeofLastTimeout -= baseTime;
    return baseTime;
}

inline void printList(void) {
    timerStruct_t *basePoint = listHead;
    while (basePoint != NULL) {
        printf("%ld -> ", (uint32_t) basePoint->absoluteTime);
        basePoint = basePoint->next;
    }
    printf("NULL\n");
}

// Returns true if the insert was at the head, false if not

bool sortedInsert(timerStruct_t *timer) {
    uint32_t timerAbsoluteTime = timer->absoluteTime;
    uint8_t atHead = 1;
    timerStruct_t *insertPoint = listHead;
    timerStruct_t *prevPoint = NULL;
    timer->next = NULL;

    if (timerAbsoluteTime < absoluteTimeofLastTimeout) {
        timerAbsoluteTime += (65535) - rebaseList() + 1;
        timer->absoluteTime = timerAbsoluteTime;
    }

    while (insertPoint != NULL) {
        if (insertPoint->absoluteTime > timerAbsoluteTime) {
            break; // found the spot
        }
        prevPoint = insertPoint;
        insertPoint = insertPoint->next;
        atHead = 0;
    }

    if (atHead == 1) // the front of the list. Checking the uint8_t saves 7 instructions
    {
        if (listHead == &dummy) {
            timer->next = dummy.next;
            LATB5 = 0;
        } else {
            timer->next = listHead;
        }
        listHead = timer;
    } else {
        timer->next = prevPoint->next;
        prevPoint->next = timer;
    }
    return atHead;
}


static volatile uint32_t tmp = 0;
void startTimerAtHead(void) {
    PIE1bits.TMR1IE = 0;

    if (listHead == NULL) // no timeouts left
    {
        stopTimeouts();
        return;
    }

    uint32_t period = listHead->absoluteTime - absoluteTimeofLastTimeout;
    tmp = period;
    
    // Timer is too far, insert dummy and schedule timer after the dummy
    if (period > (65535)) {
        NOP();
        NOP();
        //__builtin_software_breakpoint();
        dummy.absoluteTime = absoluteTimeofLastTimeout + (65535);
        dummy.next = listHead;
        listHead = &dummy;
        period = (65535);
        LATB5 = 1;
    }

    if (period == 0) {
      NOP();
   //   NOP();
      __builtin_software_breakpoint();
    }
    
    setTimerDuration(period);


    PIE1bits.TMR1IE = 1;
    isRunning = true;
}

// Cancel and remove all active timers

void timeout_flushAll(void) {
    stopTimeouts();

    while (listHead != NULL)
        timeout_delete(listHead);

    while (executeQueueHead != NULL)
        timeout_delete(executeQueueHead);

}


// Deletes a timer from a list and returns true if the timer was found and 
//     removed from the list specified

bool timeout_deleteHelper(timerStruct_t * volatile *list, timerStruct_t *timer) {
    bool retVal = false;
    if (*list == NULL)
        return retVal;

    // Guard in case we get interrupted, we cannot safely compare/search and get interrupted
    PIE1bits.TMR1IE = 0;

    // Special case, the head is the one we are deleting
    if (timer == *list) {
        *list = (*list)->next; // Delete the head
        retVal = true;
        startTimerAtHead(); // Start the new timer at the head
    } else { // More than one timer in the list, search the list.  
        timerStruct_t *findTimer = *list;
        timerStruct_t *prevTimer = NULL;
        while (findTimer != NULL) {
            if (findTimer == timer) {
                prevTimer->next = findTimer->next;
                retVal = true;
                break;
            }
            prevTimer = findTimer;
            findTimer = findTimer->next;
        }
        PIE1bits.TMR1IE = 1;
    }

    return retVal;
}

// This will cancel/remove a running timer. If the timer is already expired it will
//     also remove it from the callback queue

void timeout_delete(timerStruct_t *timer) {
    if (!timeout_deleteHelper(&listHead, timer)) {
        timeout_deleteHelper(&executeQueueHead, timer);
    }

    timer->next = NULL;
}

// Moves the timer from the active list to the list of timers which are expired and 
//    needs their callbacks called in callNextCallback

inline void enqueueCallback(timerStruct_t* timer) {
    timerStruct_t *tmp;
    timer->next = NULL;

    // Special case for empty list
    if (executeQueueHead == NULL) {
        executeQueueHead = timer;
        return;
    }

    // Find the end of the list and insert the next expired timer at the back of the queue
    tmp = executeQueueHead;
    while (tmp->next != NULL)
        tmp = tmp->next;

    tmp->next = timer;
}

// This function checks the list of expired timers and calls the first one in the 
//    list if the list is not empty. It also reschedules the timer if the callback
//    returned a value greater than 0
// It is recommended this is called from the main superloop (while(1)) in your code
//    but by design this can also be called from the timer ISR. If you wish callbacks
//    to happen from the ISR context you can call this as the last action in timeout_isr 
//    instead. 

extern timerStruct_t myTimer2;

inline void timeout_callNextCallback(void) {
    if (executeQueueHead == NULL)
        return;

    bool tempIE = PIE1bits.TMR1IE;
    PIE1bits.TMR1IE = 0;

    timerStruct_t *callBackTimer = executeQueueHead;

    // Done, remove from list
    executeQueueHead = executeQueueHead->next;
    // Mark the timer as not in use
    callBackTimer->next = NULL;

    PIE1bits.TMR1IE = tempIE;

    uint32_t reschedule = callBackTimer->callbackPtr(callBackTimer->payload);

    // Do we have to reschedule it? If yes then add delta to absolute for reschedule
    if (reschedule) {
        //        if(reschedule > 600000)
        //            NOP();
        timeout_create(callBackTimer, reschedule);
    }
}

void timeout_initialize(void) {
    TMR1_SetInterruptHandler(timeout_isr);
}


// This function starts the timer provided with an expiry equal to "timeout".
// If the timer was already active/running it will be replaced by this and the 
//    old (active) timer will be removed/canceled first

void timeout_create(timerStruct_t *timer, uint32_t timeout) {
    // If this timer is already active, replace it
    timeout_delete(timer);

    PIE1bits.TMR1IE = 0;

    if (timeout > 600000) {
        LATB4 = 1;
    } else {
        LATB4 = 0;
    }
    timer->period = timeout;
    timer->absoluteTime = makeAbsolute(timeout);

    // We only have to start the timer at head if the insert was at the head
    if (sortedInsert(timer)) {
        startTimerAtHead();
    } else {
        if (isRunning)
            PIE1bits.TMR1IE = 1;
    }
}

// NOTE: assumes the callback completes before the next timer tick
timerStruct_t *thisListHead = NULL;

void timeout_isr(void) {
    thisListHead = listHead;

    LATC4 = 1;

    if (listHead == &dummy) {
        //__builtin_software_breakpoint();
        LATB5 = 0;
    }

    timerStruct_t *next = listHead->next;
    absoluteTimeofLastTimeout = listHead->absoluteTime;
    enqueueCallback(listHead);

    listHead = next;

    startTimerAtHead();

    LATC4 = 0;
}


// These methods are for calculating the elapsed time in stopwatch mode. 
// startTimer will start a timer with (maximum range)/2. You cannot time more than 
//    this and the timer will stop after this time elapses

void timeout_startTimer(timerStruct_t *timer) {
    uint32_t i = -1;
    timeout_create(timer, i >> 1);
}

// This function stops the "stopwatch" and returns the elapsed time.

uint32_t timeout_stopTimer(timerStruct_t *timer) {
    uint32_t now = makeAbsolute(0); // Do this as fast as possible for accuracy
    uint32_t i = -1;
    i >>= 1;

    timeout_delete(timer);

    uint32_t diff = timer->absoluteTime - now;

    // This calculates the (max range)/2 minus (remaining time) which = elapsed time
    return (i - diff);
}
