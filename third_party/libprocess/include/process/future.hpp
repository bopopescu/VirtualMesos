#ifndef __PROCESS_FUTURE_HPP__
#define __PROCESS_FUTURE_HPP__

#include <queue>
#include <set>

#include <tr1/functional>

#include <process/latch.hpp>
#include <process/option.hpp>


namespace process {

// Forward declaration of Promise.
template <typename T>
class Promise;


// Definition of a "shared" future. A future can hold any
// copy-constructible value. A future is considered "shared" because
// by default a future can be accessed concurrently.
template <typename T>
class Future
{
public:
  Future();
  Future(const T& _t);
  Future(const Future<T>& that);
  ~Future();

  // Futures are assignable. This results in the reference to the
  // previous future data being decremented and a reference to 'that'
  // being incremented.
  Future<T>& operator = (const Future<T>& that);

  // Comparision operators useful for using futures in collections.
  bool operator == (const Future<T>& that) const;
  bool operator < (const Future<T>& that) const;

  // Helpers to get the current state of this future.
  bool pending() const;
  bool ready() const;
  bool discarded() const;
  bool failed() const;

  // Discards this future. This is similar to cancelling a future,
  // however it also occurs when the last reference to this future
  // gets cleaned up. Returns false if the future could not be
  // discarded (for example, because it is ready or failed).
  bool discard();

  // Waits for this future to become ready, discarded, or failed.
  bool await(double secs = 0) const;

  // Return the value associated with this future, waits indefinitely
  // until a value gets associated or until the future is discarded.
  T get() const;

  // Returns the failure message associated with this future if it was
  // discarded because it failed, or none if this future has not yet
  // been set or has been discarded without a failure message.
  Option<std::string> failure() const;

  // Type of the callback function that can get invoked when the
  // future gets set, fails, or is discarded.
  typedef std::tr1::function<void(const Future<T>&)> Callback;

  // Installs callbacks for the specified events and returns a const
  // reference to 'this' in order to easily support chaining.
  const Future<T>& onReady(const Callback& callback) const;
  const Future<T>& onFailed(const Callback& callback) const;
  const Future<T>& onDiscarded(const Callback& callback) const;

private:
  friend class Promise<T>;

  // Sets the value for this future, unless the future is already set,
  // failed, or discarded, in which case it returns false.
  bool set(const T& _t);

  // Sets this future as failed, unless the future is already set,
  // failed, or discarded, in which case it returns false.
  bool fail(const std::string& _message);

  void copy(const Future<T>& that);
  void cleanup();

  enum State {
    PENDING,
    READY,
    FAILED,
    DISCARDED,
  };

  int* refs;
  int* lock;
  State* state;
  T** t;
  std::string** message; // Message associated with failure.
  std::queue<Callback>* onReadyCallbacks;
  std::queue<Callback>* onFailedCallbacks;
  std::queue<Callback>* onDiscardedCallbacks;
  Latch* latch;
};


// TODO(benh): Consider making promise a non-copyable, non-assignabl
// subclass of Future. For now, they'll live as distinct types.
template <typename T>
class Promise
{
public:
  Promise();
  Promise(const T& t);
  Promise(const Promise<T>& that);
  ~Promise();

  bool set(const T& _t);
  bool fail(const std::string& message);

  // Returns a copy of the future associated with this promise.
  Future<T> future() const;

private:
  // Not assignable.
  void operator = (const Promise<T>&);

  Future<T> f;
};


template <>
class Promise<void>;


template <typename T>
class Promise<T&>;


template <typename T>
Promise<T>::Promise() {}


template <typename T>
Promise<T>::Promise(const T& t)
  : f(t) {}


template <typename T>
Promise<T>::Promise(const Promise<T>& that)
  : f(that.f) {}


template <typename T>
Promise<T>::~Promise() {}


template <typename T>
bool Promise<T>::set(const T& t)
{
  return f.set(t);
}


template <typename T>
bool Promise<T>::fail(const std::string& message)
{
  return f.fail(message);
}


template <typename T>
Future<T> Promise<T>::future() const
{
  return f;
}


// Internal helper utilities.
namespace internal {

inline void acquire(int* lock)
{
  while (!__sync_bool_compare_and_swap(lock, 0, 1)) {
    asm volatile ("pause");
  }
}

inline void release(int* lock)
{
  // Unlock via a compare-and-swap so we get a memory barrier too.
  bool unlocked = __sync_bool_compare_and_swap(lock, 1, 0);
  assert(unlocked);
}

namespace callbacks {

template <typename T>
void select(const Future<T>& future, Promise<Future<T> > promise)
{
  assert(future.ready());

  // We never fail the future associated with our promise.
  assert(!promise.future().failed());

  // Check if the promise is already ready or discarded, this avoids
  // acquiring a lock when invoking Future::set.
  if (!promise.future().ready() && !promise.future().discarded()) {
    promise.set(future);
  }
}

} // namespace callback {
} // namespace internal {


// TODO(benh): Move select and discard into 'futures' namespace.

// Returns an option of a ready future or none in the event of
// timeout. Note that select DOES NOT return for a future that has
// failed or been discarded.
template <typename T>
Option<Future<T> > select(const std::set<Future<T> >& futures, double secs)
{
  Promise<Future<T> > promise;

  std::tr1::function<void(const Future<T>&)> callback =
    std::tr1::bind(internal::callbacks::select<T>,
                   std::tr1::placeholders::_1, promise);

  typename std::set<Future<T> >::iterator iterator;
  for (iterator = futures.begin(); iterator != futures.end(); ++iterator) {
    const Future<T>& future = *iterator;
    future.onReady(callback);
  }

  Future<Future<T> > future = promise.future();

  if (future.await(secs)) {
    return Option<Future<T> >::some(future.get());
  } else {
    future.discard();
    return Option<Future<T> >::none();
  }
}


template <typename T>
void discard(const std::set<Future<T> >& futures)
{
  typename std::set<Future<T> >::const_iterator iterator;
  for (iterator = futures.begin(); iterator != futures.end(); ++iterator) {
    Future<T> future = *iterator;
    future.discard();
  }
}


template <typename T>
Future<T>::Future()
  : refs(new int(1)),
    lock(new int(0)),
    state(new State(PENDING)),
    t(new T*(NULL)),
    message(new std::string*(NULL)),
    onReadyCallbacks(new std::queue<Callback>()),
    onFailedCallbacks(new std::queue<Callback>()),
    onDiscardedCallbacks(new std::queue<Callback>()),
    latch(new Latch()) {}


template <typename T>
Future<T>::Future(const T& _t)
  : refs(new int(1)),
    lock(new int(0)),
    state(new State(PENDING)),
    t(new T*(NULL)),
    message(new std::string*(NULL)),
    onReadyCallbacks(new std::queue<Callback>()),
    onFailedCallbacks(new std::queue<Callback>()),
    onDiscardedCallbacks(new std::queue<Callback>()),
    latch(new Latch())
{
  set(_t);
}


template <typename T>
Future<T>::Future(const Future<T>& that)
{
  copy(that);
}


template <typename T>
Future<T>::~Future()
{
  cleanup();
}


template <typename T>
Future<T>& Future<T>::operator = (const Future<T>& that)
{
  if (this != &that) {
    cleanup();
    copy(that);
  }
  return *this;
}


template <typename T>
bool Future<T>::operator == (const Future<T>& that) const
{
  assert(latch != NULL);
  assert(that.latch != NULL);
  return latch == that.latch;
}


template <typename T>
bool Future<T>::operator < (const Future<T>& that) const
{
  assert(latch != NULL);
  assert(that.latch != NULL);
  return latch < that.latch;
}


template <typename T>
bool Future<T>::discard()
{
  bool result = false;

  assert(lock != NULL);
  internal::acquire(lock);
  {
    assert(state != NULL);
    if (*state == PENDING) {
      *state = DISCARDED;
      latch->trigger();
      result = true;
    }
  }
  internal::release(lock);

  // Invoke all callbacks associated with this future being
  // DISCARDED. We don't need a lock because the state is now in
  // DISCARDED so there should not be any concurrent modications.
  if (result) {
    while (!onDiscardedCallbacks->empty()) {
      const Callback& callback = onDiscardedCallbacks->front();
      // TODO(*): Invoke callbacks in another execution context.
      callback(*this);
      onDiscardedCallbacks->pop();
    }
  }

  return result;
}


template <typename T>
bool Future<T>::pending() const
{
  assert(state != NULL);
  return *state == PENDING;
}


template <typename T>
bool Future<T>::ready() const
{
  assert(state != NULL);
  return *state == READY;
}


template <typename T>
bool Future<T>::discarded() const
{
  assert(state != NULL);
  return *state == DISCARDED;
}


template <typename T>
bool Future<T>::failed() const
{
  assert(state != NULL);
  return *state == FAILED;
}


template <typename T>
bool Future<T>::await(double secs) const
{
  if (!ready() && !discarded() && !failed()) {
    assert(latch != NULL);
    return latch->await(secs);
  }
  return true;
}


template <typename T>
T Future<T>::get() const
{
  if (!ready()) {
    await();
  }

  if (!ready()) {
    abort();
  }

  assert(t != NULL);
  assert(*t != NULL);
  return **t;
}


template <typename T>
Option<std::string> Future<T>::failure() const
{
  assert(message != NULL);
  if (*message != NULL) {
    return **message;
  }

  return Option<std::string>::none();
}


template <typename T>
const Future<T>& Future<T>::onReady(const Callback& callback) const
{
  bool run = false;

  assert(lock != NULL);
  internal::acquire(lock);
  {
    assert(state != NULL);
    if (*state == READY) {
      run = true;
    } else if (*state == PENDING) {
      onReadyCallbacks->push(callback);
    }
  }
  internal::release(lock);

  // TODO(*): Invoke callback in another execution context.
  if (run) {
    callback(*this);
  }

  return *this;
}


template <typename T>
const Future<T>& Future<T>::onFailed(const Callback& callback) const
{
  bool run = false;

  assert(lock != NULL);
  internal::acquire(lock);
  {
    assert(state != NULL);
    if (*state == FAILED) {
      run = true;
    } else if (*state == PENDING) {
      onFailedCallbacks->push(callback);
    }
  }
  internal::release(lock);

  // TODO(*): Invoke callback in another execution context.
  if (run) {
    callback(*this);
  }

  return *this;
}


template <typename T>
const Future<T>& Future<T>::onDiscarded(const Callback& callback) const
{
  bool run = false;

  assert(lock != NULL);
  internal::acquire(lock);
  {
    assert(state != NULL);
    if (*state == DISCARDED) {
      run = true;
    } else if (*state == PENDING) {
      onDiscardedCallbacks->push(callback);
    }
  }
  internal::release(lock);

  // TODO(*): Invoke callback in another execution context.
  if (run) {
    callback(*this);
  }

  return *this;
}


template <typename T>
bool Future<T>::set(const T& _t)
{
  bool result = false;

  assert(lock != NULL);
  internal::acquire(lock);
  {
    assert(state != NULL);
    if (*state == PENDING) {
      *t = new T(_t);
      *state = READY;
      latch->trigger();
      result = true;
    }
  }
  internal::release(lock);

  // Invoke all callbacks associated with this future being READY. We
  // don't need a lock because the state is now in READY so there
  // should not be any concurrent modications.
  if (result) {
    while (!onReadyCallbacks->empty()) {
      const Callback& callback = onReadyCallbacks->front();
      // TODO(*): Invoke callbacks in another execution context.
      callback(*this);
      onReadyCallbacks->pop();
    }
  }

  return result;
}


template <typename T>
bool Future<T>::fail(const std::string& _message)
{
  bool result = false;

  assert(lock != NULL);
  internal::acquire(lock);
  {
    assert(state != NULL);
    if (*state == PENDING) {
      *message = new std::string(_message);
      *state = FAILED;
      latch->trigger();
      result = true;
    }
  }
  internal::release(lock);

  // Invoke all callbacks associated with this future being FAILED. We
  // don't need a lock because the state is now in FAILED so there
  // should not be any concurrent modications.
  if (result) {
    while (!onFailedCallbacks->empty()) {
      const Callback& callback = onFailedCallbacks->front();
      // TODO(*): Invoke callbacks in another execution context.
      callback(*this);
      onFailedCallbacks->pop();
    }
  }

  return result;
}


template <typename T>
void Future<T>::copy(const Future<T>& that)
{
  assert(that.refs > 0);
  __sync_fetch_and_add(that.refs, 1);
  refs = that.refs;
  lock = that.lock;
  state = that.state;
  t = that.t;
  message = that.message;
  onReadyCallbacks = that.onReadyCallbacks;
  onFailedCallbacks = that.onFailedCallbacks;
  onDiscardedCallbacks = that.onDiscardedCallbacks;
  latch = that.latch;
}


template <typename T>
void Future<T>::cleanup()
{
  assert(refs != NULL);
  if (__sync_sub_and_fetch(refs, 1) == 0) {
    // Discard the future if it is still pending (so we invoke any
    // discarded callbacks that have been setup). Note that we put the
    // reference count back at 1 here in case one of the callbacks
    // decides it wants to keep a reference.
    assert(state != NULL);
    if (*state == PENDING) {
      *refs = 1;
      discard();
    }

    // Now try and cleanup again (this time we know the future has
    // either been discarded or was not pending). Note that one of the
    // callbacks might have stored the future, in which case we'll
    // just return without doing anything, but the state will forever
    // be "discarded".
    assert(refs != NULL);
    if (__sync_sub_and_fetch(refs, 1) == 0) {
      delete refs;
      refs = NULL;
      assert(lock != NULL);
      delete lock;
      lock = NULL;
      assert(state != NULL);
      delete state;
      state = NULL;
      assert(t != NULL);
      delete *t;
      delete t;
      t = NULL;
      assert(message != NULL);
      delete *message;
      delete message;
      message = NULL;
      assert(onReadyCallbacks != NULL);
      delete onReadyCallbacks;
      onReadyCallbacks = NULL;
      assert(onFailedCallbacks != NULL);
      delete onFailedCallbacks;
      onFailedCallbacks = NULL;
      assert(onDiscardedCallbacks != NULL);
      delete onDiscardedCallbacks;
      onDiscardedCallbacks = NULL;
      assert(latch != NULL);
      delete latch;
      latch = NULL;
    }
  }
}

}  // namespace process {

#endif // __PROCESS_FUTURE_HPP__
