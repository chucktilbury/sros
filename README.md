# SROS
Simple Robotic Operating System

This is a cooperative tasking library that allows behavior to be structured into a hierarchy of threads that can be controlled from a single thread.

## Features
* Individual heap management for each thread.
* Message passing between threads.
* Written in portable C (except the parts that manage the stack)

## Missing features
* Interrupt handlers
* Timers
* Lots of other stuff
