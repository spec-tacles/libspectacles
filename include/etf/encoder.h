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

#include <cmath>
#include <limits>
#include "encode_func.h"

namespace spectacles {
  namespace etf {
    struct out_buf {
      size_t length;
      char *buf;
    };

    class Encoder {
      static const size_t DEFAULT_RECURSE_LIMIT = 256;
      static const size_t INITIAL_BUFFER_SIZE = 1024 * 1024;

    public:
      Encoder() {
        ret = 0;
        pk.buf = (char*)malloc(INITIAL_BUFFER_SIZE);
        pk.length = 0;
        pk.allocated_size = INITIAL_BUFFER_SIZE;

        ret = erlpack_append_version(&pk);
        if (ret == -1) {
          throw std::runtime_error("Unable to allocate large buffer for encoding.");
        }
      }

      out_buf release() {
        out_buf buf;
        buf.buf = pk.buf;
        buf.length = pk.length;
        return buf;
      }

      ~Encoder() {
        if (pk.buf) {
          free(pk.buf);
        }

        pk.buf = NULL;
        pk.length = 0;
        pk.allocated_size = 0;
      }

      int pack(Data value, const int nestLimit = DEFAULT_RECURSE_LIMIT) {
        ret = 0;

        if (nestLimit < 0) {
          throw std::runtime_error("Reached recursion limit");
          return -1;
        }

        if (value.isInt32() || value.isUint32()) {
          int number = value;
          if (number >= 0 && number <= 255) {
            unsigned char num = (unsigned char)number;
            ret = erlpack_append_small_integer(&pk, num);
          }
          else if (value.isInt32()) {
            ret = erlpack_append_integer(&pk, number);
          }
          else if (value.isUint32()) {
            unsigned int uNum = value;
            auto uLong = (unsigned long long)uNum;
            ret = erlpack_append_unsigned_long_long(&pk, uLong);
          }
        }
        else if(value.isDouble()) {
          double decimal = value;
          ret = erlpack_append_double(&pk, decimal);
        }
        else if (value.isNull() || value.isUndefined()) {
          ret = erlpack_append_nil(&pk);
        }
        else if (value.isTrue()) {
          ret = erlpack_append_true(&pk);
        }
        else if(value.isFalse()) {
          ret = erlpack_append_false(&pk);
        }
        else if(value.isString()) {
          ret = erlpack_append_binary(&pk, value, value.size());
        }
        else if (value.isArray()) {
          const size_t length = value.size();
          if (length == 0) {
            ret = erlpack_append_nil_ext(&pk);
          }
          else {
            if (length > std::numeric_limits<uint32_t>::max() - 1) {
              throw std::runtime_error("List is too large");
              return -1;
            }

            ret = erlpack_append_list_header(&pk, length);
            if (ret != 0) {
              return ret;
            }

            for(size_t i = 0; i < length; ++i) {
              ret = pack(value[i], nestLimit - 1);
              if (ret != 0) {
              return ret;
              }
            }

            ret = erlpack_append_nil_ext(&pk);
          }
        }
        else if (value.isMap()) {
          const size_t len = value.size();
          if (len > std::numeric_limits<uint32_t>::max() - 1) {
            throw std::runtime_error("Dictionary has too many properties");
            return -1;
          }

          ret = erlpack_append_map_header(&pk, len);
          if (ret != 0) {
            return ret;
          }

          for (auto const& x : value.map) {
            Data k = x.first;
            Data v = x.second;

            ret = pack(k, nestLimit - 1);
            if (ret != 0) {
              return ret;
            }

            ret = pack(v, nestLimit - 1);
            if (ret != 0) {
              return ret;
            }
          }
        }

        return ret;
      }

    private:
      int ret;
      erlpack_buffer pk;
    };
  }
}
