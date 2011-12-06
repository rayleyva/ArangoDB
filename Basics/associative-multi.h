////////////////////////////////////////////////////////////////////////////////
/// @brief associative multi array implementation
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
/// @author Martin Schoenert
/// @author Copyright 2006-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_PHILADELPHIA_BASICS_ASSOCIATIVE_MULTI_H
#define TRIAGENS_PHILADELPHIA_BASICS_ASSOCIATIVE_MULTI_H 1

#include <Basics/Common.h>

#include <Basics/locks.h>
#include <Basics/vector.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief associative multi array of pointers
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_multi_pointer_s {
  uint64_t (*hashKey) (struct TRI_multi_pointer_s*, void const*);
  uint64_t (*hashElement) (struct TRI_multi_pointer_s*, void const*);

  bool (*isEqualKeyElement) (struct TRI_multi_pointer_s*, void const*, void const*);
  bool (*isEqualElementElement) (struct TRI_multi_pointer_s*, void const*, void const*);

  uint64_t _nrAlloc; // the size of the table
  uint64_t _nrUsed; // the number of used entries

  void** _table; // the table itself

  uint64_t _nrFinds; // statistics: number of lookup calls
  uint64_t _nrAdds; // statistics: number of insert calls
  uint64_t _nrRems; // statistics: number of remove calls
  uint64_t _nrResizes; // statistics: number of resizes

  uint64_t _nrProbesF; // statistics: number of misses while looking up
  uint64_t _nrProbesA; // statistics: number of misses while inserting
  uint64_t _nrProbesD; // statistics: number of misses while removing
  uint64_t _nrProbesR; // statistics: number of misses while adding
}
TRI_multi_pointer_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        MULTI ASSOCIATIVE POINTERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMultiPointer (TRI_multi_pointer_t* array,
                           uint64_t (*hashKey) (TRI_multi_pointer_t*, void const*),
                           uint64_t (*hashElement) (TRI_multi_pointer_t*, void const*),
                           bool (*isEqualKeyElement) (TRI_multi_pointer_t*, void const*, void const*),
                           bool (*isEqualElementElement) (TRI_multi_pointer_t*, void const*, void const*));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMultiPointer (TRI_multi_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeMultiPointer (TRI_multi_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByKeyMultiPointer (TRI_multi_pointer_t*, void const* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementMultiPointer (TRI_multi_pointer_t*, void const* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementMultiPointer (TRI_multi_pointer_t*, void* element, bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveElementMultiPointer (TRI_multi_pointer_t*, void const* element);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End: