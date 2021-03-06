////////////////////////////////////////////////////////////////////////////////
/// @brief mruby client connection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "MRubyClientConnection.h"

#include <sstream>

#include "Basics/StringUtils.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Variant/VariantArray.h"
#include "Variant/VariantString.h"

#include "json.h"

extern "C" {
#include "mruby/array.h"
#include "mruby/hash.h"
}

using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::mrclient;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ClientConnection
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

MRubyClientConnection::MRubyClientConnection (MR_state_t* mrs,
                                              const std::string& hostname,
                                              int port,
                                              double requestTimeout,
                                              size_t retries,
                                              double connectionTimeout,
                                              bool warn)
  : _mrs(mrs),
    _connected(false),
    _lastHttpReturnCode(0),
    _lastErrorMessage(""),
    _client(0),
    _httpResult(0) {
      
  _client = new SimpleHttpClient(hostname, port, requestTimeout, retries, connectionTimeout, warn);

  // connect to server and get version number
  map<string, string> headerFields;
  SimpleHttpResult* result = _client->request(SimpleHttpClient::GET, "/_api/version", 0, 0, headerFields);

  if (!result->isComplete()) {
    // save error message
    _lastErrorMessage = _client->getErrorMessage();
    _lastHttpReturnCode = 500;
  }
  else {
    _lastHttpReturnCode = result->getHttpReturnCode();

    if (result->getHttpReturnCode() == SimpleHttpResult::HTTP_STATUS_OK) {
      triagens::basics::VariantArray* json = result->getBodyAsVariantArray();

      if (json) {
        triagens::basics::VariantString* vs = json->lookupString("server");

        if (vs && vs->getValue() == "arango") {
          // connected to arango server
          _connected = true;
          vs = json->lookupString("version");

          if (vs) {
            _version = vs->getValue();
          }
        }

        delete json;
      }
    }        
  }
  
  delete result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

MRubyClientConnection::~MRubyClientConnection () {
  if (_httpResult) {
    delete _httpResult;
  }

  if (_client) {
    delete _client;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRubyClientConnection
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if it is connected
////////////////////////////////////////////////////////////////////////////////

bool MRubyClientConnection::isConnected () {
  return _connected;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the version and build number of the arango server
////////////////////////////////////////////////////////////////////////////////

const string& MRubyClientConnection::getVersion () {
  return _version;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last http return code
////////////////////////////////////////////////////////////////////////////////

int MRubyClientConnection::getLastHttpReturnCode () {
  return _lastHttpReturnCode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last error message
////////////////////////////////////////////////////////////////////////////////

const std::string& MRubyClientConnection::getErrorMessage () {
  return _lastErrorMessage;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the hostname 
////////////////////////////////////////////////////////////////////////////////

const std::string& MRubyClientConnection::getHostname () {
  return _client->getHostname();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the port
////////////////////////////////////////////////////////////////////////////////

int MRubyClientConnection::getPort () {
  return _client->getPort();
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief get the simple http client
////////////////////////////////////////////////////////////////////////////////
            
triagens::httpclient::SimpleHttpClient* MRubyClientConnection::getHttpClient() {
  return _client;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "GET" request
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::getData (std::string const& location,
                                          map<string, string> const& headerFields) {
  return requestData(SimpleHttpClient::GET, location, "", headerFields);
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief do a "DELETE" request
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::deleteData (std::string const& location,
                                             map<string, string> const& headerFields) {
  return requestData(SimpleHttpClient::DELETE, location, "", headerFields);
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief do a "HEAD" request
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::headData (std::string const& location,
                                           map<string, string> const& headerFields) {
  return requestData(SimpleHttpClient::HEAD, location, "", headerFields);
}    

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "POST" request
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::postData (std::string const& location,
                                           std::string const& body,
                                           map<string, string> const& headerFields) {
  return requestData(SimpleHttpClient::POST, location, body, headerFields);
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief do a "PUT" request
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::putData (std::string const& location,
                                          std::string const& body, 
                                          map<string, string> const& headerFields) {
  return requestData(SimpleHttpClient::PUT, location, body, headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRubyClientConnection
/// @{
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::requestData (int method,
                                              string const& location,
                                              string const& body,
                                              map<string, string> const& headerFields) {
  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;
      
  if (_httpResult) {
    delete _httpResult;
  }
      
  if (body.empty()) {
    _httpResult = _client->request(method, location, 0, 0, headerFields);
  }
  else {
    _httpResult = _client->request(method, location, body.c_str(), body.length(), headerFields);
  }
                  
  if (!_httpResult->isComplete()) {
    // not complete
    _lastErrorMessage = _client->getErrorMessage();
        
    if (_lastErrorMessage.empty()) {
      _lastErrorMessage = "Unknown error";
    }
        
    _lastHttpReturnCode = SimpleHttpResult::HTTP_STATUS_SERVER_ERROR;
        
    mrb_value result = mrb_hash_new_capa(&_mrs->_mrb, 2);

    mrb_hash_set(&_mrs->_mrb, result, _mrs->_errorSym, mrb_true_value());
    mrb_hash_set(&_mrs->_mrb, result, _mrs->_codeSym, mrb_fixnum_value(SimpleHttpResult::HTTP_STATUS_SERVER_ERROR));

    int errorNumber = 0;

    switch (_httpResult->getResultType()) {
      case (SimpleHttpResult::COULD_NOT_CONNECT) :
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT;
        break;
            
      case (SimpleHttpResult::READ_ERROR) :
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_READ;
        break;

      case (SimpleHttpResult::WRITE_ERROR) :
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_WRITE;
        break;

      default:
        errorNumber = TRI_SIMPLE_CLIENT_UNKNOWN_ERROR;
        break;
    }        
        
    mrb_hash_set(&_mrs->_mrb, result, _mrs->_errorNumSym, mrb_fixnum_value(errorNumber));
    mrb_hash_set(&_mrs->_mrb,
                 result,
                 _mrs->_errorMessageSym, 
                 mrb_str_new(&_mrs->_mrb, _lastErrorMessage.c_str(), _lastErrorMessage.length()));

    return result;
  }
  else {
    // complete        
    _lastHttpReturnCode = _httpResult->getHttpReturnCode();
        
    // got a body
    if (_httpResult->getBody().str().length() > 0) {
      string contentType = _httpResult->getContentType(true);

      if (contentType == "application/json") {
        TRI_json_t* js = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, _httpResult->getBody().str().c_str());

        if (js != NULL) {
          // return v8 object
          mrb_value result = MR_ObjectJson(&_mrs->_mrb, js);
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, js);

          return result;
        }
      }

      // return body as string
      mrb_value result = mrb_str_new(&_mrs->_mrb, 
                                     _httpResult->getBody().str().c_str(),
                                     _httpResult->getBody().str().length());

      return result;
    }
    else {
      // no body 
      // this should not happen
      mrb_value result;

      mrb_hash_set(&_mrs->_mrb, result, _mrs->_errorSym, mrb_false_value());
      mrb_hash_set(&_mrs->_mrb, result, _mrs->_codeSym, mrb_fixnum_value(_httpResult->getHttpReturnCode()));

      return result;
    }        
  }      
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
