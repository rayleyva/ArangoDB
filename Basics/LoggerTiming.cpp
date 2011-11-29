////////////////////////////////////////////////////////////////////////////////
/// @brief logger timing
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
/// @author Copyright 2007-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "LoggerTiming.h"

using namespace triagens::basics;
using namespace std;

namespace triagens {
  namespace basics {

   // -----------------------------------------------------------------------------
   // logger timing enabled
   // -----------------------------------------------------------------------------

#ifdef TRI_ENABLE_LOGGER_TIMING

    LoggerTiming::LoggerTiming ()
      : timing(Timing::TI_WALLCLOCK) {
    }



    void LoggerTiming::measure () {
      info.measure = LoggerData::Measure((double) timing.time(), LoggerData::UNIT_MICRO_SECONDS);
    }

#else

   // -----------------------------------------------------------------------------
   // logger timing disabled
   // -----------------------------------------------------------------------------

    LoggerTiming::LoggerTiming () {
    }



    void LoggerTiming::measure () {
      info.measure = LoggerData::Measure(0.0, LoggerData::UNIT_MICRO_SECONDS);
    }

#endif
  }
}

