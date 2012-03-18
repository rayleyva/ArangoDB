////////////////////////////////////////////////////////////////////////////////
/// @brief document request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestDocumentHandler.h"

#include "Basics/StringUtils.h"
#include "BasicsC/string-buffer.h"
#include "Rest/HttpRequest.h"
#include "VocBase/simple-collection.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::avocado;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestDocumentHandler::RestDocumentHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestDocumentHandler::queue () {
  static string const client = "CLIENT";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestDocumentHandler::execute () {

  // extract the sub-request type
  HttpRequest::HttpRequestType type = request->requestType();

  // prepare logging
  static LoggerData::Task const logCreate(DOCUMENT_PATH + " [create]");
  static LoggerData::Task const logRead(DOCUMENT_PATH + " [read]");
  static LoggerData::Task const logUpdate(DOCUMENT_PATH + " [update]");
  static LoggerData::Task const logDelete(DOCUMENT_PATH + " [delete]");
  static LoggerData::Task const logHead(DOCUMENT_PATH + " [head]");
  static LoggerData::Task const logIllegal(DOCUMENT_PATH + " [illegal]");

  LoggerData::Task const * task = &logCreate;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE: task = &logDelete; break;
    case HttpRequest::HTTP_REQUEST_GET: task = &logRead; break;
    case HttpRequest::HTTP_REQUEST_HEAD: task = &logHead; break;
    case HttpRequest::HTTP_REQUEST_ILLEGAL: task = &logIllegal; break;
    case HttpRequest::HTTP_REQUEST_POST: task = &logCreate; break;
    case HttpRequest::HTTP_REQUEST_PUT: task = &logUpdate; break;
  }

  _timing << *task;
  LOGGER_REQUEST_IN_START_I(_timing);

  // execute one of the CRUD methods
  bool res = false;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE: res = deleteDocument(); break;
    case HttpRequest::HTTP_REQUEST_GET: res = readDocument(); break;
    case HttpRequest::HTTP_REQUEST_HEAD: res = checkDocument(); break;
    case HttpRequest::HTTP_REQUEST_POST: res = createDocument(); break;
    case HttpRequest::HTTP_REQUEST_PUT: res = updateDocument(); break;

    case HttpRequest::HTTP_REQUEST_ILLEGAL:
      res = false;
      generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
      break;
  }

  _timingResult = res ? RES_ERR : RES_OK;

  // this handler is done
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document
///
/// @REST{POST /document?collection=@FA{collection-identifier}}
///
/// Creates a new document in the collection identified by the
/// @FA{collection-identifier}.  A JSON representation of the document must be
/// passed as the body of the POST request.
///
/// If the document was created successfully, then a @LIT{HTTP 201} is returned
/// and the "Location" header contains the path to the newly created
/// document. The "ETag" header field contains the revision of the document.
///
/// The body of the response contains a JSON object with the same information.
/// The attribute @LIT{_id} contains the document handle of the newly created
/// document, the attribute @LIT{_rev} contains the document revision.
///
/// If the collection parameter @LIT{waitForSync} is @LIT{false}, then a
/// @LIT{HTTP 202} is returned in order to indicate that the document has been
/// accepted but not yet stored.
///
/// If the @FA{collection-identifier} is unknown, then a @LIT{HTTP 404} is
/// returned and the body of the response contains an error document.
///
/// If the body does not contain a valid JSON representation of an document,
/// then a @LIT{HTTP 400} is returned and the body of the response contains
/// an error document.
///
/// @REST{POST /document?collection=@FA{collection-identifier}&createCollection=@FA{create}}
///
/// Instead of a @FA{collection-identifier}, a collection name can be used. If
/// @FA{create} is true, then the collection is created if it does not exists.
///
/// @EXAMPLES
///
/// Create a document given a collection identifier @LIT{161039} for the collection
/// named @LIT{demo}. Note that the revision identifier might or might by equal to
/// the last part of the document handle. It generally will be equal, but there is
/// no guaranty.
///
/// @verbinclude rest_create-document
///
/// Create a document in collection where @LIT{waitForSync} is @LIT{false}.
///
/// @verbinclude rest_create-document-accept
///
/// Create a document in a known, named collection
///
/// @verbinclude rest_create-document-named-collection
///
/// Create a document in a new, named collection
///
/// @verbinclude rest_create-document-new-named-collection
///
/// Unknown collection identifier:
///
/// @verbinclude rest4
///
/// Illegal document:
///
/// @verbinclude rest5
///
/// Create a document given a collection name:
///
/// @verbinclude rest6
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::createDocument () {
  vector<string> const& suffix = request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_REST_ERROR_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  // extract the cid
  bool found;
  string cid = request->value("collection", found);

  if (! found || cid.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_VOC_ERROR_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  // should we create the collection
  string createStr = request->value("create", found);
  bool create = found ? StringUtils::boolean(createStr) : false;

  // find and load collection given by name oder identifier
  bool ok = findCollection(cid, create) && loadCollection();

  if (! ok) {
    return false;
  }

  // parse document
  TRI_json_t* json = parseJsonBody();

  if (json == 0) {
    return false;
  }

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  bool waitForSync = _documentCollection->base._waitForSync;

  // note: unlocked is performed by createJson() FIXME URGENT SHOULD RETURN A DOC_MPTR NOT A POINTER!!!
  TRI_doc_mptr_t const* mptr = _documentCollection->createJson(_documentCollection, TRI_DOC_MARKER_DOCUMENT, json, 0, true);
  TRI_voc_did_t did = 0;
  TRI_voc_rid_t rid = 0;

  if (mptr != 0) {
    did = mptr->_did;
    rid = mptr->_rid;
  }

  // .............................................................................
  // outside write transaction
  // .............................................................................

  TRI_FreeJson(json);

  if (mptr != 0) {
    if (waitForSync) {
      generateCreated(_documentCollection->base._cid, did, rid);
    }
    else {
      generateAccepted(_documentCollection->base._cid, did, rid);
    }

    return true;
  }
  else {
    if (TRI_errno() == TRI_VOC_ERROR_READ_ONLY) {
      generateError(HttpResponse::FORBIDDEN, 
                    TRI_VOC_ERROR_READ_ONLY,
                    "collection is read-only");
      return false;
    }
    else {
      generateError(HttpResponse::SERVER_ERROR,
                    TRI_ERROR_INTERNAL,
                    "cannot create, failed with: " + string(TRI_last_error()));
      return false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single or all documents
///
/// Either readSingleDocument or readAllDocuments.
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readDocument () {
#ifdef FIXME
  switch (request->suffix().size()) {
    case 0:
      return readAllDocuments();

    case 1:
      return readSingleDocument(true);

    default:
      generateError(HttpResponse::BAD, "expecting URI /document/<document-handle>");
      return false;
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document
///
/// @REST{GET /document/@FA{document-handle}}
///
/// Returns the document identified by @FA{document-handle}. The returned
/// document contains two special attributes: @LIT{_id} containing the document
/// handle and @LIT{_rev} containing the revision.
///
/// If the document exists, then a @LIT{HTTP 200} is returned and the JSON
/// representation of the document is the body of the response.
///
/// If the @FA{document-handle} points to a non-existing document, then a
/// @LIT{HTTP 404} is returned and the body contains an error document.
///
/// @EXAMPLES
///
/// Use a collection and document identfier:
///
/// @verbinclude rest1
///
/// Use a collection name and document reference:
///
/// @verbinclude rest18
///
/// Unknown document identifier:
///
/// @verbinclude rest2
///
/// Unknown collection identifier:
///
/// @verbinclude rest17
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readSingleDocument (bool generateBody) {
#ifdef FIXME
  vector<string> const& suffix = request->suffix();

  // split the document reference
  string cid;
  string did;

  bool ok = splitDocumentReference(suffix[0], cid, did);

  if (! ok) {
    return false;
  }

  // find and load collection given by name oder identifier
  ok = findCollection(cid) && loadCollection();

  if (! ok) {
    return false;
  }

  // .............................................................................
  // inside read transaction
  // .............................................................................

  _documentCollection->beginRead(_documentCollection);

  TRI_doc_mptr_t const* document = findDocument(did);

  _documentCollection->endRead(_documentCollection);

  // .............................................................................
  // outside read transaction
  // .............................................................................

  if (document == 0) {
    generateDocumentNotFound(suffix[0]);
    return false;
  }

  generateDocument(document, generateBody);
#endif
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads all documents
///
/// @REST{GET /document?collection=@FA{collection-identifier}}
///
/// Returns a list of all URI for all documents from the collection identified
/// by @FA{collection-identifier}.
///
/// Instead of a @FA{collection-identifier}, a collection name can be given.
///
/// @EXAMPLES
///
/// @verbinclude rest20
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readAllDocuments () {
#ifdef FIXME
  vector<string> const& suffix = request->suffix();

  // find and load collection given by name oder identifier
  bool ok = findCollection(cid) && loadCollection();

  if (! ok) {
    return false;
  }

  // .............................................................................
  // inside read transaction
  // .............................................................................

  vector<TRI_voc_did_t> ids;

  _documentCollection->beginRead(_documentCollection);

  try {
    TRI_sim_collection_t* collection = (TRI_sim_collection_t*) _documentCollection;

    if (0 < collection->_primaryIndex._nrUsed) {
      void** ptr = collection->_primaryIndex._table;
      void** end = collection->_primaryIndex._table + collection->_primaryIndex._nrAlloc;

      for (;  ptr < end;  ++ptr) {
        if (*ptr) {
          TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

          if (d->_deletion == 0) {
            ids.push_back(d->_did);
          }
        }
      }
    }
  }
  catch (...) {
    // necessary so we will always remove the read lock
  }
  
  _documentCollection->endRead(_documentCollection);

  // .............................................................................
  // outside read transaction
  // .............................................................................

  TRI_string_buffer_t buffer;

  TRI_InitStringBuffer(&buffer);

  TRI_AppendStringStringBuffer(&buffer, "{ \"documents\" : [\n");

  bool first = true;
  string prefix = "\"" + DOCUMENT_PATH + "/" + StringUtils::itoa(_documentCollection->base._cid) + "/";

  for (vector<TRI_voc_did_t>::iterator i = ids.begin();  i != ids.end();  ++i) {
    TRI_AppendString2StringBuffer(&buffer, prefix.c_str(), prefix.size());
    TRI_AppendUInt64StringBuffer(&buffer, *i);

    if (first) {
      prefix = "\",\n" + prefix;
      first = false;
    }
  }

  TRI_AppendStringStringBuffer(&buffer, "\"\n] }\n");

  // and generate a response
  response = new HttpResponse(HttpResponse::OK);
  response->setContentType("application/json; charset=utf-8");

  response->body().appendText(TRI_BeginStringBuffer(&buffer), TRI_LengthStringBuffer(&buffer));

  TRI_AnnihilateStringBuffer(&buffer);
#endif

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document head
///
/// @REST{HEAD /document/@FA{document-handle}}
///
/// Like @FN{GET}, but only returns the header fields and not the body. You
/// can use this call to get the current revision of a document or check if
/// the document was deleted.
///
/// @EXAMPLES
///
/// @verbinclude rest19
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::checkDocument () {
#ifdef FIXME
  vector<string> const& suffix = request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, "expecting URI /document/<document-handle>");
    return false;
  }

  return readSingleDocument(false);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
///
/// @REST{PUT /document/@FA{document-handle}}
///
/// Updates the document identified by @FA{document-handle}. If the document exists
/// and can be updated, then a @LIT{HTTP 201} is returned and the "ETag" header
/// field contains the new revision of the document.
///
/// Note the updated document passed in the body of the request normally also
/// contains the @FA{document-handle} in the attribute @LIT{_id} and the
/// revision in @LIT{_rev}. These attributes, however, are ignored. Only the URI
/// and the "ETag" header are relevant in order to avoid confusion when using
/// proxies.
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision.  The attribute @LIT{_id} contains the known
/// @FA{document-handle} of the updated document, the attribute @LIT{_rev}
/// contains the new document revision.
///
/// If the document does not exists, then a @LIT{HTTP 404} is returned and the
/// body of the response contains an error document.
///
/// If an etag is supplied in the "If-Match" header field, then the AvocadoDB
/// checks that the revision of the document is equal to the etag. If there is a
/// mismatch, then a @LIT{HTTP 409} conflict is returned and no update is
/// performed.
///
/// @REST{PUT /document/@FA{document-handle}?policy=@FA{policy}}
///
/// As before, if @FA{policy} is @LIT{error}. If @FA{policy} is @LIT{last},
/// then the last write will win.
///
/// @REST{PUT /document/@FA{collection-identifier}/@FA{document-identifier}?rev=@FA{etag}}
///
/// You can also supply the etag using the parameter @LIT{rev} instead of an "ETag"
/// header. You must never supply both the "ETag" header and the @LIT{rev}
/// parameter.
///
/// @EXAMPLES
///
/// Using collection and document identifier:
///
/// @verbinclude rest7
///
/// Unknown document identifier:
///
/// @verbinclude rest8
///
/// Produce a revision conflict:
///
/// @verbinclude rest9
///
/// Last write wins:
///
/// @verbinclude rest10
///
/// Alternative to ETag header field:
///
/// @verbinclude rest11
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::updateDocument () {
#ifdef FIXME
  vector<string> const& suffix = request->suffix();

  // split the document reference
  string cid;
  string did;

  ok = splitDocumentReference(suffix[1], cid, did);

  if (! ok) {
    return false;
  }

  // find and load collection given by name oder identifier
  bool ok = findCollection(cid) && loadCollection();

  if (! ok) {
    return false;
  }

  // parse document
  TRI_json_t* json = parseJsonBody();

  if (json == 0) {
    return false;
  }

  // extract document identifier
  TRI_voc_did_t did = StringUtils::uint64(didStr);

  // extract the revision
  TRI_voc_rid_t revision = extractRevision();

  // extract or chose the update policy
  TRI_doc_update_policy_e policy = extractUpdatePolicy();

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  // unlocking is performed in updateJson()
  TRI_doc_mptr_t const* mptr = _documentCollection->updateJson(_documentCollection, json, did, revision, policy, true);
  TRI_voc_rid_t rid = 0;

  if (mptr != 0) {
    rid = mptr->_rid;
  }

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (mptr != 0) {
    generateOk(_documentCollection->base._cid, did, rid);
    return true;
  }
  else {
    if (TRI_errno() == TRI_VOC_ERROR_READ_ONLY) {
      generateError(HttpResponse::FORBIDDEN, "collection is read-only");
      return false;
    }
    else if (TRI_errno() == TRI_VOC_ERROR_DOCUMENT_NOT_FOUND) {
      generateDocumentNotFound(suffix[0], didStr);
      return false;
    }
    else if (TRI_errno() == TRI_VOC_ERROR_CONFLICT) {
      generateConflict(suffix[0], didStr);
      return false;
    }
    else {
      generateError(HttpResponse::SERVER_ERROR, "cannot update, failed with " + string(TRI_last_error()));
      return false;
    }
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
///
/// @REST{DELETE /document/@FA{document-handle}}
///
/// Deletes the document identified by @FA{document-handle} from the collection
/// identified by @FA{collection-identifier}. If the document exists and could
/// be deleted, then a @LIT{HTTP 204} is returned.
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision.  The attribute @LIT{_id} contains the known
/// @FA{document-handle} of the updated document, the attribute @LIT{_rev}
/// contains the known document revision.
///
/// If the document does not exists, then a @LIT{HTTP 404} is returned and the
/// body of the response contains an error document.
///
/// If an etag is supplied in the "If-Match" field, then the AvocadoDB checks
/// that the revision of the document is equal to the tag. If there is a
/// mismatch, then a @LIT{HTTP 409} conflict is returned and no delete is
/// performed.
///
/// @REST{DELETE /document/@FA{document-handle}?policy=@FA{policy}}
///
/// As before, if @FA{policy} is @LIT{error}. If @FA{policy} is @LIT{last}, then
/// the last write will win.
///
/// @REST{DELETE /document/@FA{document-handle}?rev=@FA{etag}}
///
/// You can also supply the etag using the parameter @LIT{rev} instead of an "ETag"
/// header. You must never supply both the "ETag" header and the @LIT{rev}
/// parameter.
///
/// @EXAMPLES
///
/// Using collection and document identifier:
///
/// @verbinclude rest13
///
/// Unknown document identifier:
///
/// @verbinclude rest14
///
/// Revision conflict:
///
/// @verbinclude rest12
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::deleteDocument () {
#ifdef FIXME
  vector<string> const& suffix = request->suffix();

  // split the document reference
  string didStr;
  ok = splitDocumentReference(suffix[1], didStr);

  if (! ok) {
    return false;
  }

  // find and load collection given by name oder identifier
  bool ok = findCollection(cid[0]) && loadCollection();

  if (! ok) {
    return false;
  }

  // extract document identifier
  TRI_voc_did_t did = StringUtils::uint64(didStr);

  // extract the revision
  TRI_voc_rid_t revision = extractRevision();

  // extract or chose the update policy
  TRI_doc_update_policy_e policy = extractUpdatePolicy();

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  // unlocking is performed in destroy()
  ok = _documentCollection->destroy(_documentCollection, did, revision, policy, true);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (ok) {
    generateOk();
    return true;
  }
  else {
    if (TRI_errno() == TRI_VOC_ERROR_READ_ONLY) {
      generateError(HttpResponse::FORBIDDEN, "collection is read-only");
      return false;
    }
    else if (TRI_errno() == TRI_VOC_ERROR_DOCUMENT_NOT_FOUND) {
      generateDocumentNotFound(suffix[0], didStr);
      return false;
    }
    else if (TRI_errno() == TRI_VOC_ERROR_CONFLICT) {
      generateConflict(suffix[0], didStr);
      return false;
    }
    else {
      generateError(HttpResponse::SERVER_ERROR, "cannot delete, failed with " + string(TRI_last_error()));
      return false;
    }
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End: