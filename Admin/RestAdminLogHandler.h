////////////////////////////////////////////////////////////////////////////////
/// @brief admin log request handler
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
/// @author Achim Brandt
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef BORNHOLM_ADMIN_REST_ADMIN_LOG_HANDLER_H
#define BORNHOLM_ADMIN_REST_ADMIN_LOG_HANDLER_H 1

#include <Admin/RestAdminBaseHandler.h>

namespace triagens {
  namespace admin {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief admin log request handler
    ////////////////////////////////////////////////////////////////////////////////

    class RestAdminLogHandler : public RestAdminBaseHandler {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructor
        ////////////////////////////////////////////////////////////////////////////////

        RestAdminLogHandler (rest::HttpRequest* request)
          : RestAdminBaseHandler(request) {
        }

      public:


        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        bool isDirect () {
          return true;
        }


        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        status_e execute ();
    };
  }
}

#endif