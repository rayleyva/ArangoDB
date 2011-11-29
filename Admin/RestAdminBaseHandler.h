////////////////////////////////////////////////////////////////////////////////
/// @brief default handler for admin handlers
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef BORNHOLM_ADMIN_REST_ADMIN_BASE_HANDLER_H
#define BORNHOLM_ADMIN_REST_ADMIN_BASE_HANDLER_H 1

#include <Admin/RestBaseHandler.h>

#include <Admin/Right.h>

namespace triagens {
  namespace admin {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief default handler for admin handlers
    ////////////////////////////////////////////////////////////////////////////////

    class RestAdminBaseHandler : public RestBaseHandler {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructor
        ////////////////////////////////////////////////////////////////////////////////

        RestAdminBaseHandler (rest::HttpRequest* request);

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief checks if authentication session has given right
        ////////////////////////////////////////////////////////////////////////////////

        virtual bool hasRight (right_t);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief checks if authentication session is bound to given user
        ////////////////////////////////////////////////////////////////////////////////

        virtual bool isSelf (string const& username);
    };
  }
}

#endif
