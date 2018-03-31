#ifndef SPECTACLES_INCLUDE_ETF_DATA_H_
#define SPECTACLES_INCLUDE_ETF_DATA_H_

#include <cinttypes>

#include <map>
#include <vector>

#define UNDEFINED_TYPE 0
#define NULL_TYPE 1
#define BOOL_TYPE 2
#define SINT_TYPE 3
#define UINT_TYPE 4
#define DOUBLE_TYPE 5
#define STRING_TYPE 6

#define ARRAY_TYPE 7
#define MAP_TYPE 8

namespace spectacles {

namespace etf {

class Encoder;

class Data {
  friend class Encoder;

 private:
  int16_t type = UNDEFINED_TYPE;
  bool boolean;
  int32_t sint;
  uint32_t uint;
  double df;

  std::string str;

  std::vector<Data> array;
  std::map<Data, Data> map;

 public:
  Data() { }
  Data(int16_t type_) : type(type_) { }
  Data(std::nullptr_t nullp) : type(NULL_TYPE) { }
  Data(bool boolean_) : boolean(boolean_), type(BOOL_TYPE) { }
  Data(int32_t sint_) : sint(sint_), type(SINT_TYPE) { }
  Data(uint32_t uint_) : uint(uint_), type(UINT_TYPE) { }
  Data(double df_) : df(df_), type(DOUBLE_TYPE) { }
  Data(std::string str_) : str(str_), type(STRING_TYPE) { }
  Data(const char *c_str_) : str(std::string(c_str_)), type(STRING_TYPE) { }
  Data(const char *c_str_, uint32_t length_) : str(std::string(c_str_, length_)), type(STRING_TYPE) { }
  Data(std::vector<Data> array_) : array(array_), type(ARRAY_TYPE) { }
  Data(std::map<Data, Data> map_) : map(map_), type(MAP_TYPE) { }

  static Data Undefined() {
    return Data(UNDEFINED_TYPE);
  }

  static Data Null() {
    return Data(NULL_TYPE);
  }

  static Data True() {
    return Data(true);
  }

  static Data False() {
    return Data(false);
  }

  static Data Array(size_t length) {
    return Data(std::vector<Data>(length));
  }

  static Data Object() {
    return Data(std::map<Data, Data>());
  }

  size_t size() {
    if (type == STRING_TYPE) {
      return str.size();
    } else if (type == ARRAY_TYPE) {
      return array.size();
    } else if (type == MAP_TYPE) {
      return map.size();
    }

    return 0;
  }

  bool isUndefined() {
    return type == UNDEFINED_TYPE;
  }

  bool isNull() {
    return type == NULL_TYPE;
  }

  bool isBoolean() {
    return type == BOOL_TYPE;
  }

  bool isTrue() {
    return type == BOOL_TYPE && boolean == true;
  }

  bool isFalse() {
    return type == BOOL_TYPE && boolean == false;
  }

  bool isInt32() {
    return type == SINT_TYPE;
  }

  bool isUint32() {
    return type == UINT_TYPE;
  }

  bool isDouble() {
    return type == DOUBLE_TYPE;
  }

  bool isString() {
    return type == STRING_TYPE;
  }

  bool isArray() {
    return type == ARRAY_TYPE;
  }

  bool isMap() {
    return type == MAP_TYPE;
  }

  bool operator<(const Data &d) const {
    if (type != d.type) {
      return type < d.type;
    } else if (type == UNDEFINED_TYPE || type == NULL_TYPE) {
      return false;
    } else if (type == BOOL_TYPE) {
      return boolean < d.boolean;
    } else if (type == SINT_TYPE) {
      return sint < d.sint;
    } else if (type == UINT_TYPE) {
      return uint < d.uint;
    } else if (type == STRING_TYPE) {
      return str < d.str;
    } else if (type == ARRAY_TYPE) {
      return array < d.array;
    } else if (type == MAP_TYPE) {
      return map < d.map;
    }

    return false;
  }

  operator std::nullptr_t() {
    return nullptr;
  }

  operator int() {
    return sint;
  }

  operator unsigned int() {
    return uint;
  }

  operator double() {
    return df;
  }

  operator bool() {
    return boolean;
  }

  operator const char *() {
    return str.c_str();
  }

  operator std::string() {
    return str;
  }

  Data &operator[](int i) {
    return array[i];
  }

  Data &operator[](size_t i) {
    return array[i];
  }

  Data &operator[](const char *key) {
    return map[Data(key)];
  }
};

}  // namespace etf

}  // namespace spectacles

#endif  // SPECTACLES_INCLUDE_ETF_DATA_H_
