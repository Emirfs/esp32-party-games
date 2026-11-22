#include "json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace json {

// ---------------------------------------------------------------- Writer

void Writer::separate() {
  if (after_key_) {
    after_key_ = false;
    return;
  }
  if (depth_ == 0) return;
  const uint64_t bit = 1ull << (depth_ - 1);
  if (first_ & bit) first_ &= ~bit;
  else out_ += ',';
}

Writer& Writer::beginObject() {
  separate();
  out_ += '{';
  ++depth_;
  first_ |= 1ull << (depth_ - 1);
  return *this;
}

Writer& Writer::endObject() {
  --depth_;
  out_ += '}';
  return *this;
}

Writer& Writer::beginArray() {
  separate();
  out_ += '[';
  ++depth_;
  first_ |= 1ull << (depth_ - 1);
  return *this;
}

Writer& Writer::endArray() {
  --depth_;
  out_ += ']';
  return *this;
}

Writer& Writer::key(const char* name) {
  separate();
  string(name);
  out_ += ':';
  after_key_ = true;
  return *this;
}

void Writer::string(const char* s) {
  out_ += '"';
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
    switch (*p) {
      case '"': out_ += "\\\""; break;
      case '\\': out_ += "\\\\"; break;
      case '\n': out_ += "\\n"; break;
      case '\r': out_ += "\\r"; break;
      case '\t': out_ += "\\t"; break;
      default:
        if (*p < 0x20) {
          char esc[8];
          snprintf(esc, sizeof esc, "\\u%04x", *p);
          out_ += esc;
        } else {
          out_ += static_cast<char>(*p);
        }
    }
  }
  out_ += '"';
}

Writer& Writer::value(const char* s) {
  if (s == nullptr) return null();
  separate();
  string(s);
  return *this;
}

Writer& Writer::value(bool b) {
  separate();
  out_ += b ? "true" : "false";
  return *this;
}

Writer& Writer::value(int v) { return value(static_cast<long long>(v)); }
Writer& Writer::value(unsigned v) { return value(static_cast<long long>(v)); }

Writer& Writer::value(long long v) {
  separate();
  char buf[24];
  snprintf(buf, sizeof buf, "%lld", v);
  out_ += buf;
  return *this;
}

Writer& Writer::null() {
  separate();
  out_ += "null";
  return *this;
}

Writer& Writer::raw(const char* text) {
  separate();
  out_ += text;
  return *this;
}

// ---------------------------------------------------------------- Value

namespace {
constexpr uint32_t kNoKey = UINT32_MAX;
}

Type Value::type() const { return doc_ ? doc_->nodes_[node_].type : Type::Null; }

const char* Value::str(const char* fallback) const {
  if (type() != Type::String) return fallback;
  return doc_->strings_.c_str() + doc_->nodes_[node_].str;
}

double Value::num(double fallback) const {
  return type() == Type::Number ? doc_->nodes_[node_].n : fallback;
}

int Value::integer(int fallback) const {
  if (type() != Type::Number) return fallback;
  const double n = doc_->nodes_[node_].n;
  if (n != std::floor(n) || n < -2147483648.0 || n > 2147483647.0) return fallback;
  return static_cast<int>(n);
}

bool Value::boolean(bool fallback) const {
  return type() == Type::Bool ? doc_->nodes_[node_].b : fallback;
}

size_t Value::size() const {
  const Type t = type();
  return t == Type::Array || t == Type::Object ? doc_->nodes_[node_].count : 0;
}

Value Value::operator[](const char* key) const {
  if (type() != Type::Object) return Value();
  for (int i = doc_->nodes_[node_].first; i >= 0; i = doc_->nodes_[i].next)
    if (strcmp(doc_->strings_.c_str() + doc_->nodes_[i].key, key) == 0) return Value(doc_, i);
  return Value();
}

Value Value::at(size_t index) const {
  const Type t = type();
  if (t != Type::Array && t != Type::Object) return Value();
  int i = doc_->nodes_[node_].first;
  for (size_t k = 0; i >= 0 && k < index; ++k) i = doc_->nodes_[i].next;
  return i >= 0 ? Value(doc_, i) : Value();
}

const char* Value::keyAt(size_t index) const {
  if (type() != Type::Object) return "";
  const Value v = at(index);
  return v.doc_ ? doc_->strings_.c_str() + doc_->nodes_[v.node_].key : "";
}

// ---------------------------------------------------------------- Doc

