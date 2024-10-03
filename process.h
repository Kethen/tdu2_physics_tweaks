#ifndef __PROCESS_H
#define __PROCESS_H
int suspend_threads(HANDLE *threads_snap);
int resume_threads(HANDLE threads_snap);
#endif
