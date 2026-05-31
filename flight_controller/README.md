# 03 - Semaphores

## Overview
- This project demonstrates  **Semaphores** using Producer Consumer problem on the STM32F411 MCU.

## Problem Statement  
- Producer constantly fills a buffer with finite space and consumer consumes it. We want efficient solution instead of busy waiting

## Working  
- Initialize 2 semaphores one keeps track for consumer and one of producer. Initialize consumer one at 0 and producer one at 10.
- Every time producer grabs a **semaphore_full** it is decremented and it gives back **semaphore_empty** to consumer
- Every time consumer consumes a **semaphore_empty** it gives back **semaphore_full** to producer
- If semaphore reaches zero it means buffer is full for **semaphore_full** and is empty for **semaphore_empty**

## UART console output  
- Producer produces 10 times at once but then it runs out of semaphores and has to wait for it's slot
