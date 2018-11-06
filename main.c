/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This is the main file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.65.2
        Device            :  PIC16F1619
        Driver Version    :  2.00
*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

#include "mcc_generated_files/mcc.h"




#define MilliSeconds (uint32_t) 500

uint32_t cb1_period = 0;
uint32_t cb2_period = 0;
uint32_t cb3_period = 0;

#define timeout_val1 1
#define timeout_val2 1200
#define timeout_val3 50

uint32_t eventCallback1(void* thing)
{
    D4_Toggle();
    cb1_period = 0;

    return (timeout_val1*MilliSeconds);

}
uint32_t eventCallback2(void* thing)
{
    D6_Toggle();
    cb2_period = 0;

    return (timeout_val2*MilliSeconds);

}
uint32_t eventCallback3(void* thing)
{
    D7_Toggle();
    cb3_period = 0;

    return (timeout_val3*MilliSeconds);

}

timerStruct_t myTimer1 = {eventCallback1};
timerStruct_t myTimer2 = {eventCallback2};
timerStruct_t myTimer3 = {eventCallback3};

extern volatile uint32_t absoluteTimeofLastTimeout;
void main(void)
{

    SYSTEM_Initialize();

    TRISB = 0;
    TRISC = 0;
    LATB = 0;
    LATC = 0;
    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();

    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();
  
    timeout_create(&myTimer1, MilliSeconds);
    timeout_create(&myTimer2, timeout_val2*MilliSeconds);
    timeout_create(&myTimer3, timeout_val3*MilliSeconds);

    while (1)
    {
        LATB6 = 1;
        while(!TMR2_HasOverflowOccured()); // TMR2 is set for 34usec...
        LATB6 = 0;
        
        cb1_period ++;
        cb2_period ++;
        cb3_period ++;
        
        timeout_callNextCallback();

        // failure detector
        if(cb1_period > (10l * timeout_val1 * 1000/34))
            LATC0 = 1;
        else
            LATC0 = 0;
        
        if(cb2_period > (10l*timeout_val2 * 1000/34))
            LATC1 = 1;
        else
            LATC1 = 0;
        
        if(cb3_period > (10l*timeout_val3 * 1000/34))
            LATC2 = 1;
        else
            LATC2 = 0;
        
        if ((myTimer1.absoluteTime > 0x1000000) ||(myTimer2.absoluteTime > 0x1000000) || (myTimer3.absoluteTime > 0x1000000))
        {
            NOP();
        }
    }
}