namespace {

void skipWs(const char*& p, const char* end) {
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
}

bool literal(const char*& p, const char* end, const char* word) {
  const size_t n = strlen(word);
  if (static_cast<size_t>(end - p) < n || memcmp(p, word, n) != 0) return false;
  p += n;
  return true;
}

int hex4(const char* p) {
  int v = 0;
  for (int i = 0; i < 4; ++i) {
    const char c = p[i];
    v <<= 4;
    if (c >= '0' && c <= '9') v |= c - '0';
    else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
    else return -1;
  }
  return v;
}

void utf8(std::string& out, uint32_t cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | cp >> 6);
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | cp >> 12);
    out += static_cast<char>(0x80 | (cp >> 6 & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | cp >> 18);
    out += static_cast<char>(0x80 | (cp >> 12 & 0x3F));
    out += static_cast<char>(0x80 | (cp >> 6 & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

}  // namespace

bool Doc::parse(const char* text, size_t len) {
  nodes_.clear();
  strings_.clear();
  const char* p = text;
  const char* end = text + len;
  if (parseValue(p, end, 0) != 0) {
    nodes_.clear();
    return false;
  }
  skipWs(p, end);
  if (p != end) {
    nodes_.clear();
    return false;
  }
  return true;
}

bool Doc::parseString(const char*& p, const char* end, uint32_t& offset) {
  if (p >= end || *p != '"') return false;
  ++p;
  offset = static_cast<uint32_t>(strings_.size());
  while (p < end && *p != '"') {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c < 0x20) return false;
    if (c != '\\') {
      strings_ += static_cast<char>(c);
      ++p;
      continue;
    }
    if (++p >= end) return false;
    switch (*p) {
      case '"': strings_ += '"'; break;
      case '\\': strings_ += '\\'; break;
      case '/': strings_ += '/'; break;
      case 'b': strings_ += '\b'; break;
      case 'f': strings_ += '\f'; break;
      case 'n': strings_ += '\n'; break;
      case 'r': strings_ += '\r'; break;
      case 't': strings_ += '\t'; break;
      case 'u': {
        if (end - p < 5) return false;
        int cp = hex4(p + 1);
        if (cp < 0) return false;
        p += 4;
        if (cp >= 0xD800 && cp <= 0xDBFF) {  // surrogate pair
          if (end - p < 7 || p[1] != '\\' || p[2] != 'u') return false;
          const int lo = hex4(p + 3);
          if (lo < 0xDC00 || lo > 0xDFFF) return false;
          cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          p += 6;
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
          return false;
        }
        if (cp == 0) return false;  // NUL would truncate the C string
        utf8(strings_, static_cast<uint32_t>(cp));
        break;
      }
      default: return false;
    }
    ++p;
  }
  if (p >= end) return false;
  ++p;
  strings_ += '\0';
  return true;
}

// Appends one value; returns its node index or -1.
int Doc::parseValue(const char*& p, const char* end, int depth) {
  if (depth > kMaxDepth) return -1;
  skipWs(p, end);
  if (p >= end) return -1;

  const int self = static_cast<int>(nodes_.size());
  nodes_.push_back(Node{Type::Null, false, 0, 0, kNoKey, -1, -1, 0});

  const char c = *p;
  if (c == '{' || c == '[') {
    const bool object = c == '{';
    nodes_[self].type = object ? Type::Object : Type::Array;
    ++p;
    skipWs(p, end);
    if (p < end && *p == (object ? '}' : ']')) {
      ++p;
      return self;
    }
    int prev = -1;
    for (;;) {
      uint32_t key = kNoKey;
      if (object) {
        skipWs(p, end);
        if (!parseString(p, end, key)) return -1;
        skipWs(p, end);
        if (p >= end || *p != ':') return -1;
        ++p;
      }
      const int child = parseValue(p, end, depth + 1);
      if (child < 0) return -1;
      nodes_[child].key = key;
      if (prev < 0) nodes_[self].first = child;
      else nodes_[prev].next = child;
      prev = child;
      ++nodes_[self].count;
      skipWs(p, end);
      if (p >= end) return -1;
      if (*p == ',') {
        ++p;
        continue;
      }
      if (*p != (object ? '}' : ']')) return -1;
      ++p;
      return self;
    }
  }
  if (c == '"') {
    uint32_t off;
    if (!parseString(p, end, off)) return -1;
    nodes_[self].type = Type::String;
    nodes_[self].str = off;
    return self;
  }
  if (literal(p, end, "true") || literal(p, end, "false")) {
    nodes_[self].type = Type::Bool;
    nodes_[self].b = p[-1] == 'e' && p[-2] == 'u';
    return self;
  }
  if (literal(p, end, "null")) return self;

  // Number: validate the JSON grammar, then convert.
  const char* start = p;
  if (p < end && *p == '-') ++p;
  if (p >= end || *p < '0' || *p > '9') return -1;
  if (*p == '0') ++p;
  else
    while (p < end && *p >= '0' && *p <= '9') ++p;
  if (p < end && *p == '.') {
    ++p;
    if (p >= end || *p < '0' || *p > '9') return -1;
    while (p < end && *p >= '0' && *p <= '9') ++p;
  }
  if (p < end && (*p == 'e' || *p == 'E')) {
    ++p;
    if (p < end && (*p == '+' || *p == '-')) ++p;
    if (p >= end || *p < '0' || *p > '9') return -1;
    while (p < end && *p >= '0' && *p <= '9') ++p;
  }
  char buf[40];
  const size_t n = static_cast<size_t>(p - start);
  if (n >= sizeof buf) return -1;
  memcpy(buf, start, n);
  buf[n] = '\0';
  nodes_[self].type = Type::Number;
  nodes_[self].n = strtod(buf, nullptr);
  return self;
}

}  // namespace json
