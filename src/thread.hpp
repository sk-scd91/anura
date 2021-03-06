/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef THREAD_HPP_INCLUDED
#define THREAD_HPP_INCLUDED

#include "graphics.hpp"

#include <list>

#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/smart_ptr.hpp>

// Threading primitives wrapper for SDL_Thread.
//
// This module defines primitives for wrapping C++ around SDL's threading
// interface
namespace threading
{

struct manager
{
	~manager();
};

// Threading object.
//
// This class defines threading objects. One such object represents a
// thread and admits killing and joining on threads. Intended to be
// used for manipulating threads instead of poking around with SDL_Thread
// calls.
class thread
{
public:
	// Construct a new thread to start executing the function
	// pointed to by f. The void* data will be passed to f, to
	// facilitate passing of parameters to f.
	//
	// \param f the function at which the thread should start executing
	// \param data passed to f
	//
	// \pre f != NULL
#if SDL_VERSION_ATLEAST(2, 0, 0)
	explicit thread(const std::string& name, boost::function<void ()> f);
#else
	explicit thread(boost::function<void ()> f);
#endif

	// Destroy the thread object. This is done by waiting on the
	// thread with the join() operation, thus blocking until the
	// thread object has finished its operation.
	~thread();

	// Join (wait) on the thread to finish. When the thread finishes,
	// the function will return. calling wait() on an already killed
	// thread is a no-op.
	void join();

	void detach();

	Uint32 get_id() { return SDL_GetThreadID(thread_); }
private:
	thread(const thread&);
	void operator=(const thread&);

	boost::function<void ()> fn_;
	SDL_Thread* thread_;
};

inline Uint32 get_current_thread_id() { return SDL_ThreadID(); }
// Binary mutexes.
//
// Implements an interface to mutexes. This class only defines the
// mutex itself. Locking is handled through the friend class lock,
// and monitor interfacing through condition variables is handled through
// the friend class condition.
class mutex
{
public:
	mutex();
	~mutex();

	//we define copy constructors and assignment operators that keep the mutex
	//intact and don't do any copying. This allows classes that contain
	//a mutex member to have sane compiler-generated copying semantics.
	mutex(const mutex&);
	const mutex& operator=(const mutex&);

	friend class lock;
	friend class condition;

private:

	SDL_mutex* const m_;
};

// Binary mutex locking.
//
// Implements a locking object for mutexes. The creation of a lock
// object on a mutex will lock the mutex as long as this object is
// not deleted.
class lock
{
public:
	// Create a lock object on the mutex given as a parameter to
	// the constructor. The lock will be held for the duration
	// of the object existence.
	// If the mutex is already locked, the constructor will
	// block until the mutex lock can be acquired.
	//
	// \param m the mutex on which we should try to lock.
	explicit lock(const mutex& m);
	// Delete the lock object, thus releasing the lock aquired
	// on the mutex which the lock object was created with.
	~lock();
private:
	lock(const lock&);
	void operator=(const lock&);

	const mutex& m_;
};

// Condition variable locking.
//
// Implements condition variables for mutexes. A condition variable
// allows you to free up a lock inside a critical section
// of the code and regain it later. Condition classes only make
// sense to do operations on, if one already aquired a mutex.
class condition
{
public:
	condition();
	~condition();

	// Wait on the condition. When the condition is met, you
	// have a lock on the mutex and can do work in the critical
	// section. When the condition is not met, wait blocks until
	// the condition is met and atomically frees up the lock on
	// the mutex. One will automatically regain the lock when the
	// thread unblocks.
	//
	// If wait returns false we have an error. In this case one cannot
	// assume that he has a lock on the mutex anymore.
	//
	// \param m the mutex you wish to free the lock for
	// \returns true: the wait was successful, false: an error occurred
	//
	// \pre You have already aquired a lock on mutex m
	//
	bool wait(const mutex& m);

	enum WAIT_TIMEOUT_RESULT { THREAD_WAIT_OK, THREAD_WAIT_TIMEOUT, THREAD_WAIT_ERROR };

	// wait on the condition with a timeout. Basically the same as the
	// wait() function, but if the lock is not aquired before the
	// timeout, the function returns with an error.
	//
	// \param m the mutex you wish free the lock for.
	// \param timeout the allowed timeout in milliseconds (ms)
	// \returns result based on whether condition was met, it timed out,
	// or there was an error
	WAIT_TIMEOUT_RESULT wait_timeout(const mutex& m, unsigned int timeout);
	// signal the condition and wake up one thread waiting on the
	// condition. If no thread is waiting, notify_one() is a no-op.
	// Does not unlock the mutex.
	bool notify_one();

	// signal all threads waiting on the condition and let them contend
	// for the lock. This is often used when varying resource amounts are
	// involved and you do not know how many processes might continue.
	// The function should be used with care, especially if many threads are
	// waiting on the condition variable.
	bool notify_all();

private:
	condition(const condition&);
	void operator=(const condition&);

	SDL_cond* const cond_;
};

//class which defines an interface for waiting on an asynchronous operation
class waiter {
public:
	enum ACTION { WAIT, ABORT };

	virtual ~waiter() {}
	virtual ACTION process() = 0;
};

}

#endif
