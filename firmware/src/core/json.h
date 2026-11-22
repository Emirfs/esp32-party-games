#pragma once

// Minimal JSON for the protocol: a writer that builds compact text into a
// std::string and a reader that parses one message into a small node table.
// Portable C++17 (no ESP-IDF), so core/ and games/ run in the native tests.

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace json {

class Writer {
 public:
  explicit Writer(std::string& out) : out_(out) {}

  Writer& beginObject();
  Writer& endObject();
  Writer& beginArray();
  Writer& endArray();
  // Object member name; the next call writes its value.
  Writer& key(const char* name);

  Writer& value(const char* s);  // nullptr writes null
  Writer& value(const std::string& s) { return value(s.c_str()); }
  Writer& value(bool b);
  Writer& value(int v);
  Writer& value(unsigned v);
  // long/unsigned long too: uint32_t is unsigned long on Xtensa, int64_t is
  // long on 64-bit Linux; without these such values are ambiguous.
  Writer& value(long v) { return value(static_cast<long long>(v)); }
  Writer& value(unsigned long v) { return value(static_cast<long long>(v)); }
  Writer& value(long long v);
  Writer& null();
  // Pre-encoded JSON (e.g. a vibration pattern array), written as is.
  Writer& raw(const char* json);

  // key(name).value(v)
  template <typename T>
  Writer& field(const char* name, T v) {
    key(name);
    return value(v);
  }

 private:
  void separate();
  void string(const char* s);

  std::string& out_;
  // One bit per nesting level: set while the container is still empty.
  uint64_t first_ = 0;
  int depth_ = 0;
  bool after_key_ = false;
};

enum class Type : uint8_t { Null, Bool, Number, String, Array, Object };

class Doc;

// Read-only handle into a parsed Doc. A missing member or index yields a Null
// value, so lookups chain without checks: msg["options"]["voteSec"].num(60).
class Value {
 public:
  Value() = default;

  Type type() const;
  bool isNull() const { return type() == Type::Null; }
  bool isBool() const { return type() == Type::Bool; }
  bool isNumber() const { return type() == Type::Number; }
  bool isString() const { return type() == Type::String; }
  bool isArray() const { return type() == Type::Array; }
  bool isObject() const { return type() == Type::Object; }

  // Typed accessors return the fallback when the type does not match.
  const char* str(const char* fallback = "") const;
  double num(double fallback = 0) const;
  // Integer if the value is an integral number in range, else the fallback.
  int integer(int fallback) const;
  bool boolean(bool fallback = false) const;

  size_t size() const;  // array elements / object members; 0 otherwise
  Value operator[](const char* key) const;
  // Any integer type (v[0], v[i], v[u32]); a template so literals are never
  // ambiguous with operator[](const char*).
  template <typename I, typename = std::enable_if_t<std::is_integral<I>::value>>
  Value operator[](I index) const {
    if constexpr (std::is_signed<I>::value)
      if (index < 0) return Value();
    return at(static_cast<size_t>(index));
  }
  Value at(size_t index) const;
  // Object member name at index (for iteration); "" otherwise.
  const char* keyAt(size_t index) const;

 private:
  friend class Doc;
  Value(const Doc* doc, int node) : doc_(doc), node_(node) {}
  const Doc* doc_ = nullptr;
  int node_ = -1;
};

class Doc {
 public:
  // Parses one complete JSON text; false on any syntax error or if nesting is
  // deeper than kMaxDepth. Strings are unescaped (\uXXXX -> UTF-8).
  bool parse(const char* text, size_t len);
  Value root() const { return nodes_.empty() ? Value() : Value(this, 0); }

  static constexpr int kMaxDepth = 16;

 private:
  friend class Value;
  struct Node {
    Type type;
    bool b;
    double n;
    uint32_t str;     // offset into strings_ (String value)
    uint32_t key;     // offset into strings_ of the member name; UINT32_MAX if none
    int32_t first;    // first child (Array/Object), -1 if empty
    int32_t next;     // next sibling, -1 if last
    uint32_t count;   // children
  };

  int parseValue(const char*& p, const char* end, int depth);
  bool parseString(const char*& p, const char* end, uint32_t& offset);

  std::vector<Node> nodes_;
  std::string strings_;  // NUL-separated
};

}  // namespace json
