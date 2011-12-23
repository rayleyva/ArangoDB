////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for variant objects
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

#ifndef TRIAGENS_JUTLAND_BASICS_VARIANT_OBJECT_H
#define TRIAGENS_JUTLAND_BASICS_VARIANT_OBJECT_H 1

#include <Basics/Common.h>

#include <Basics/Exceptions.h>

////////////////////////////////////////////////////////////////////////////////
/// @defgroup Variants Variant Objects
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace basics {
    struct StringBuffer;

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup Variants
    /// @brief abstract base class for variant objects
    ////////////////////////////////////////////////////////////////////////////////

    class VariantObject {
      private:
        VariantObject (VariantObject const&);
        VariantObject& operator= (VariantObject const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief enumeration of all non-abstract object types
        ////////////////////////////////////////////////////////////////////////////////

        enum ObjectType {
          VARIANT_ARRAY,
          VARIANT_BLOB,
          VARIANT_BOOLEAN,
          VARIANT_DATE,
          VARIANT_DATETIME,
          VARIANT_DOUBLE,
          VARIANT_FLOAT,
          VARIANT_INT8,
          VARIANT_INT16,
          VARIANT_INT32,
          VARIANT_INT64,
          VARIANT_MATRIX2,
          VARIANT_NULL,
          VARIANT_ROW,
          VARIANT_STRING,
          VARIANT_UINT8,
          VARIANT_UINT16,
          VARIANT_UINT32,
          VARIANT_UINT64,
          VARIANT_VECTOR
        };

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief name of an object types
        ////////////////////////////////////////////////////////////////////////////////

        static string NameObjectType (ObjectType);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a result object
        ////////////////////////////////////////////////////////////////////////////////

        VariantObject ();

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructs a result object
        ////////////////////////////////////////////////////////////////////////////////

        virtual ~VariantObject ();

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief converts into specific subtype or raises an error
        ////////////////////////////////////////////////////////////////////////////////

        template<typename T> T* as () {
          T* c = dynamic_cast<T*>(this);

          if (c == 0) {
            THROW_INTERNAL_ERROR("illegal cast");
          }

          return c;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief checks for specific subtype
        ////////////////////////////////////////////////////////////////////////////////

        template<typename T> bool is () const {
          return this->type() == T::TYPE;
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the object type
        ////////////////////////////////////////////////////////////////////////////////

        virtual ObjectType type () const = 0;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief clones an object
        ////////////////////////////////////////////////////////////////////////////////

        virtual VariantObject* clone () const = 0;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief prints an object to a string buffer für debugging
        ////////////////////////////////////////////////////////////////////////////////

        virtual void print (StringBuffer&, size_t indent) const = 0;

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief print indentation
        ////////////////////////////////////////////////////////////////////////////////

        void printIndent (StringBuffer&, size_t indent) const;
    };

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief helper class for simple types
    ////////////////////////////////////////////////////////////////////////////////

    template<typename C, enum VariantObject::ObjectType OT, typename VT>
    class VariantObjectTemplate : public VariantObject {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief type of VariantObject
        ////////////////////////////////////////////////////////////////////////////////

        static ObjectType const TYPE = OT;

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new simple variant
        ////////////////////////////////////////////////////////////////////////////////

        VariantObjectTemplate (C value)
          : value(value) {
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        ObjectType type () const {
          return TYPE;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        VariantObject* clone () const {
          return new VT(value);
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the name
        ////////////////////////////////////////////////////////////////////////////////

        C getValue () const {
          return value;
        }

      private:
        C value;
    };
  }
}

#endif