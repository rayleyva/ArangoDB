////////////////////////////////////////////////////////////////////////////////
/// @brief input-output scheduler using libev
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Scheduler/SchedulerLibev.h"

#include <ev.h>

#include <Basics/Exceptions.h>
#include <Basics/Logger.h>
#include <Basics/MutexLocker.h>
#include <Rest/Task.h>

#include "Scheduler/SchedulerThread.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// LIBEV
// -----------------------------------------------------------------------------

/* EV_TIMER is an alias for EV_TIMEOUT */
#ifndef EV_TIMER
#define EV_TIMER EV_TIMEOUT
#endif

namespace {

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // async events
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  struct AsyncWatcher {
    ev_async async;
    struct ev_loop* loop;
    EventToken token;
    Task* task;
  };



  void asyncCallback (struct ev_loop*, ev_async* w, int revents) {
    AsyncWatcher* watcher = reinterpret_cast<AsyncWatcher*>(w);
    Task* task = watcher->task;

    if (task != 0 && (revents & EV_ASYNC) && task->isActive()) {
      task->handleEvent(watcher->token, EVENT_ASYNC);
    }
  }




  void wakerCallback (struct ev_loop* loop, ev_async*, int) {
    ev_unloop(loop, EVUNLOOP_ALL);
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // socket events
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  struct SocketWatcher {
    ev_io io;
    struct ev_loop* loop;
    EventToken token;
    Task* task;
  };



  void socketCallback (struct ev_loop*, ev_io* w, int revents) {
    SocketWatcher* watcher = reinterpret_cast<SocketWatcher*>(w);
    Task* task = watcher->task;

    if (task != 0 && task->isActive()) {
      if (revents & EV_READ) {
        if (revents & EV_WRITE) {
          task->handleEvent(watcher->token, EVENT_SOCKET_READ | EVENT_SOCKET_WRITE);
        }
        else {
          task->handleEvent(watcher->token, EVENT_SOCKET_READ);
        }
      }
      else if (revents & EV_WRITE) {
        task->handleEvent(watcher->token, EVENT_SOCKET_WRITE);
      }
    }
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // periodic events
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  struct PeriodicWatcher {
    ev_periodic periodic;
    struct ev_loop* loop;
    EventToken token;
    Task* task;
  };



  void periodicCallback (struct ev_loop*, ev_periodic* w, int revents) {
    PeriodicWatcher* watcher = reinterpret_cast<PeriodicWatcher*>(w);
    Task* task = watcher->task;

    if (task != 0 && (revents & EV_PERIODIC) && task->isActive()) {
      task->handleEvent(watcher->token, EVENT_PERIODIC);
    }
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // signal events
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  struct SignalWatcher {
    ev_signal signal;
    struct ev_loop* loop;
    EventToken token;
    Task* task;
  };



  void signalCallback (struct ev_loop*, ev_signal* w, int revents) {
    SignalWatcher* watcher = reinterpret_cast<SignalWatcher*>(w);
    Task* task = watcher->task;

    if (task != 0 && (revents & EV_SIGNAL) && task->isActive()) {
      task->handleEvent(watcher->token, EVENT_SIGNAL);
    }
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // timer events
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  struct TimerWatcher {
    ev_timer timer;
    struct ev_loop* loop;
    EventToken token;
    Task* task;
  };



  void timerCallback (struct ev_loop*, ev_timer* w, int revents) {
    TimerWatcher* watcher = reinterpret_cast<TimerWatcher*>(w);
    Task* task = watcher->task;

    if (task != 0 && (revents & EV_TIMER) && task->isActive()) {
      task->handleEvent(watcher->token, EVENT_TIMER);
    }
  }
}

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // static methods
    // -----------------------------------------------------------------------------

    int SchedulerLibev::availableBackends () {
      return ev_supported_backends();
    }

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------


    SchedulerLibev::SchedulerLibev (size_t concurrency, int backend)
      : SchedulerImpl(concurrency),
        _backend(backend) {

      // report status
      LOGGER_TRACE << "supported backends: " << ev_supported_backends();
      LOGGER_TRACE << "recommended backends: " << ev_recommended_backends();
      LOGGER_TRACE << "embeddable backends: " << ev_embeddable_backends();
      LOGGER_TRACE << "backend flags: " << backend;


      // construct the loops
      _loops = new struct ev_loop*[nrThreads];

      ((struct ev_loop**) _loops)[0] = ev_default_loop(_backend);

      for (size_t i = 1;  i < nrThreads;  ++i) {
        ((struct ev_loop**) _loops)[i] = ev_loop_new(_backend);
      }

      // construct the scheduler threads
      threads = new SchedulerThread* [nrThreads];
      _wakers = new ev_async*[nrThreads];

      for (size_t i = 0;  i < nrThreads;  ++i) {
        threads[i] = new SchedulerThread(this, EventLoop(i), i == 0);

        ev_async* w = new ev_async;

        ev_async_init(w, wakerCallback);
        ev_async_start(((struct ev_loop**) _loops)[i], w);

        ((ev_async**) _wakers)[i] = w;
      }

      // watcher 0 is undefined
      _watchers.push_back(0);
    }



    SchedulerLibev::~SchedulerLibev () {

      // shutdown loops
      for (size_t i = 1;  i < nrThreads;  ++i) {
        ev_async_stop(((struct ev_loop**) _loops)[i], ((ev_async**) _wakers)[i]);
        ev_loop_destroy(((struct ev_loop**) _loops)[i]);
      }

      // delete loops buffer
      delete[] ((struct ev_loop**) _loops);

      // begin shutdown sequence within threads
      for (size_t i = 0;  i < nrThreads;  ++i) {
        threads[i]->beginShutdown();
      }

      // force threads to shutdown
      for (size_t i = 0;  i < nrThreads;  ++i) {
        threads[i]->stop();
      }

      usleep(10000);

      // and delete threads
      for (size_t i = 0;  i < nrThreads;  ++i) {
        delete threads[i];
        delete ((ev_async**) _wakers)[i];
      }

      // delete threads buffer and wakers
      delete[] threads;
      delete[] (ev_async**)_wakers;
     }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void SchedulerLibev::eventLoop (EventLoop loop) {
      struct ev_loop* l = (struct ev_loop*) lookupLoop(loop);

      ev_loop(l, 0);
    }



    void SchedulerLibev::wakeupLoop (EventLoop loop) {
      if (size_t(loop) >= nrThreads) {
        THROW_INTERNAL_ERROR("unknown loop");
      }

      ev_async_send(((struct ev_loop**) _loops)[loop], ((ev_async**) _wakers)[loop]);
    }



    void SchedulerLibev::uninstallEvent (EventToken token) {
      EventType type;
      void* watcher = lookupWatcher(token, type);

      if (watcher == 0) {
        return;
      }

      switch (type) {
        case EVENT_ASYNC: {
          AsyncWatcher* w = reinterpret_cast<AsyncWatcher*>(watcher);
          ev_async_stop(w->loop, (ev_async*) w);

          unregisterWatcher(token);
          delete w;

          break;
        }

        case EVENT_PERIODIC: {
          PeriodicWatcher* w = reinterpret_cast<PeriodicWatcher*>(watcher);
          ev_periodic_stop(w->loop, (ev_periodic*) w);

          unregisterWatcher(token);
          delete w;

          break;
        }

        case EVENT_SIGNAL: {
          SignalWatcher* w = reinterpret_cast<SignalWatcher*>(watcher);
          ev_signal_stop(w->loop, (ev_signal*) w);

          unregisterWatcher(token);
          delete w;

          break;
        }

        case EVENT_SOCKET_READ: {
          SocketWatcher* w = reinterpret_cast<SocketWatcher*>(watcher);
          ev_io_stop(w->loop, (ev_io*) w);

          unregisterWatcher(token);
          delete w;

          break;
        }


        case EVENT_TIMER: {
          TimerWatcher* w = reinterpret_cast<TimerWatcher*>(watcher);
          ev_timer_stop(w->loop, (ev_timer*) w);

          unregisterWatcher(token);
          delete w;

          break;
        }
      }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // async events
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    EventToken SchedulerLibev::installAsyncEvent (EventLoop loop, Task* task) {
      AsyncWatcher* watcher = new AsyncWatcher;
      watcher->loop = (struct ev_loop*) lookupLoop(loop);
      watcher->task = task;
      watcher->token = registerWatcher(watcher, EVENT_ASYNC);

      ev_async* w = (ev_async*) watcher;
      ev_async_init(w, asyncCallback);
      ev_async_start(watcher->loop, w);

      return watcher->token;
    }



    void SchedulerLibev::sendAsync (EventToken token) {
      AsyncWatcher* watcher = reinterpret_cast<AsyncWatcher*>(lookupWatcher(token));

      if (watcher == 0) {
        return;
      }

      ev_async* w = (ev_async*) watcher;
      ev_async_send(watcher->loop, w);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // periodic events
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    EventToken SchedulerLibev::installPeriodicEvent (EventLoop loop, Task* task, double offset, double intervall) {
      PeriodicWatcher* watcher = new PeriodicWatcher;
      watcher->loop = (struct ev_loop*) lookupLoop(loop);
      watcher->task = task;
      watcher->token = registerWatcher(watcher, EVENT_PERIODIC);

      ev_periodic* w = (ev_periodic*) watcher;
      ev_periodic_init(w, periodicCallback, offset, intervall, 0);
      ev_periodic_start(watcher->loop, w);

      return watcher->token;
    }



    void SchedulerLibev::rearmPeriodic (EventToken token, double offset, double intervall) {
      PeriodicWatcher* watcher = reinterpret_cast<PeriodicWatcher*>(lookupWatcher(token));

      if (watcher == 0) {
        return;
      }

      ev_periodic* w = (ev_periodic*) watcher;
      ev_periodic_set(w, offset, intervall, 0);
      ev_periodic_again(watcher->loop, w);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // signal events
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    EventToken SchedulerLibev::installSignalEvent (EventLoop loop, Task* task, int signal) {
      SignalWatcher* watcher = new SignalWatcher;
      watcher->loop = (struct ev_loop*) lookupLoop(loop);
      watcher->task = task;
      watcher->token = registerWatcher(watcher, EVENT_SIGNAL);

      ev_signal* w = (ev_signal*) watcher;
      ev_signal_init(w, signalCallback, signal);
      ev_signal_start(watcher->loop, w);

      return watcher->token;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // socket events
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    EventToken SchedulerLibev::installSocketEvent (EventLoop loop, EventType type, Task* task, socket_t fd) {
      SocketWatcher* watcher = new SocketWatcher;
      watcher->loop = (struct ev_loop*) lookupLoop(loop);
      watcher->task = task;
      watcher->token = registerWatcher(watcher, EVENT_SOCKET_READ);

      int flags = 0;

      if (type & EVENT_SOCKET_READ) {
        flags |= EV_READ;
      }

      if (type & EVENT_SOCKET_WRITE) {
        flags |= EV_WRITE;
      }

      ev_io* w = (ev_io*) watcher;
      ev_io_init(w, socketCallback, fd, flags);
      ev_io_start(watcher->loop, w);

      return watcher->token;
    }



    void SchedulerLibev::startSocketEvents (EventToken token) {
      SocketWatcher* watcher = reinterpret_cast<SocketWatcher*>(lookupWatcher(token));

      if (watcher == 0) {
        return;
      }

      ev_io* w = (ev_io*) watcher;

      if (! ev_is_active(w)) {
        ev_io_start(watcher->loop, w);
      }
    }



    void SchedulerLibev::stopSocketEvents (EventToken token) {
      SocketWatcher* watcher = reinterpret_cast<SocketWatcher*>(lookupWatcher(token));

      if (watcher == 0) {
        return;
      }

      ev_io* w = (ev_io*) watcher;

      if (ev_is_active(w)) {
        ev_io_stop(watcher->loop, w);
      }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // timer events
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    EventToken SchedulerLibev::installTimerEvent (EventLoop loop, Task* task, double timeout) {
      TimerWatcher* watcher = new TimerWatcher;
      watcher->loop = (struct ev_loop*) lookupLoop(loop);
      watcher->task = task;
      watcher->token = registerWatcher(watcher, EVENT_TIMER);

      ev_timer* w = (ev_timer*) watcher;
      ev_timer_init(w, timerCallback, timeout, 0.0);
      ev_timer_start(watcher->loop, w);

      return watcher->token;
    }



    void SchedulerLibev::clearTimer (EventToken token) {
      TimerWatcher* watcher = reinterpret_cast<TimerWatcher*>(lookupWatcher(token));

      if (watcher == 0) {
        return;
      }

      ev_timer* w = (ev_timer*) watcher;
      ev_timer_stop(watcher->loop, w);
    }



    void SchedulerLibev::rearmTimer (EventToken token, double timeout) {
      TimerWatcher* watcher = reinterpret_cast<TimerWatcher*>(lookupWatcher(token));

      if (watcher == 0) {
        return;
      }

      ev_timer* w = (ev_timer*) watcher;
      ev_timer_set(w, 0.0, timeout);
      ev_timer_again(watcher->loop, w);
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    void* SchedulerLibev::lookupLoop (EventLoop loop) {
      if (size_t(loop) >= nrThreads) {
        THROW_INTERNAL_ERROR("unknown loop");
      }

      return ((struct ev_loop**) _loops)[loop];
    }



    void* SchedulerLibev::lookupWatcher (EventToken token) {
      MUTEX_LOCKER(_watcherLock);

      if (token >= _watchers.size()) {
        return 0;
      }

      return _watchers[token];
    }



    void* SchedulerLibev::lookupWatcher (EventToken token, EventType& type) {
      MUTEX_LOCKER(_watcherLock);

      if (token >= _watchers.size()) {
        return 0;
      }

      type = _types[token];
      return _watchers[token];
    }



    EventToken SchedulerLibev::registerWatcher (void* watcher, EventType type) {
      MUTEX_LOCKER(_watcherLock);

      EventToken token;

      if (_frees.empty()) {
        token = _watchers.size();
        _watchers.push_back(watcher);
      }
      else {
        token = _frees.back();
        _frees.pop_back();
        _watchers[token] = watcher;
      }

      _types[token] = type;

      return token;
    }



    void SchedulerLibev::unregisterWatcher (EventToken token) {
      MUTEX_LOCKER(_watcherLock);

      _frees.push_back(token);
      _watchers[token] = 0;
    }
  }
}