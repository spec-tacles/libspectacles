/*
Original work Copyright 2015 Hammer and Chisel, Inc.
Modified work Copyright 2018 Zachary Vacura

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Acknowledgements:
    sysdep.h:
        Based on work by FURUHASHI Sadayuki in msgpack-python
        (https://github.com/msgpack/msgpack-python)

        Copyright (C) 2008-2010 FURUHASHI Sadayuki
        Licensed under the Apache License, Version 2.0 (the "License").

    zlib:
        version 1.2.8, April 28th, 2013

        Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler

        This software is provided 'as-is', without any express or implied
        warranty.  In no event will the authors be held liable for any damages
        arising from the use of this software.

        Permission is granted to anyone to use this software for any purpose,
        including commercial applications, and to alter it and redistribute it
        freely, subject to the following restrictions:

        1. The origin of this software must not be misrepresented; you must not
         claim that you wrote the original software. If you use this software
         in a product, an acknowledgment in the product documentation would be
         appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not be
         misrepresented as being the original software.
        3. This notice may not be removed or altered from any source distribution.

        Jean-loup Gailly        Mark Adler
        jloup@gzip.org          madler@alumni.caltech.edu

MIT License

Original work Copyright (c) 2017 Discord
Modified work Copyright (c) 2018 Zachary Vacura

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

#include <zlib.h>
#include <cinttypes>
#include <cstring>
#include <cstdio>
#include <map>
#include <vector>

#include <iostream>

#include "constants.h"
#include "data.h"
#include "sysdep.h"

#define THROW(msg) isInvalid = true; printf("[Error %s:%d] %s\n", __FILE__, __LINE__, msg)

namespace spectacles {
  namespace etf {
    class Decoder {
    public:
      Decoder(const uint8_t* data_, size_t length_, bool skipVersion = false)
      : data(data_)
      , size(length_)
      , isInvalid(false)
      , offset(0)
      {
        if (!skipVersion) {
          const auto version = read8();
          if (version != FORMAT_VERSION) {
            THROW("Bad version number.");
            isInvalid = true;
          }
        }
      }

      uint8_t read8() {
        if (offset + sizeof(uint8_t) > size) {
          THROW("Reading a byte passes the end of the buffer.");
          return 0;
        }
        auto val = *reinterpret_cast<const uint8_t*>(data + offset);
        offset += sizeof(uint8_t);
        return val;
      }

      uint16_t read16() {
        if (offset + sizeof(uint16_t) > size) {
          THROW("Reading two bytes passes the end of the buffer.");
          return 0;
        }

        uint16_t val = _erlpack_be16(*reinterpret_cast<const uint16_t*>(data + offset));
        offset += sizeof(uint16_t);
        return val;
      }

      uint32_t read32() {
        if (offset + sizeof(uint32_t) > size) {
          THROW("Reading three bytes passes the end of the buffer.");
          return 0;
        }

        uint32_t val = _erlpack_be32(*reinterpret_cast<const uint32_t*>(data + offset));
        offset += sizeof(uint32_t);
        return val;
      }

      uint64_t read64() {
        if (offset + sizeof(uint64_t) > size) {
          THROW("Reading four bytes passes the end of the buffer.");
          return 0;
        }

        uint64_t val = _erlpack_be64(*reinterpret_cast<const uint64_t*>(data + offset));
        offset += sizeof(val);
        return val;
      }

      Data decodeSmallInteger() {
        return Data(read8());
      }

      Data decodeInteger() {
        return Data((int32_t)read32());
      }

      Data decodeArray(uint32_t length) {
        std::vector<Data> array(length);
        for (uint32_t i = 0; i < length; i++) {
          auto value = unpack();
          if (isInvalid) {
            return Data::Undefined();
          }
          array[i] = value;
        }

        return Data(array);
      }

      Data decodeList() {
        const uint32_t length = read32();
        auto array = decodeArray(length);

        const auto tailMarker = read8();
        if (tailMarker != NIL_EXT) {
          THROW("List doesn't end with a tail marker, but it must!");
          return Data::Null();
        }

        return array;
      }

      Data decodeTuple(uint32_t length) {
        return decodeArray(length);
      }

      Data decodeNil() {
        std::vector<Data> array(0);
        return Data(array);
      }

      Data decodeMap() {
        const uint32_t length = read32();
        std::map<Data, Data> map;

        for(uint32_t i = 0; i < length; ++i) {
          const auto key = unpack();
          const auto value = unpack();
          if (isInvalid) {
            return Data::Undefined();
          }
          map[key] = value;
        }

        return Data(map);
      }

      const char* readString(uint32_t length) {
        if (offset + length > size) {
          THROW("Reading sequence past the end of the buffer.");
          return NULL;
        }

        const uint8_t* str = data + offset;
        offset += length;
        return (const char*)str;
      }

      Data processAtom(const char* atom, uint16_t length) {
        if (atom == NULL) {
          return Data::Undefined();
        }

        if (length >= 3 && length <= 5) {
          if (length == 3 && strncmp(atom, "nil", 3) == 0) {
            return Data::Null();
          }
          else if (length == 4 && strncmp(atom, "null", 4) == 0) {
            return Data::Null();
          }
          else if(length == 4 && strncmp(atom, "true", 4) == 0) {
            return Data::True();
          }
          else if (length == 5 && strncmp(atom, "false", 5) == 0) {
            return Data::False();
          }
        }

        return Data(atom, length);
      }

      Data decodeAtom() {
        auto length = read16();
        const char* atom = readString(length);
        return processAtom(atom, length);
      }

      Data decodeSmallAtom() {
        auto length = read8();
        const char* atom = readString(length);
        return processAtom(atom, length);
      }

      Data decodeFloat() {
        const uint8_t FLOAT_LENGTH = 31;
        const char* floatStr = readString(FLOAT_LENGTH);
        if (floatStr == NULL) {
          return Data::Undefined();
        }

        double number;
        char nullTerimated[FLOAT_LENGTH + 1] = {0};
        memcpy(nullTerimated, floatStr, FLOAT_LENGTH);

        auto count = sscanf(nullTerimated, "%lf", &number);
        if (count != 1) {
          THROW("Invalid float encoded.");
          return Data::Null();
        }

        return Data(number);
      }

      Data decodeNewFloat() {
        union {
          uint64_t ui64;
          double df;
        } val;
        val.ui64 = read64();
        return Data(val.df);
      }

      Data decodeBig(uint32_t digits) {
        const uint8_t sign = read8();

        if (digits > 8) {
          THROW("Unable to decode big ints larger than 8 bytes");
          return Data::Null();
        }

        uint64_t value = 0;
        uint64_t b = 1;
        for(uint32_t i = 0; i < digits; ++i) {
          uint64_t digit = read8();
          value += digit * b;
          b <<= 8;
        }

        if (digits <= 4) {
          if (sign == 0) {
            return Data(static_cast<uint32_t>(value));
          }

          const bool isSignBitAvailable = (value & (1 << 31)) == 0;
          if (isSignBitAvailable) {
            int32_t negativeValue = -static_cast<int32_t>(value);
            return Data(negativeValue);
          }
        }

        char outBuffer[32] = {0}; // 9223372036854775807
        const char* const formatString = sign == 0 ? "%" PRIu64 : "-%" PRIu64;
        const int res = sprintf(outBuffer, formatString, value);

        if (res < 0) {
          THROW("Unable to convert big int to string");
          return Data::Null();
        }
        const uint8_t length = static_cast<const uint8_t>(res);

        return Data(outBuffer, length);
      }

      Data decodeSmallBig() {
        const auto bytes = read8();
        return decodeBig(bytes);
      }

      Data decodeLargeBig() {
        const auto bytes = read32();
        return decodeBig(bytes);
      }

      Data decodeBinaryAsString() {
        const auto length = read32();
        const char* str = readString(length);
        if (str == NULL) {
          return Data::Undefined();
        }
        return Data(str, length);
      }

      Data decodeString() {
        const auto length = read16();
        const char* str = readString(length);
        if (str == NULL) {
          return Data::Undefined();
        }
        return Data(str, length);
      }

      Data decodeStringAsList() {
        const auto length = read16();
        if (offset + length > size) {
          THROW("Reading sequence past the end of the buffer.");
          return Data::Null();
        }

        std::vector<Data> array(length);
        for(uint16_t i = 0; i < length; ++i) {
          array[i] = decodeSmallInteger();
        }

        return Data(array);
      }

      Data decodeSmallTuple() {
        return decodeTuple(read8());
      }

      Data decodeLargeTuple() {
        return decodeTuple(read32());
      }

      Data decodeCompressed() {
        const uint32_t uncompressedSize = read32();

        unsigned long sourceSize = uncompressedSize;
        uint8_t* outBuffer = (uint8_t*)malloc(uncompressedSize);
        const int ret = uncompress(outBuffer, &sourceSize, (const unsigned char*)(data + offset), (uLong)(size - offset));

        offset += sourceSize;
        if (ret != Z_OK) {
          free(outBuffer);
          THROW("Failed to uncompresss compressed item");
          return Data::Null();
        }

        Decoder children(outBuffer, uncompressedSize, true);
        Data value = children.unpack();
        free(outBuffer);
        return value;
      }

      Data decodeReference() {
        std::map<Data, Data> reference;
        reference[Data("node")] = unpack();

        std::vector<Data> ids(1);
        ids[0] = Data(read32());
        reference[Data("id")] = Data(ids);

        reference[Data("creation")] = Data(read8());

        return Data(reference);
      }

      Data decodeNewReference() {
        std::map<Data, Data> reference;

        uint16_t len = read16();
        reference[Data("node")] = unpack();
        reference[Data("creation")] = Data(read8());

        std::vector<Data> ids(len);
        for(uint16_t i = 0; i < len; ++i) {
          ids[i] = Data(read32());
        }
        reference[Data("id")] = Data(ids);

        return Data(reference);
      }

      Data decodePort() {
        std::map<Data, Data> port;
        port[Data("node")] = unpack();
        port[Data("id")] = Data(read32());
        port[Data("creation")]  = Data(read8());
        return Data(port);
      }

      Data decodePID() {
        std::map<Data, Data> pid;
        pid[Data("node")] = unpack();
        pid[Data("id")] = Data(read32());
        pid[Data("serial")] = Data(read32());
        pid[Data("creation")]  = Data(read8());
        return Data(pid);
      }

      Data decodeExport() {
        std::map<Data, Data> exp;
        exp[Data("mod")] = unpack();
        exp[Data("fun")] = unpack();
        exp[Data("arity")] = unpack();
        return Data(exp);
      }

      Data unpack() {
        if (isInvalid) {
          return Data::Undefined();
        }

        if(offset >= size) {
          THROW("Unpacking beyond the end of the buffer");
          return Data::Undefined();
        }

        const auto type = read8();
        switch(type) {
          case SMALL_INTEGER_EXT:
            return decodeSmallInteger();
          case INTEGER_EXT:
            return decodeInteger();
          case FLOAT_EXT:
            return decodeFloat();
          case NEW_FLOAT_EXT:
            return decodeNewFloat();
          case ATOM_EXT:
            return decodeAtom();
          case SMALL_ATOM_EXT:
            return decodeSmallAtom();
          case SMALL_TUPLE_EXT:
            return decodeSmallTuple();
          case LARGE_TUPLE_EXT:
            return decodeLargeTuple();
          case NIL_EXT:
            return decodeNil();
          case STRING_EXT:
            return decodeStringAsList();
          case LIST_EXT:
            return decodeList();
          case MAP_EXT:
            return decodeMap();
          case BINARY_EXT:
            return decodeBinaryAsString();
          case SMALL_BIG_EXT:
            return decodeSmallBig();
          case LARGE_BIG_EXT:
            return decodeLargeBig();
          case REFERENCE_EXT:
            return decodeReference();
          case NEW_REFERENCE_EXT:
            return decodeNewReference();
          case PORT_EXT:
            return decodePort();
          case PID_EXT:
            return decodePID();
          case EXPORT_EXT:
            return decodeExport();
          case COMPRESSED:
            return decodeCompressed();
          default:
            THROW("Unsupported erlang term type identifier found");
            return Data::Undefined();
        }

        return Data::Undefined();
      }
    private:
      const uint8_t* const data;
      const size_t size;
      bool isInvalid;
      size_t offset;
    };
  }
}
