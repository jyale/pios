// Minimal version of the pthreads API for PIOS
#ifndef PIOS_INC_PTHREAD_H
#define PIOS_INC_PTHREAD_H
#ifndef PIOS_DSCHED	// Minimal "naturally deterministic" pthreads interface

#include <types.h>
#include <file.h>


// Pthread type
typedef int pthread_t;


// We don't support any pthread_attrs currently...
typedef void pthread_attr_t;


// Fork and join threads
int pthread_create(pthread_t *out_thread, const pthread_attr_t *attr,
		void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t th, void **out_exitval);


// The "barrier" type is an integer holding the index
// its index in the parent's global barriers array.
// The array entry at the corresponding index holds
// the barrier's count, i.e., the number of threads
// to stop at that barrier.
typedef int pthread_barrier_t;


// We do not currently support pthread_barrierattrs.
typedef void pthread_barrierattr_t;

// Maximum number of barriers a process/thread may use
#define BARRIER_MAX	PROC_CHILDREN  

int pthread_barrier_init(pthread_barrier_t * barrier, 
			 const pthread_barrierattr_t * attr, 
			 unsigned int count);

int pthread_barrier_wait(pthread_barrier_t * barrier);

int pthread_barrier_destroy(pthread_barrier_t * barrier);

pthread_t pthread_self(void);


#else	// PIOS_DSCHED	- "nondeterministic compatibility mode" pthreads API

// Keep the two pthreads implementations disjoint at the linker level.
#define pthread_create		dsthread_create
#define pthread_join		dsthread_join
#define pthread_mutex_init	dsthread_mutex_init
#define pthread_mutex_destroy	dsthread_mutex_destroy
#define pthread_mutex_lock	dsthread_mutex_lock
#define pthread_mutex_unlock	dsthread_mutex_unlock
#define pthread_cond_init	dsthread_cond_init
#define pthread_cond_destroy	dsthread_cond_destroy
#define pthread_cond_signal	dsthread_cond_signal
#define pthread_cond_broadcast	dsthread_cond_broadcast
#define pthread_cond_wait	dsthread_cond_wait

typedef struct pthread {
	int			child;	// child number in master process
	struct pthread *	next;	// next on wait queue
} *pthread_t;
typedef void pthread_attr_t;

typedef struct pthread_mutex {
	int		owner;		// which thread currently owns mutex
	int		locked;		// true if mutex currently locked
	pthread_t	waiting;	// list of waiting threads
} pthread_mutex_t;
typedef void pthread_mutexattr_t;

typedef struct pthread_cond {
	int		event;		// 0 = none, 1 = signal, 3 = broadcast
	pthread_t	waiting;	// list of waiting threads
} pthread_cond_t;
typedef void pthread_condattr_t;

int pthread_create(pthread_t *out_thread, const pthread_attr_t *attr,
		void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t th, void **out_exitval);

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

#endif	// PIOS_DSCHED
#endif /* !PIOS_INC_PTHREAD_H */
