1. Compile with ./compile.sh
2. Link your project with qInterceptor.o
3. Declare the following prototype in the same file as your main function: void qInterceptorInit()
4. Call qInterceptorInit() at the beginning of your main function
5. Enjoy helpful leak information when your program exits!

Additional features:
- export the environment variable ALLOCATION_FAILURES to deliberately make malloc fail at specific points in time. 001 means only the third malloc will fail
- The qInterceptorAllocationCount and qInterceptorAllocatedSize globals contain the current allocation count and combined allocation size respectively.
