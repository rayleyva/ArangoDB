////////////////////////////////////////////////////////////////////////////////
/// @brief job dispatcher
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Martin Schoenert
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Dispatcher.h"

#include "Basics/ConditionLocker.h"
#include "Logger/Logger.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"

#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/DispatcherThread.h"
#include "Dispatcher/Job.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

DispatcherThread* Dispatcher::defaultDispatcherThread (DispatcherQueue* queue) {
  return new DispatcherThread(queue);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

Dispatcher::Dispatcher ()
  : _accessDispatcher(),
    _stopping(0),
    _queues() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

Dispatcher::~Dispatcher () {
  for (map<string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
    delete i->second;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief is the dispatcher still running
////////////////////////////////////////////////////////////////////////////////

bool Dispatcher::isRunning () {
  MUTEX_LOCKER(_accessDispatcher);

  bool isRunning = false;

  for (map<string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
    isRunning = isRunning || i->second->isRunning();
  }

  return isRunning;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new queue
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::addQueue (string const& name, size_t nrThreads) {
  _queues[name] = new DispatcherQueue(this, name, defaultDispatcherThread, nrThreads);
}

/////////////////////////////////////////////////////////////////////////
/// @brief adds a queue which given dispatcher thread type
/////////////////////////////////////////////////////////////////////////

void Dispatcher::addQueue (string const& name, newDispatcherThread_fptr func, size_t nrThreads) {
  _queues[name] = new DispatcherQueue(this, name, func, nrThreads);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new job
////////////////////////////////////////////////////////////////////////////////

bool Dispatcher::addJob (Job* job) {

  // do not start new jobs if we are already shutting down
  if (_stopping != 0) {
    return false;
  }
  
  // try to find a suitable queue
  DispatcherQueue* queue = lookupQueue(job->queue());
  
  if (queue == 0) {
    LOGGER_WARNING << "unknown queue '" << job->queue() << "'";
    return false;
  }
  
  // log success, but do this BEFORE the real add, because the addJob might execute
  // and delete the job before we have a chance to log something
  LOGGER_TRACE << "added job " << job << " to queue " << job->queue();
  
  // add the job to the list of ready jobs
  queue->addJob(job);
  
  // indicate sucess, BUT never access job after it has been added to the queue
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the dispatcher
////////////////////////////////////////////////////////////////////////////////

bool Dispatcher::start () {
  MUTEX_LOCKER(_accessDispatcher);
  
  for (map<string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
    bool ok = i->second->start();
    
    if (! ok) {
      LOGGER_FATAL << "cannot start dispatcher queue";
      return false;
    }
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins shutdown process
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::beginShutdown () {
  if (_stopping != 0) {
    return;
  }
  
  MUTEX_LOCKER(_accessDispatcher);
  
  if (_stopping == 0) {
    LOGGER_DEBUG << "beginning shutdown sequence of dispatcher";
    
    _stopping = 1;
    
    for (map<string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
      i->second->beginShutdown();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reports status of dispatcher queues
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::reportStatus () {
  if (TRI_IsDebugLogging(__FILE__)) {
    MUTEX_LOCKER(_accessDispatcher);
    
    for (map<string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
      string const& name = i->first;
      DispatcherQueue* q = i->second;
      
      LOGGER_DEBUG << "dispatcher queue '" << name << "': "
                   << "threads = " << q->nrThreads << " "
                   << "started = " << q->nrStarted << " "
                   << "running = " << q->nrRunning << " "
                   << "waiting = " << q->nrWaiting << " "
                   << "stopped = " << q->nrStopped << " "
                   << "special = " << q->nrSpecial << " "
                   << "monopolistic = " << (q->monopolizer ? "yes" : "no");
      
      LOGGER_HEARTBEAT
      << LoggerData::Task("dispatcher status")
      << LoggerData::Extra(name)
      << LoggerData::Extra("threads")
      << LoggerData::Extra(StringUtils::itoa(q->nrThreads))
      << LoggerData::Extra("started")
      << LoggerData::Extra(StringUtils::itoa(q->nrStarted))
      << LoggerData::Extra("running")
      << LoggerData::Extra(StringUtils::itoa(q->nrRunning))
      << LoggerData::Extra("waiting")
      << LoggerData::Extra(StringUtils::itoa(q->nrWaiting))
      << LoggerData::Extra("stopped")
      << LoggerData::Extra(StringUtils::itoa(q->nrStopped))
      << LoggerData::Extra("special")
      << LoggerData::Extra(StringUtils::itoa(q->nrSpecial))
      << LoggerData::Extra("monopilizer")
      << LoggerData::Extra((q->monopolizer ? "1" : "0"))
      << "dispatcher status";
      
      CONDITION_LOCKER(guard, q->accessQueue);
      
      for (set<DispatcherThread*>::iterator j = q->startedThreads.begin();  j != q->startedThreads.end(); ++j) {
        (*j)->reportStatus();
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a queue
////////////////////////////////////////////////////////////////////////////////

DispatcherQueue* Dispatcher::lookupQueue (string const& name) {
  map<string, DispatcherQueue*>::const_iterator i = _queues.find(name);
  
  if (i == _queues.end()) {
    return 0;
  }
  else {
    return i->second;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End: