/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#include "RunContract.h"
#include "astra-sim/workload/WorkloadCollectiveDecoder.hh"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace SimAIContract {
namespace {

const char* kRunSchemaVersion = "simai.run/v1";
const char* kResultSchemaVersion = "simai.result/v1";
const size_t kMaximumRoutingArtifactBytes = 1024U * 1024U;
const int kMaximumDenseRoutingRanks = 256;
const size_t kMaximumDenseRoutingCells = 256U * 256U;
const size_t kMaximumTargetModelArtifactBytes = 256U * 1024U;
const size_t kMaximumTargetStepArtifactBytes = 64U * 1024U;
const size_t kMaximumTargetRoutingArtifactBytes = 64U * 1024U;
const size_t kMaximumTargetMemoryArtifactBytes = 128U * 1024U;

uint32_t RotateRight(uint32_t value, uint32_t count) {
  return (value >> count) | (value << (32U - count));
}

std::string Sha256Hex(const std::string& input) {
  static const std::array<uint32_t, 64> constants = {{
      0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
      0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
      0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
      0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
      0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
      0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
      0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
      0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
      0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
      0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
      0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
      0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
      0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
      0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
      0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
      0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U}};

  std::array<uint32_t, 8> hash = {{
      0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
      0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U}};
  std::vector<uint8_t> bytes(input.begin(), input.end());
  const uint64_t bit_length = static_cast<uint64_t>(bytes.size()) * 8U;
  bytes.push_back(0x80U);
  while ((bytes.size() % 64U) != 56U) {
    bytes.push_back(0U);
  }
  for (int shift = 56; shift >= 0; shift -= 8) {
    bytes.push_back(static_cast<uint8_t>((bit_length >> shift) & 0xffU));
  }

  for (size_t chunk = 0; chunk < bytes.size(); chunk += 64U) {
    std::array<uint32_t, 64> words = {{0U}};
    for (size_t index = 0; index < 16U; ++index) {
      const size_t offset = chunk + index * 4U;
      words[index] = (static_cast<uint32_t>(bytes[offset]) << 24U) |
                     (static_cast<uint32_t>(bytes[offset + 1U]) << 16U) |
                     (static_cast<uint32_t>(bytes[offset + 2U]) << 8U) |
                     static_cast<uint32_t>(bytes[offset + 3U]);
    }
    for (size_t index = 16U; index < 64U; ++index) {
      const uint32_t before15 = words[index - 15U];
      const uint32_t before2 = words[index - 2U];
      const uint32_t sigma0 = RotateRight(before15, 7U) ^
                              RotateRight(before15, 18U) ^ (before15 >> 3U);
      const uint32_t sigma1 = RotateRight(before2, 17U) ^
                              RotateRight(before2, 19U) ^ (before2 >> 10U);
      words[index] = words[index - 16U] + sigma0 + words[index - 7U] + sigma1;
    }

    uint32_t a = hash[0];
    uint32_t b = hash[1];
    uint32_t c = hash[2];
    uint32_t d = hash[3];
    uint32_t e = hash[4];
    uint32_t f = hash[5];
    uint32_t g = hash[6];
    uint32_t h = hash[7];
    for (size_t index = 0; index < 64U; ++index) {
      const uint32_t sum1 = RotateRight(e, 6U) ^ RotateRight(e, 11U) ^
                            RotateRight(e, 25U);
      const uint32_t choice = (e & f) ^ ((~e) & g);
      const uint32_t temp1 =
          h + sum1 + choice + constants[index] + words[index];
      const uint32_t sum0 = RotateRight(a, 2U) ^ RotateRight(a, 13U) ^
                            RotateRight(a, 22U);
      const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t temp2 = sum0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }
    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
    hash[5] += f;
    hash[6] += g;
    hash[7] += h;
  }

  std::ostringstream digest;
  digest << std::hex << std::setfill('0');
  for (const uint32_t word : hash) {
    digest << std::setw(8) << word;
  }
  return digest.str();
}

bool ReadFile(const std::string& path, std::string* content) {
  std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
  if (!input) {
    return false;
  }
  content->assign(
      std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
  return input.good() || input.eof();
}

enum class BoundedReadResult {
  Success,
  Unreadable,
  TooLarge,
};

BoundedReadResult ReadFileWithMaximumBytes(
    const std::string& path,
    size_t maximum_bytes,
    std::string* content) {
  std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
  if (!input) {
    return BoundedReadResult::Unreadable;
  }
  content->clear();
  std::array<char, 8192> buffer = {{'\0'}};
  while (input) {
    const size_t remaining = maximum_bytes - content->size();
    const size_t requested = std::min(buffer.size(), remaining + 1U);
    input.read(buffer.data(), static_cast<std::streamsize>(requested));
    const std::streamsize bytes_read = input.gcount();
    if (bytes_read > 0) {
      content->append(buffer.data(), static_cast<size_t>(bytes_read));
    }
    if (content->size() > maximum_bytes) {
      return BoundedReadResult::TooLarge;
    }
  }
  return input.eof() ? BoundedReadResult::Success
                     : BoundedReadResult::Unreadable;
}

std::string FileSha256(const std::string& path) {
  std::string content;
  if (!ReadFile(path, &content)) {
    return "UNKNOWN";
  }
  return "sha256:" + Sha256Hex(content);
}

std::string CanonicalPath(const std::string& path) {
  if (path.empty()) {
    return "";
  }
  std::array<char, PATH_MAX> resolved = {{'\0'}};
  return realpath(path.c_str(), resolved.data()) == nullptr
      ? ""
      : std::string(resolved.data());
}

std::string ResolveExecutableFromArgument(const std::string& argument) {
  if (argument.empty()) {
    return "";
  }
  if (argument.find('/') != std::string::npos) {
    return CanonicalPath(argument);
  }
  const char* path_environment = std::getenv("PATH");
  if (path_environment == nullptr) {
    return "";
  }
  std::istringstream paths(path_environment);
  std::string directory;
  while (std::getline(paths, directory, ':')) {
    const std::string candidate =
        (directory.empty() ? "." : directory) + "/" + argument;
    if (access(candidate.c_str(), X_OK) == 0) {
      const std::string resolved = CanonicalPath(candidate);
      if (!resolved.empty()) {
        return resolved;
      }
    }
  }
  return "";
}

std::string ResolveCurrentExecutable(const std::string& argument) {
#if defined(__APPLE__)
  std::vector<char> path(PATH_MAX);
  uint32_t size = static_cast<uint32_t>(path.size());
  if (_NSGetExecutablePath(path.data(), &size) != 0) {
    path.resize(size);
    if (_NSGetExecutablePath(path.data(), &size) != 0) {
      return ResolveExecutableFromArgument(argument);
    }
  }
  const std::string resolved = CanonicalPath(path.data());
  return resolved.empty() ? ResolveExecutableFromArgument(argument) : resolved;
#elif defined(__linux__)
  std::array<char, PATH_MAX> path = {{'\0'}};
  const ssize_t length = readlink("/proc/self/exe", path.data(), path.size() - 1U);
  if (length > 0) {
    path[static_cast<size_t>(length)] = '\0';
    const std::string resolved = CanonicalPath(path.data());
    if (!resolved.empty()) {
      return resolved;
    }
  }
  return ResolveExecutableFromArgument(argument);
#else
  return ResolveExecutableFromArgument(argument);
#endif
}

class JsonValue {
 public:
  enum class Type { Null, Boolean, Number, String, Object, Array };

  JsonValue() : type(Type::Null), boolean(false), number(0.0) {}

  Type type;
  bool boolean;
  double number;
  std::string number_lexeme;
  std::string string;
  std::map<std::string, JsonValue> object;
  std::vector<JsonValue> array;
};

class JsonParser {
 public:
  explicit JsonParser(const std::string& input) : input_(input), offset_(0U) {}

  JsonValue Parse() {
    JsonValue result = ParseValue();
    SkipWhitespace();
    if (offset_ != input_.size()) {
      throw std::runtime_error("trailing JSON content");
    }
    return result;
  }

 private:
  void SkipWhitespace() {
    while (offset_ < input_.size()) {
      const char current = input_[offset_];
      if (current != ' ' && current != '\n' && current != '\r' &&
          current != '\t') {
        break;
      }
      ++offset_;
    }
  }

  char Consume() {
    if (offset_ >= input_.size()) {
      throw std::runtime_error("unexpected end of JSON");
    }
    return input_[offset_++];
  }

  void Expect(char expected) {
    if (Consume() != expected) {
      throw std::runtime_error("unexpected JSON token");
    }
  }

  bool ConsumeLiteral(const char* literal) {
    const size_t start = offset_;
    while (*literal != '\0') {
      if (offset_ >= input_.size() || input_[offset_] != *literal) {
        offset_ = start;
        return false;
      }
      ++offset_;
      ++literal;
    }
    return true;
  }

  JsonValue ParseValue() {
    SkipWhitespace();
    if (offset_ >= input_.size()) {
      throw std::runtime_error("missing JSON value");
    }
    const char current = input_[offset_];
    if (current == '{') {
      return ParseObject();
    }
    if (current == '[') {
      return ParseArray();
    }
    if (current == '"') {
      JsonValue value;
      value.type = JsonValue::Type::String;
      value.string = ParseString();
      return value;
    }
    if (current == '-' || (current >= '0' && current <= '9')) {
      return ParseNumber();
    }
    JsonValue value;
    if (ConsumeLiteral("true")) {
      value.type = JsonValue::Type::Boolean;
      value.boolean = true;
      return value;
    }
    if (ConsumeLiteral("false")) {
      value.type = JsonValue::Type::Boolean;
      value.boolean = false;
      return value;
    }
    if (ConsumeLiteral("null")) {
      return value;
    }
    throw std::runtime_error("invalid JSON value");
  }

  JsonValue ParseObject() {
    JsonValue value;
    value.type = JsonValue::Type::Object;
    Expect('{');
    SkipWhitespace();
    if (offset_ < input_.size() && input_[offset_] == '}') {
      ++offset_;
      return value;
    }
    while (true) {
      SkipWhitespace();
      if (offset_ >= input_.size() || input_[offset_] != '"') {
        throw std::runtime_error("object key must be a string");
      }
      const std::string key = ParseString();
      SkipWhitespace();
      Expect(':');
      if (value.object.count(key) != 0U) {
        throw std::runtime_error("duplicate JSON object key");
      }
      value.object[key] = ParseValue();
      SkipWhitespace();
      const char separator = Consume();
      if (separator == '}') {
        break;
      }
      if (separator != ',') {
        throw std::runtime_error("invalid JSON object separator");
      }
    }
    return value;
  }

  JsonValue ParseArray() {
    JsonValue value;
    value.type = JsonValue::Type::Array;
    Expect('[');
    SkipWhitespace();
    if (offset_ < input_.size() && input_[offset_] == ']') {
      ++offset_;
      return value;
    }
    while (true) {
      value.array.push_back(ParseValue());
      SkipWhitespace();
      const char separator = Consume();
      if (separator == ']') {
        break;
      }
      if (separator != ',') {
        throw std::runtime_error("invalid JSON array separator");
      }
    }
    return value;
  }

  std::string ParseString() {
    Expect('"');
    std::string result;
    while (true) {
      const char current = Consume();
      if (current == '"') {
        return result;
      }
      if (static_cast<unsigned char>(current) < 0x20U) {
        throw std::runtime_error("control character in JSON string");
      }
      if (current != '\\') {
        result.push_back(current);
        continue;
      }
      const char escaped = Consume();
      switch (escaped) {
        case '"':
        case '\\':
        case '/':
          result.push_back(escaped);
          break;
        case 'b':
          result.push_back('\b');
          break;
        case 'f':
          result.push_back('\f');
          break;
        case 'n':
          result.push_back('\n');
          break;
        case 'r':
          result.push_back('\r');
          break;
        case 't':
          result.push_back('\t');
          break;
        default:
          throw std::runtime_error("unsupported JSON escape");
      }
    }
  }

  JsonValue ParseNumber() {
    const size_t start = offset_;
    if (input_[offset_] == '-') {
      ++offset_;
    }
    if (offset_ >= input_.size()) {
      throw std::runtime_error("invalid JSON number");
    }
    if (input_[offset_] == '0') {
      ++offset_;
    } else {
      if (input_[offset_] < '1' || input_[offset_] > '9') {
        throw std::runtime_error("invalid JSON number");
      }
      while (offset_ < input_.size() && input_[offset_] >= '0' &&
             input_[offset_] <= '9') {
        ++offset_;
      }
    }
    if (offset_ < input_.size() && input_[offset_] == '.') {
      ++offset_;
      const size_t fractional_start = offset_;
      while (offset_ < input_.size() && input_[offset_] >= '0' &&
             input_[offset_] <= '9') {
        ++offset_;
      }
      if (fractional_start == offset_) {
        throw std::runtime_error("invalid JSON fraction");
      }
    }
    if (offset_ < input_.size() &&
        (input_[offset_] == 'e' || input_[offset_] == 'E')) {
      ++offset_;
      if (offset_ < input_.size() &&
          (input_[offset_] == '+' || input_[offset_] == '-')) {
        ++offset_;
      }
      const size_t exponent_start = offset_;
      while (offset_ < input_.size() && input_[offset_] >= '0' &&
             input_[offset_] <= '9') {
        ++offset_;
      }
      if (exponent_start == offset_) {
        throw std::runtime_error("invalid JSON exponent");
      }
    }
    const std::string encoded = input_.substr(start, offset_ - start);
    char* end = nullptr;
    const double parsed = std::strtod(encoded.c_str(), &end);
    if (end == nullptr || *end != '\0' || !std::isfinite(parsed)) {
      throw std::runtime_error("non-finite JSON number");
    }
    JsonValue value;
    value.type = JsonValue::Type::Number;
    value.number = parsed;
    value.number_lexeme = encoded;
    return value;
  }

  const std::string& input_;
  size_t offset_;
};

const JsonValue* Member(const JsonValue& object, const std::string& key) {
  if (object.type != JsonValue::Type::Object) {
    return nullptr;
  }
  const auto found = object.object.find(key);
  return found == object.object.end() ? nullptr : &found->second;
}

bool ObjectHasExactKeys(
    const JsonValue& object,
    const std::vector<std::string>& expected_keys) {
  if (object.type != JsonValue::Type::Object ||
      object.object.size() != expected_keys.size()) {
    return false;
  }
  for (const std::string& key : expected_keys) {
    if (object.object.count(key) != 1U) {
      return false;
    }
  }
  return true;
}

bool StringMember(
    const JsonValue& object,
    const std::string& key,
    std::string* value) {
  const JsonValue* member = Member(object, key);
  if (member == nullptr || member->type != JsonValue::Type::String) {
    return false;
  }
  *value = member->string;
  return true;
}

bool NumberMember(
    const JsonValue& object,
    const std::string& key,
    double* value) {
  const JsonValue* member = Member(object, key);
  if (member == nullptr || member->type != JsonValue::Type::Number) {
    return false;
  }
  *value = member->number;
  return true;
}

bool ExactUnsignedDecimal(
    const JsonValue& value,
    uint64_t maximum,
    bool allow_zero,
    uint64_t* parsed) {
  if (value.type != JsonValue::Type::Number || value.number_lexeme.empty() ||
      (value.number_lexeme.size() > 1U && value.number_lexeme[0] == '0') ||
      value.number_lexeme[0] == '-') {
    return false;
  }
  uint64_t accumulated = 0U;
  for (const char digit : value.number_lexeme) {
    if (digit < '0' || digit > '9') {
      return false;
    }
    const uint64_t numeric_digit = static_cast<uint64_t>(digit - '0');
    if (accumulated > (maximum - numeric_digit) / 10U) {
      return false;
    }
    accumulated = accumulated * 10U + numeric_digit;
  }
  if (!allow_zero && accumulated == 0U) {
    return false;
  }
  *parsed = accumulated;
  return true;
}

bool ExactPositiveUint64Member(
    const JsonValue& object,
    const std::string& key,
    uint64_t* value) {
  const JsonValue* member = Member(object, key);
  const uint64_t maximum_exact_json_integer = 9007199254740991ULL;
  return member != nullptr &&
      ExactUnsignedDecimal(
          *member, maximum_exact_json_integer, false, value);
}

bool ExactNonNegativeUint64Member(
    const JsonValue& object,
    const std::string& key,
    uint64_t* value) {
  const JsonValue* member = Member(object, key);
  const uint64_t maximum_exact_json_integer = 9007199254740991ULL;
  return member != nullptr &&
      ExactUnsignedDecimal(
          *member, maximum_exact_json_integer, true, value);
}

bool ExactPositiveIntMember(
    const JsonValue& object,
    const std::string& key,
    int* value) {
  const JsonValue* member = Member(object, key);
  uint64_t parsed = 0U;
  if (member == nullptr ||
      !ExactUnsignedDecimal(
          *member,
          static_cast<uint64_t>(std::numeric_limits<int>::max()),
          false,
          &parsed)) {
    return false;
  }
  *value = static_cast<int>(parsed);
  return true;
}

bool BooleanMember(
    const JsonValue& object,
    const std::string& key,
    bool* value) {
  const JsonValue* member = Member(object, key);
  if (member == nullptr || member->type != JsonValue::Type::Boolean) {
    return false;
  }
  *value = member->boolean;
  return true;
}

bool PositiveUint64Member(
    const JsonValue& object,
    const std::string& key,
    uint64_t* value) {
  double number = 0.0;
  const double maximum_exact_json_integer = 9007199254740991.0;
  if (!NumberMember(object, key, &number) || number < 1.0 ||
      number > maximum_exact_json_integer ||
      std::floor(number) != number) {
    return false;
  }
  *value = static_cast<uint64_t>(number);
  return true;
}

const JsonValue* FirstArrayObject(
    const JsonValue& object,
    const std::string& key) {
  const JsonValue* array = Member(object, key);
  if (array == nullptr || array->type != JsonValue::Type::Array ||
      array->array.empty() ||
      array->array.front().type != JsonValue::Type::Object) {
    return nullptr;
  }
  return &array->array.front();
}

bool FirstArrayString(
    const JsonValue& object,
    const std::string& key,
    std::string* value) {
  const JsonValue* array = Member(object, key);
  if (array == nullptr || array->type != JsonValue::Type::Array ||
      array->array.size() != 1U ||
      array->array.front().type != JsonValue::Type::String) {
    return false;
  }
  *value = array->array.front().string;
  return true;
}

bool FirstArrayPositiveInt(
    const JsonValue& object,
    const std::string& key,
    int* value) {
  const JsonValue* array = Member(object, key);
  if (array == nullptr || array->type != JsonValue::Type::Array ||
      array->array.size() != 1U ||
      array->array.front().type != JsonValue::Type::Number ||
      array->array.front().number < 1.0 ||
      array->array.front().number >
          static_cast<double>(std::numeric_limits<int>::max()) ||
      std::floor(array->array.front().number) != array->array.front().number) {
    return false;
  }
  *value = static_cast<int>(array->array.front().number);
  return true;
}

bool IsSha256Identifier(const std::string& digest);

bool ParseArtifactReference(
    const JsonValue& reference,
    std::string* path,
    std::string* digest) {
  return reference.type == JsonValue::Type::Object &&
      StringMember(reference, "path", path) && !path->empty() &&
      StringMember(reference, "sha256", digest) &&
      IsSha256Identifier(*digest);
}

bool ParseJsonDocument(const std::string& content, JsonValue* root) {
  try {
    *root = JsonParser(content).Parse();
  } catch (const std::exception&) {
    return false;
  }
  return root->type == JsonValue::Type::Object;
}

bool UnitsAreCanonical(const JsonValue& value) {
  if (value.type == JsonValue::Type::Object) {
    for (const auto& entry : value.object) {
      if (entry.first == "unit") {
        if (entry.second.type != JsonValue::Type::String) {
          return false;
        }
        const std::string& unit = entry.second.string;
        if (unit != "count" && unit != "B" && unit != "B/s" &&
            unit != "FLOP/s" && unit != "ns") {
          return false;
        }
      }
      if (!UnitsAreCanonical(entry.second)) {
        return false;
      }
    }
  } else if (value.type == JsonValue::Type::Array) {
    for (const JsonValue& element : value.array) {
      if (!UnitsAreCanonical(element)) {
        return false;
      }
    }
  }
  return true;
}

bool ContainsString(const JsonValue& value, const std::string& expected) {
  if (value.type == JsonValue::Type::String) {
    return value.string == expected;
  }
  if (value.type == JsonValue::Type::Object) {
    for (const auto& entry : value.object) {
      if (ContainsString(entry.second, expected)) {
        return true;
      }
    }
  } else if (value.type == JsonValue::Type::Array) {
    for (const JsonValue& element : value.array) {
      if (ContainsString(element, expected)) {
        return true;
      }
    }
  }
  return false;
}

bool EvidenceRecordIsComplete(const JsonValue& evidence) {
  const JsonValue* source = Member(evidence, "source");
  const JsonValue* method = Member(evidence, "method");
  const JsonValue* conditions = Member(evidence, "conditions");
  std::string id;
  std::string evidence_class;
  std::string source_uri;
  std::string source_ref;
  std::string method_name;
  std::string method_version;
  std::string as_of;
  std::string sanitization;
  if (!StringMember(evidence, "id", &id) || id.empty() ||
      !StringMember(evidence, "class", &evidence_class) ||
      (evidence_class != "MEASURED" &&
       evidence_class != "VENDOR_SPEC" &&
       evidence_class != "USER_INPUT" &&
       evidence_class != "DERIVED" &&
       evidence_class != "EXTRAPOLATED" &&
       evidence_class != "LEGACY_ASSUMED") ||
      source == nullptr || !StringMember(*source, "uri", &source_uri) ||
      source_uri.empty() || !StringMember(*source, "ref", &source_ref) ||
      source_ref.empty() || method == nullptr ||
      !StringMember(*method, "name", &method_name) || method_name.empty() ||
      !StringMember(*method, "version", &method_version) ||
      method_version.empty() || !StringMember(evidence, "asOf", &as_of) ||
      as_of.empty() || conditions == nullptr ||
      conditions->type != JsonValue::Type::Object ||
      !StringMember(evidence, "sanitization", &sanitization) ||
      sanitization.empty()) {
    return false;
  }
  return true;
}

bool A2EvidenceRecordIsExact(
    const JsonValue& evidence,
    std::string* id,
    std::string* evidence_class,
    std::string* readiness,
    bool* hardware_available) {
  const JsonValue* source = Member(evidence, "source");
  const JsonValue* method = Member(evidence, "method");
  const JsonValue* conditions = Member(evidence, "conditions");
  if (!ObjectHasExactKeys(
          evidence,
          {"id", "class", "readiness", "source", "method", "asOf",
           "conditions", "sanitization"}) ||
      source == nullptr ||
      !ObjectHasExactKeys(*source, {"uri", "ref"}) ||
      method == nullptr ||
      !ObjectHasExactKeys(*method, {"name", "version"}) ||
      conditions == nullptr ||
      !ObjectHasExactKeys(*conditions, {"hardwareAvailable"}) ||
      !EvidenceRecordIsComplete(evidence) ||
      !StringMember(evidence, "id", id) || id->empty() ||
      !StringMember(evidence, "class", evidence_class) ||
      !StringMember(evidence, "readiness", readiness) ||
      (*readiness != "FIELD_UNVERIFIED" &&
       *readiness != "FIELD_VERIFIED") ||
      !BooleanMember(
          *conditions, "hardwareAvailable", hardware_available)) {
    return false;
  }
  return !(*readiness == "FIELD_UNVERIFIED" &&
           *evidence_class == "MEASURED");
}

bool ParseExactRankMembers(
    const JsonValue& members,
    int world_size,
    std::vector<int>* parsed) {
  if (members.type != JsonValue::Type::Array || members.array.empty()) {
    return false;
  }
  parsed->clear();
  int previous = -1;
  for (const JsonValue& member : members.array) {
    uint64_t rank = 0U;
    if (!ExactUnsignedDecimal(
            member,
            static_cast<uint64_t>(std::numeric_limits<int>::max()),
            true,
            &rank) ||
        rank >= static_cast<uint64_t>(world_size) ||
        static_cast<int>(rank) <= previous) {
      return false;
    }
    previous = static_cast<int>(rank);
    parsed->push_back(previous);
  }
  return true;
}

std::string A2MembershipDigest(
    const std::string& id,
    const std::string& group_type,
    const std::vector<int>& members,
    const std::string& topology_digest) {
  std::ostringstream canonical;
  canonical << id << "|" << group_type << "|";
  for (size_t index = 0U; index < members.size(); ++index) {
    if (index != 0U) {
      canonical << ",";
    }
    canonical << members[index];
  }
  canonical << "|" << topology_digest;
  return "sha256:" + Sha256Hex(canonical.str());
}

struct A2SemanticBindings {
  std::string profile_id;
  std::string profile_sha256;
  std::string profile_evidence_class;
  std::string profile_readiness;
  std::string profile_evidence_ref;
  std::string workload_sha256;
  int world_size = 0;
  std::string topology_sha256;
  std::string group_id;
  std::string group_type;
  int group_rank_count = 0;
  std::vector<int> group_members;
  std::string group_membership_sha256;
};

bool ParseA2SemanticBindings(
    const JsonValue& bindings,
    A2SemanticBindings* parsed) {
  const JsonValue* profile = Member(bindings, "profile");
  const JsonValue* workload = Member(bindings, "workload");
  const JsonValue* topology = Member(bindings, "topology");
  const JsonValue* group = Member(bindings, "hcclGroup");
  const JsonValue* members = group == nullptr ? nullptr : Member(*group, "members");
  if (!ObjectHasExactKeys(
          bindings, {"profile", "workload", "topology", "hcclGroup"}) ||
      profile == nullptr ||
      !ObjectHasExactKeys(
          *profile,
          {"id", "sha256", "evidenceClass", "readiness", "evidenceRef"}) ||
      !StringMember(*profile, "id", &parsed->profile_id) ||
      parsed->profile_id.empty() ||
      !StringMember(*profile, "sha256", &parsed->profile_sha256) ||
      !IsSha256Identifier(parsed->profile_sha256) ||
      !StringMember(
          *profile, "evidenceClass", &parsed->profile_evidence_class) ||
      (parsed->profile_evidence_class != "USER_INPUT" &&
       parsed->profile_evidence_class != "MEASURED") ||
      !StringMember(*profile, "readiness", &parsed->profile_readiness) ||
      (parsed->profile_readiness != "FIELD_UNVERIFIED" &&
       parsed->profile_readiness != "FIELD_VERIFIED") ||
      !StringMember(
          *profile, "evidenceRef", &parsed->profile_evidence_ref) ||
      parsed->profile_evidence_ref.empty() || workload == nullptr ||
      !ObjectHasExactKeys(*workload, {"sha256"}) ||
      !StringMember(*workload, "sha256", &parsed->workload_sha256) ||
      !IsSha256Identifier(parsed->workload_sha256) || topology == nullptr ||
      !ObjectHasExactKeys(
          *topology, {"worldSize", "rankMappingDigest"}) ||
      !ExactPositiveIntMember(*topology, "worldSize", &parsed->world_size) ||
      parsed->world_size != 8 ||
      !StringMember(
          *topology, "rankMappingDigest", &parsed->topology_sha256) ||
      !IsSha256Identifier(parsed->topology_sha256) || group == nullptr ||
      !ObjectHasExactKeys(
          *group,
          {"id", "groupType", "rankCount", "members",
           "membershipDigest", "topologyDigest"}) ||
      !StringMember(*group, "id", &parsed->group_id) ||
      parsed->group_id.empty() ||
      !StringMember(*group, "groupType", &parsed->group_type) ||
      (parsed->group_type != "TP" && parsed->group_type != "DP" &&
       parsed->group_type != "EP") ||
      !ExactPositiveIntMember(
          *group, "rankCount", &parsed->group_rank_count) ||
      members == nullptr ||
      !ParseExactRankMembers(
          *members, parsed->world_size, &parsed->group_members) ||
      parsed->group_members.size() !=
          static_cast<size_t>(parsed->group_rank_count) ||
      !StringMember(
          *group,
          "membershipDigest",
          &parsed->group_membership_sha256) ||
      !IsSha256Identifier(parsed->group_membership_sha256)) {
    return false;
  }
  std::string group_topology_digest;
  if (!StringMember(
          *group, "topologyDigest", &group_topology_digest) ||
      group_topology_digest != parsed->topology_sha256 ||
      parsed->group_membership_sha256 !=
          A2MembershipDigest(
              parsed->group_id,
              parsed->group_type,
              parsed->group_members,
              parsed->topology_sha256)) {
    return false;
  }
  const int expected_rank_count = parsed->group_type == "TP" ? 1 : 4;
  return parsed->group_rank_count == expected_rank_count;
}

bool A2BindingsEqual(
    const A2SemanticBindings& left,
    const A2SemanticBindings& right) {
  return left.profile_id == right.profile_id &&
      left.profile_sha256 == right.profile_sha256 &&
      left.profile_evidence_class == right.profile_evidence_class &&
      left.profile_readiness == right.profile_readiness &&
      left.profile_evidence_ref == right.profile_evidence_ref &&
      left.workload_sha256 == right.workload_sha256 &&
      left.world_size == right.world_size &&
      left.topology_sha256 == right.topology_sha256 &&
      left.group_id == right.group_id &&
      left.group_type == right.group_type &&
      left.group_rank_count == right.group_rank_count &&
      left.group_members == right.group_members &&
      left.group_membership_sha256 == right.group_membership_sha256;
}

struct ProfileEvidenceRecord {
  std::string evidence_class;
  std::string readiness;
  bool hardware_available = false;
};

bool BuildProfileEvidenceIndex(
    const JsonValue& spec,
    bool exact_v02,
    std::map<std::string, ProfileEvidenceRecord>* evidence_records) {
  const JsonValue* records = Member(spec, "evidence");
  if (records == nullptr || records->type != JsonValue::Type::Array ||
      records->array.empty()) {
    return false;
  }
  for (const JsonValue& record : records->array) {
    std::string id;
    std::string evidence_class;
    std::string readiness = "FIELD_UNVERIFIED";
    bool hardware_available = false;
    const JsonValue* conditions = Member(record, "conditions");
    const bool record_valid = exact_v02
        ? A2EvidenceRecordIsExact(
              record,
              &id,
              &evidence_class,
              &readiness,
              &hardware_available)
        : EvidenceRecordIsComplete(record) &&
            StringMember(record, "id", &id) &&
            StringMember(record, "class", &evidence_class) &&
            conditions != nullptr &&
            BooleanMember(
                *conditions, "hardwareAvailable", &hardware_available);
    if (!record_valid || id.empty() || evidence_records->count(id) != 0U) {
      return false;
    }
    ProfileEvidenceRecord parsed_record;
    parsed_record.evidence_class = evidence_class;
    parsed_record.readiness = readiness;
    parsed_record.hardware_available = hardware_available;
    (*evidence_records)[id] = parsed_record;
  }
  return true;
}

bool ConsumedEvidenceIsValid(
    const JsonValue& field,
    bool exact_v02,
    const std::map<std::string, ProfileEvidenceRecord>& evidence_records,
    bool* has_unverified_field,
    std::set<std::string>* referenced_evidence) {
  std::string evidence_ref;
  std::string readiness;
  if (!StringMember(field, "evidenceRef", &evidence_ref) ||
      !StringMember(field, "readiness", &readiness) ||
      (readiness != "FIELD_UNVERIFIED" && readiness != "FIELD_VERIFIED")) {
    return false;
  }
  const auto evidence = evidence_records.find(evidence_ref);
  if (evidence == evidence_records.end()) {
    return false;
  }
  if (exact_v02 &&
      (readiness != evidence->second.readiness ||
       (readiness == "FIELD_VERIFIED" &&
        (evidence->second.evidence_class != "MEASURED" ||
         !evidence->second.hardware_available)))) {
    return false;
  }
  if (!exact_v02 && readiness == "FIELD_UNVERIFIED" &&
      evidence->second.evidence_class == "MEASURED") {
    return false;
  }
  *has_unverified_field =
      *has_unverified_field || readiness == "FIELD_UNVERIFIED";
  referenced_evidence->insert(evidence_ref);
  return true;
}

bool ConsumedFieldEvidenceIsValid(
    const JsonValue& field,
    bool exact_v02,
    const std::map<std::string, ProfileEvidenceRecord>& evidence_records,
    bool* has_unverified_field,
    std::set<std::string>* referenced_evidence) {
  std::string status;
  if (!StringMember(field, "status", &status) || status != "KNOWN") {
    return false;
  }
  return ConsumedEvidenceIsValid(
      field,
      exact_v02,
      evidence_records,
      has_unverified_field,
      referenced_evidence);
}

bool PositiveIntMember(
    const JsonValue& object,
    const std::string& key,
    int* value) {
  double number = 0.0;
  if (!NumberMember(object, key, &number) || number < 1.0 ||
      number > static_cast<double>(std::numeric_limits<int>::max()) ||
      std::floor(number) != number) {
    return false;
  }
  *value = static_cast<int>(number);
  return true;
}

bool IsSafeRunId(const std::string& run_id) {
  if (run_id.empty() || run_id.size() > 64U) {
    return false;
  }
  for (const char character : run_id) {
    const bool valid =
        (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9') || character == '-' ||
        character == '_' || character == '.';
    if (!valid) {
      return false;
    }
  }
  return true;
}

bool ParseGpuType(const std::string& name, GPUType* gpu_type) {
  if (name == "A100") {
    *gpu_type = GPUType::A100;
  } else if (name == "A800") {
    *gpu_type = GPUType::A800;
  } else if (name == "H100") {
    *gpu_type = GPUType::H100;
  } else if (name == "H800") {
    *gpu_type = GPUType::H800;
  } else if (name == "H20") {
    *gpu_type = GPUType::H20;
  } else {
    *gpu_type = GPUType::NONE;
    return false;
  }
  return true;
}

bool IsSha256Identifier(const std::string& digest) {
  if (digest.size() != 71U || digest.substr(0U, 7U) != "sha256:") {
    return false;
  }
  for (size_t index = 7U; index < digest.size(); ++index) {
    const char character = digest[index];
    if (!((character >= '0' && character <= '9') ||
          (character >= 'a' && character <= 'f'))) {
      return false;
    }
  }
  return true;
}

void Reject(
    AnalyticalRunContract* contract,
    const std::string& reject_code,
    const std::string& message,
    const std::string& remediation) {
  contract->accepted = false;
  contract->exit_code = 2;
  contract->status = "INVALID_INPUT";
  contract->reject_code = reject_code;
  contract->message = message;
  contract->remediation = remediation;
  contract->a2_calibration_eligible = false;
}

void RejectUnsupported(
    AnalyticalRunContract* contract,
    const std::string& reject_code,
    const std::string& message,
    const std::string& remediation) {
  contract->accepted = false;
  contract->exit_code = 3;
  contract->status = "UNSUPPORTED";
  contract->reject_code = reject_code;
  contract->message = message;
  contract->remediation = remediation;
}

struct ArtifactLoadPolicy {
  const char* reference_code;
  const char* reference_message;
  const char* reference_remediation;
  const char* not_found_code;
  const char* not_found_message;
  const char* not_found_remediation;
  const char* digest_code;
  const char* digest_message;
  const char* digest_remediation;
  const char* json_code;
  const char* json_message;
  const char* json_remediation;
};

struct LoadedArtifact {
  JsonValue document;
  std::string sha256;
};

bool LoadArtifact(
    const JsonValue& reference,
    const ArtifactLoadPolicy& policy,
    AnalyticalRunContract* contract,
    LoadedArtifact* artifact,
    size_t maximum_bytes = 0U,
    const char* too_large_code = nullptr,
    const char* too_large_message = nullptr,
    const char* too_large_remediation = nullptr) {
  std::string path;
  std::string declared_digest;
  if (!ParseArtifactReference(reference, &path, &declared_digest)) {
    Reject(
        contract,
        policy.reference_code,
        policy.reference_message,
        policy.reference_remediation);
    return false;
  }
  std::string content;
  const BoundedReadResult read_result = maximum_bytes > 0U
      ? ReadFileWithMaximumBytes(path, maximum_bytes, &content)
      : (ReadFile(path, &content) ? BoundedReadResult::Success
                                  : BoundedReadResult::Unreadable);
  if (read_result == BoundedReadResult::TooLarge) {
    Reject(
        contract,
        too_large_code,
        too_large_message,
        too_large_remediation);
    return false;
  }
  if (read_result != BoundedReadResult::Success) {
    Reject(
        contract,
        policy.not_found_code,
        policy.not_found_message,
        policy.not_found_remediation);
    return false;
  }
  artifact->sha256 = "sha256:" + Sha256Hex(content);
  if (artifact->sha256 != declared_digest) {
    Reject(
        contract,
        policy.digest_code,
        policy.digest_message,
        policy.digest_remediation);
    return false;
  }
  if (!ParseJsonDocument(content, &artifact->document)) {
    Reject(
        contract,
        policy.json_code,
        policy.json_message,
        policy.json_remediation);
    return false;
  }
  return true;
}

bool ValidateTargetWorkloadEnvelope(
    const JsonValue& target_workload,
    AnalyticalRunContract* contract) {
  if (Member(target_workload, "routing") == nullptr) {
    contract->target_routing_ready = false;
    RejectUnsupported(
        contract,
        "TARGET_ROUTING_REQUIRED",
        "The Target Workload requires a separate Routing Artifact.",
        "Provide the immutable target_workload.routing reference.");
    return false;
  }
  std::string schema_version;
  std::string composition;
  std::string composite_digest;
  const std::vector<std::string> resource_names = {
      "model", "step", "routing", "memory_event_plan"};
  bool valid = ObjectHasExactKeys(
      target_workload,
      {"schema_version",
       "composition",
       "sha256",
       "model",
       "step",
       "routing",
       "memory_event_plan"}) &&
      StringMember(target_workload, "schema_version", &schema_version) &&
      schema_version == "simai.target.workload/v1" &&
      StringMember(target_workload, "composition", &composition) &&
      composition == "SHA256_NEWLINE_DELIMITED_RESOURCE_DIGESTS_V1" &&
      StringMember(target_workload, "sha256", &composite_digest) &&
      IsSha256Identifier(composite_digest);
  for (const std::string& resource_name : resource_names) {
    const JsonValue* reference = Member(target_workload, resource_name);
    std::string path;
    std::string digest;
    valid = valid && reference != nullptr &&
        ObjectHasExactKeys(*reference, {"path", "sha256"}) &&
        ParseArtifactReference(*reference, &path, &digest);
  }
  if (!valid) {
    Reject(
        contract,
        "TARGET_WORKLOAD_SCHEMA_INVALID",
        "The Target Workload composition envelope is invalid.",
        "Use the exact v1 envelope and immutable resource references.");
  }
  return valid;
}

struct ValidatedTargetModel {
  uint64_t logical_trainable_parameters = 0;
  uint64_t checkpoint_auxiliary_elements = 0;
  uint64_t checkpoint_quant_scale_elements = 0;
  uint64_t checkpoint_routing_table_elements = 0;
  uint64_t checkpoint_storage_bytes = 0;
  uint64_t active_main_blocks_parameters = 0;
  uint64_t active_main_forward_parameters = 0;
  uint64_t active_training_graph_parameters = 0;
  int routed_experts = 0;
  int top_k = 0;
  int expert_intermediate_size = 0;
  int shared_experts = 0;
  std::string evidence_level;
  std::string field_readiness;
};

bool TargetEvidenceHasExactShape(
    const JsonValue& spec,
    const std::string& condition_key) {
  const JsonValue* evidence = Member(spec, "evidence");
  if (evidence == nullptr || evidence->type != JsonValue::Type::Array ||
      evidence->array.empty()) {
    return false;
  }
  for (const JsonValue& record : evidence->array) {
    const JsonValue* source = Member(record, "source");
    const JsonValue* method = Member(record, "method");
    const JsonValue* conditions = Member(record, "conditions");
    const JsonValue* condition = conditions == nullptr
        ? nullptr
        : Member(*conditions, condition_key);
    if (!ObjectHasExactKeys(
            record,
            {"id",
             "class",
             "readiness",
             "source",
             "method",
             "asOf",
             "conditions",
             "sanitization"}) ||
        source == nullptr ||
        !ObjectHasExactKeys(*source, {"uri", "ref"}) || method == nullptr ||
        !ObjectHasExactKeys(*method, {"name", "version"}) ||
        conditions == nullptr ||
        !ObjectHasExactKeys(*conditions, {condition_key}) ||
        condition == nullptr || condition->type != JsonValue::Type::Boolean) {
      return false;
    }
  }
  return true;
}

bool ValidateTargetResourceEvidence(
    const JsonValue& spec,
    std::string* evidence_level,
    std::string* field_readiness) {
  std::string evidence_ref;
  const JsonValue* evidence = Member(spec, "evidence");
  if (!StringMember(spec, "evidenceRef", &evidence_ref) ||
      evidence_ref.empty() ||
      !StringMember(spec, "evidenceClass", evidence_level) ||
      !StringMember(spec, "readiness", field_readiness) ||
      (*field_readiness != "FIELD_UNVERIFIED" &&
       *field_readiness != "FIELD_VERIFIED") ||
      evidence == nullptr || evidence->type != JsonValue::Type::Array ||
      evidence->array.size() != 1U) {
    return false;
  }

  const JsonValue& record = evidence->array.front();
  std::string record_id;
  std::string record_class;
  std::string record_readiness;
  if (!EvidenceRecordIsComplete(record) ||
      !StringMember(record, "id", &record_id) ||
      record_id != evidence_ref ||
      !StringMember(record, "class", &record_class) ||
      record_class != *evidence_level ||
      !StringMember(record, "readiness", &record_readiness) ||
      (record_readiness != "FIELD_UNVERIFIED" &&
       record_readiness != "FIELD_VERIFIED") ||
      record_readiness != *field_readiness) {
    return false;
  }
  return !(*field_readiness == "FIELD_UNVERIFIED" &&
           *evidence_level == "MEASURED");
}

bool SafeMultiplyAdd(
    uint64_t left,
    uint64_t right,
    uint64_t* total) {
  if (left != 0U && right > std::numeric_limits<uint64_t>::max() / left) {
    return false;
  }
  const uint64_t product = left * right;
  if (product > std::numeric_limits<uint64_t>::max() - *total) {
    return false;
  }
  *total += product;
  return true;
}

bool SafeMultiply(uint64_t left, uint64_t right, uint64_t* product) {
  if (left != 0U && right > std::numeric_limits<uint64_t>::max() / left) {
    return false;
  }
  *product = left * right;
  return true;
}

bool PositiveUint64Value(const JsonValue& value, uint64_t* parsed_value) {
  const uint64_t maximum_exact_json_integer = 9007199254740991ULL;
  return ExactUnsignedDecimal(
      value, maximum_exact_json_integer, false, parsed_value);
}

bool PositiveShapeProduct(
    const JsonValue& shape,
    uint64_t* product,
    std::string* canonical = nullptr) {
  if (shape.type != JsonValue::Type::Array || shape.array.empty()) {
    return false;
  }
  uint64_t accumulated = 1U;
  std::ostringstream canonical_shape;
  for (size_t index = 0U; index < shape.array.size(); ++index) {
    const JsonValue& dimension = shape.array[index];
    uint64_t parsed_dimension = 0U;
    uint64_t next = 0U;
    if (!PositiveUint64Value(dimension, &parsed_dimension) ||
        !SafeMultiply(accumulated, parsed_dimension, &next)) {
      return false;
    }
    accumulated = next;
    if (index != 0U) {
      canonical_shape << ',';
    }
    canonical_shape << parsed_dimension;
  }
  *product = accumulated;
  if (canonical != nullptr) {
    *canonical = canonical_shape.str();
  }
  return true;
}

bool StringIsOneOf(
    const std::string& value,
    const std::vector<std::string>& allowed) {
  return std::find(allowed.begin(), allowed.end(), value) != allowed.end();
}

bool CheckpointStorageDtypeBytes(
    const std::string& dtype,
    uint64_t* bytes) {
  if (dtype == "F8_E4M3" || dtype == "F8_E8M0" ||
      dtype == "PACKED_FP4_I8") {
    *bytes = 1U;
    return true;
  }
  if (dtype == "BF16") {
    *bytes = 2U;
    return true;
  }
  if (dtype == "F32") {
    *bytes = 4U;
    return true;
  }
  if (dtype == "I64") {
    *bytes = 8U;
    return true;
  }
  return false;
}

bool ValidateTargetModel(
    const JsonValue& model,
    AnalyticalRunContract* contract,
    ValidatedTargetModel* validated) {
  std::string api_version;
  std::string kind;
  std::string schema_semver;
  std::string metadata_id;
  std::string source_repository;
  std::string source_commit;
  std::string source_header_digest;
  std::string registry_representation;
  std::string declared_registry_digest;
  std::string active_unit;
  std::string auxiliary_unit;
  std::string checkpoint_unit;
  std::string checkpoint_semantics;
  int hidden_size = 0;
  int main_moe_layers = 0;
  int mtp_moe_layers = 0;
  int hash_routed_main_layers = 0;
  int baseline_routed_experts = 0;
  uint64_t declared_logical_parameters = 0U;
  uint64_t declared_active_main = 0U;
  uint64_t declared_active_forward = 0U;
  uint64_t declared_active_training = 0U;
  uint64_t declared_quant_scale_elements = 0U;
  uint64_t declared_routing_table_elements = 0U;
  uint64_t declared_auxiliary_elements = 0U;
  uint64_t declared_checkpoint_bytes = 0U;

  const JsonValue* metadata = Member(model, "metadata");
  const JsonValue* spec = Member(model, "spec");
  const JsonValue* source = spec == nullptr ? nullptr : Member(*spec, "source");
  const JsonValue* architecture =
      spec == nullptr ? nullptr : Member(*spec, "architecture");
  const JsonValue* registry =
      spec == nullptr ? nullptr : Member(*spec, "tensorRegistry");
  const JsonValue* entries =
      registry == nullptr ? nullptr : Member(*registry, "entries");
  const JsonValue* active =
      spec == nullptr ? nullptr : Member(*spec, "activeLogicalParameters");
  const JsonValue* auxiliary =
      spec == nullptr ? nullptr : Member(*spec, "checkpointAuxiliaryElements");
  const JsonValue* checkpoint =
      spec == nullptr ? nullptr : Member(*spec, "checkpointStorage");

  if (!ObjectHasExactKeys(
          model,
          {"apiVersion", "kind", "schemaSemver", "metadata", "spec"}) ||
      !StringMember(model, "apiVersion", &api_version) ||
      api_version != "simai.target.model/v1alpha1" ||
      !StringMember(model, "kind", &kind) || kind != "ModelManifest" ||
      !StringMember(model, "schemaSemver", &schema_semver) ||
      schema_semver != "0.1.0" || metadata == nullptr ||
      !ObjectHasExactKeys(*metadata, {"id"}) ||
      !StringMember(*metadata, "id", &metadata_id) ||
      metadata_id != "deepseek-v4-pro-target-10t" || spec == nullptr ||
      !ObjectHasExactKeys(
          *spec,
          {"source",
           "architecture",
           "tensorRegistry",
           "logicalTrainableParameters",
           "activeLogicalParameters",
           "checkpointStorage",
           "evidenceRef",
           "evidenceClass",
           "readiness",
           "evidence",
           "checkpointAuxiliaryElements"}) ||
      source == nullptr ||
      !ObjectHasExactKeys(
          *source, {"repository", "commit", "officialHeaderDigest"}) ||
      architecture == nullptr ||
      !ObjectHasExactKeys(
          *architecture,
          {"hiddenSize",
           "mainMoeLayers",
           "mtpMoeLayers",
           "hashRoutedMainLayers",
           "baselineRoutedExperts",
           "routedExperts",
           "topK",
           "expertIntermediateSize",
           "sharedExperts"}) ||
      registry == nullptr ||
      !ObjectHasExactKeys(
          *registry, {"representation", "canonicalDigest", "entries"}) ||
      entries == nullptr || entries->type != JsonValue::Type::Array ||
      active == nullptr ||
      !ObjectHasExactKeys(
          *active,
          {"mainBlocksOnly",
           "mainForwardIncludingIo",
           "trainingGraphIncludingMtp",
           "unit"}) ||
      auxiliary == nullptr ||
      !ObjectHasExactKeys(
          *auxiliary, {"quantScale", "routingTable", "total", "unit"}) ||
      checkpoint == nullptr ||
      !ObjectHasExactKeys(*checkpoint, {"value", "unit", "semantics"}) ||
      !TargetEvidenceHasExactShape(*spec, "tensorDataDownloaded") ||
      !StringMember(*source, "repository", &source_repository) ||
      source_repository != "deepseek-ai/DeepSeek-V4-Pro" ||
      !StringMember(*source, "commit", &source_commit) ||
      source_commit != "45040942eb0d1c4e29fa6b92a6195f110e9e7444" ||
      !StringMember(*source, "officialHeaderDigest", &source_header_digest) ||
      source_header_digest !=
          "sha256:a3a39b9ccb4e729851922fc9c770f5c5755e7b9d7e96cd02c23f0e12b5e25cb9" ||
      !ExactPositiveIntMember(*architecture, "hiddenSize", &hidden_size) ||
      !ExactPositiveIntMember(
          *architecture, "mainMoeLayers", &main_moe_layers) ||
      !ExactPositiveIntMember(
          *architecture, "mtpMoeLayers", &mtp_moe_layers) ||
      !ExactPositiveIntMember(
          *architecture, "hashRoutedMainLayers", &hash_routed_main_layers) ||
      !ExactPositiveIntMember(
          *architecture,
          "baselineRoutedExperts",
          &baseline_routed_experts) ||
      !ExactPositiveIntMember(
          *architecture, "routedExperts", &validated->routed_experts) ||
      !ExactPositiveIntMember(*architecture, "topK", &validated->top_k) ||
      !ExactPositiveIntMember(
          *architecture,
          "expertIntermediateSize",
          &validated->expert_intermediate_size) ||
      !ExactPositiveIntMember(
          *architecture, "sharedExperts", &validated->shared_experts) ||
      !StringMember(*registry, "representation", &registry_representation) ||
      registry_representation != "CANONICAL_TENSOR_TYPE_REGISTRY_V1" ||
      !StringMember(
          *registry, "canonicalDigest", &declared_registry_digest) ||
      !IsSha256Identifier(declared_registry_digest) ||
      !ExactPositiveUint64Member(
          *spec,
          "logicalTrainableParameters",
          &declared_logical_parameters) ||
      !ExactPositiveUint64Member(
          *active, "mainBlocksOnly", &declared_active_main) ||
      !ExactPositiveUint64Member(
          *active, "mainForwardIncludingIo", &declared_active_forward) ||
      !ExactPositiveUint64Member(
          *active, "trainingGraphIncludingMtp", &declared_active_training) ||
      !StringMember(*active, "unit", &active_unit) || active_unit != "count" ||
      !ExactPositiveUint64Member(
          *auxiliary, "quantScale", &declared_quant_scale_elements) ||
      !ExactPositiveUint64Member(
          *auxiliary, "routingTable", &declared_routing_table_elements) ||
      !ExactPositiveUint64Member(
          *auxiliary, "total", &declared_auxiliary_elements) ||
      !StringMember(*auxiliary, "unit", &auxiliary_unit) ||
      auxiliary_unit != "count" ||
      !ExactPositiveUint64Member(
          *checkpoint, "value", &declared_checkpoint_bytes) ||
      !StringMember(*checkpoint, "unit", &checkpoint_unit) ||
      checkpoint_unit != "B" ||
      !StringMember(*checkpoint, "semantics", &checkpoint_semantics) ||
      checkpoint_semantics !=
          "FIXED_QUANTIZED_CHECKPOINT_ONLY_NOT_TRAINING_HBM" ||
      !ValidateTargetResourceEvidence(
          *spec,
          &validated->evidence_level,
          &validated->field_readiness)) {
    Reject(
        contract,
        "TARGET_MODEL_SCHEMA_INVALID",
        "The Target Model Manifest schema or evidence is invalid.",
        "Provide the exact public v1alpha1 Target Model Manifest.");
    return false;
  }

  const std::vector<std::string> allowed_logical_dtypes = {
      "BF16", "F32", "F8_E4M3", "F8_E8M0", "FP4", "I64"};
  const std::vector<std::string> allowed_scopes = {
      "GLOBAL", "MAIN_BLOCK", "MTP_BLOCK"};
  const std::vector<std::string> allowed_kinds = {
      "ATTENTION",
      "ATTENTION_INDEXER",
      "EMBEDDING",
      "HEAD",
      "HYPERCONNECTION",
      "MTP_PROJECTION",
      "NORM",
      "QUANT_SCALE",
      "ROUTED_EXPERT",
      "ROUTER",
      "ROUTING_TABLE",
      "SHARED_EXPERT"};

  std::map<std::string, bool> observed_ids;
  std::ostringstream canonical_registry;
  uint64_t tensor_instances = 0U;
  uint64_t computed_logical_parameters = 0U;
  uint64_t computed_checkpoint_bytes = 0U;
  uint64_t computed_auxiliary_elements = 0U;
  uint64_t computed_quant_scale_elements = 0U;
  uint64_t computed_routing_table_elements = 0U;
  uint64_t global_non_routed = 0U;
  uint64_t main_non_routed = 0U;
  uint64_t main_routed = 0U;
  uint64_t mtp_non_routed = 0U;
  uint64_t mtp_routed = 0U;
  bool registry_valid = entries->array.size() == 76U;

  for (const JsonValue& entry : entries->array) {
    std::string id;
    std::string logical_dtype;
    std::string storage_dtype;
    std::string role;
    std::string scope;
    std::string tensor_kind;
    uint64_t instances = 0U;
    uint64_t logical_elements_per_instance = 0U;
    uint64_t storage_elements_per_instance = 0U;
    uint64_t logical_elements = 0U;
    uint64_t storage_elements = 0U;
    uint64_t storage_dtype_bytes = 0U;
    uint64_t storage_bytes = 0U;
    std::string logical_shape_text;
    std::string storage_shape_text;
    const JsonValue* logical_shape = Member(entry, "logicalShape");
    const JsonValue* storage_shape =
        Member(entry, "checkpointStorageShape");

    if (!ObjectHasExactKeys(
            entry,
            {"id",
             "instances",
             "logicalShape",
             "checkpointStorageShape",
             "logicalDtype",
             "checkpointStorageDtype",
             "trainableRole",
             "blockScope",
             "tensorKind"}) ||
        !StringMember(entry, "id", &id) || id.empty() ||
        observed_ids.count(id) != 0U ||
        !ExactPositiveUint64Member(entry, "instances", &instances) ||
        logical_shape == nullptr ||
        !PositiveShapeProduct(
            *logical_shape,
            &logical_elements_per_instance,
            &logical_shape_text) ||
        storage_shape == nullptr ||
        !PositiveShapeProduct(
            *storage_shape,
            &storage_elements_per_instance,
            &storage_shape_text) ||
        !StringMember(entry, "logicalDtype", &logical_dtype) ||
        !StringIsOneOf(logical_dtype, allowed_logical_dtypes) ||
        !StringMember(
            entry, "checkpointStorageDtype", &storage_dtype) ||
        !CheckpointStorageDtypeBytes(storage_dtype, &storage_dtype_bytes) ||
        !StringMember(entry, "trainableRole", &role) ||
        (role != "LOGICAL_TRAINABLE" &&
         role != "CHECKPOINT_AUXILIARY") ||
        !StringMember(entry, "blockScope", &scope) ||
        !StringIsOneOf(scope, allowed_scopes) ||
        !StringMember(entry, "tensorKind", &tensor_kind) ||
        !StringIsOneOf(tensor_kind, allowed_kinds) ||
        !SafeMultiply(
            logical_elements_per_instance, instances, &logical_elements) ||
        !SafeMultiply(
            storage_elements_per_instance, instances, &storage_elements) ||
        !SafeMultiply(
            storage_elements, storage_dtype_bytes, &storage_bytes) ||
        !SafeMultiplyAdd(instances, 1U, &tensor_instances) ||
        !SafeMultiplyAdd(storage_bytes, 1U, &computed_checkpoint_bytes)) {
      registry_valid = false;
      break;
    }

    uint64_t doubled_storage_elements = 0U;
    const bool fp4_shape_is_closed =
        logical_dtype == "FP4" &&
        storage_dtype == "PACKED_FP4_I8" &&
        SafeMultiply(storage_elements_per_instance, 2U, &doubled_storage_elements) &&
        doubled_storage_elements == logical_elements_per_instance;
    const bool ordinary_shape_is_closed =
        logical_dtype != "FP4" && logical_dtype == storage_dtype &&
        logical_elements_per_instance == storage_elements_per_instance;
    const bool auxiliary_kind =
        tensor_kind == "QUANT_SCALE" || tensor_kind == "ROUTING_TABLE";
    if ((!fp4_shape_is_closed && !ordinary_shape_is_closed) ||
        (role == "CHECKPOINT_AUXILIARY") != auxiliary_kind) {
      registry_valid = false;
      break;
    }

    if (role == "LOGICAL_TRAINABLE") {
      if (!SafeMultiplyAdd(logical_elements, 1U, &computed_logical_parameters)) {
        registry_valid = false;
        break;
      }
      uint64_t* scoped_total = nullptr;
      if (scope == "GLOBAL") {
        scoped_total = &global_non_routed;
      } else if (scope == "MAIN_BLOCK") {
        scoped_total =
            tensor_kind == "ROUTED_EXPERT" ? &main_routed : &main_non_routed;
      } else {
        scoped_total =
            tensor_kind == "ROUTED_EXPERT" ? &mtp_routed : &mtp_non_routed;
      }
      if (!SafeMultiplyAdd(logical_elements, 1U, scoped_total)) {
        registry_valid = false;
        break;
      }
    } else {
      if (!SafeMultiplyAdd(
              storage_elements, 1U, &computed_auxiliary_elements)) {
        registry_valid = false;
        break;
      }
      uint64_t* auxiliary_total = tensor_kind == "QUANT_SCALE"
          ? &computed_quant_scale_elements
          : &computed_routing_table_elements;
      if (!SafeMultiplyAdd(storage_elements, 1U, auxiliary_total)) {
        registry_valid = false;
        break;
      }
    }

    canonical_registry
        << id << '|' << instances << '|' << logical_shape_text << '|'
        << storage_shape_text << '|' << logical_dtype << '|' << storage_dtype
        << '|' << role << '|' << scope << '|' << tensor_kind << '\n';
    observed_ids[id] = true;
  }

  const std::string computed_registry_digest =
      "sha256:" + Sha256Hex(canonical_registry.str());
  uint64_t active_main_routed = 0U;
  uint64_t active_mtp_routed = 0U;
  uint64_t computed_active_main = 0U;
  uint64_t computed_active_forward = 0U;
  uint64_t computed_active_training = 0U;
  registry_valid = registry_valid && observed_ids.size() == 76U &&
      main_routed % 2048U == 0U && mtp_routed % 2048U == 0U &&
      SafeMultiply(main_routed / 2048U, 16U, &active_main_routed) &&
      SafeMultiply(mtp_routed / 2048U, 16U, &active_mtp_routed) &&
      SafeMultiplyAdd(main_non_routed, 1U, &computed_active_main) &&
      SafeMultiplyAdd(active_main_routed, 1U, &computed_active_main);
  computed_active_forward = computed_active_main;
  registry_valid = registry_valid &&
      SafeMultiplyAdd(global_non_routed, 1U, &computed_active_forward);
  computed_active_training = computed_active_forward;
  registry_valid = registry_valid &&
      SafeMultiplyAdd(mtp_non_routed, 1U, &computed_active_training) &&
      SafeMultiplyAdd(active_mtp_routed, 1U, &computed_active_training);

  const std::string frozen_registry_digest =
      "sha256:f5984772e2fca84aeb8e36786b1273c26576a083cf17dd07a1eb0637e0d9daa2";
  if (!registry_valid ||
      declared_registry_digest != frozen_registry_digest ||
      computed_registry_digest != frozen_registry_digest ||
      tensor_instances != 764124U ||
      computed_logical_parameters != 8414884746526ULL ||
      computed_auxiliary_elements != 262134842368ULL ||
      computed_quant_scale_elements != 262128636928ULL ||
      computed_routing_table_elements != 6205440ULL ||
      computed_checkpoint_bytes != 4486847493752ULL ||
      computed_active_main != 88950053982ULL ||
      computed_active_forward != 90803533923ULL ||
      computed_active_training != 92345423134ULL) {
    Reject(
        contract,
        "TARGET_MODEL_REGISTRY_INVALID",
        "The canonical tensor-type registry is incomplete or inconsistent.",
        "Restore the frozen logical/storage shapes, dtypes, roles, and scopes.");
    return false;
  }
  if (hidden_size != 7168 || main_moe_layers != 61 ||
      mtp_moe_layers != 1 || hash_routed_main_layers != 3 ||
      baseline_routed_experts != 384 || validated->routed_experts != 2048 ||
      validated->top_k != 16 ||
      validated->expert_intermediate_size != 3072 ||
      validated->shared_experts != 1 ||
      declared_logical_parameters != computed_logical_parameters ||
      declared_auxiliary_elements != computed_auxiliary_elements ||
      declared_quant_scale_elements != computed_quant_scale_elements ||
      declared_routing_table_elements != computed_routing_table_elements ||
      declared_checkpoint_bytes != computed_checkpoint_bytes ||
      declared_active_main != computed_active_main ||
      declared_active_forward != computed_active_forward ||
      declared_active_training != computed_active_training) {
    Reject(
        contract,
        "TARGET_MODEL_IDENTITY_MISMATCH",
        "The Target Model differs from the frozen 10T-scale identity.",
        "Restore the frozen architecture and independently computed summaries.");
    return false;
  }

  validated->logical_trainable_parameters = computed_logical_parameters;
  validated->checkpoint_auxiliary_elements =
      computed_auxiliary_elements;
  validated->checkpoint_quant_scale_elements =
      computed_quant_scale_elements;
  validated->checkpoint_routing_table_elements =
      computed_routing_table_elements;
  validated->checkpoint_storage_bytes = computed_checkpoint_bytes;
  validated->active_main_blocks_parameters = computed_active_main;
  validated->active_main_forward_parameters = computed_active_forward;
  validated->active_training_graph_parameters = computed_active_training;
  return true;
}

bool LoadTargetModel(
    const JsonValue& target_workload,
    AnalyticalRunContract* contract) {
  const JsonValue* model_reference = Member(target_workload, "model");
  if (model_reference == nullptr) {
    RejectUnsupported(
        contract,
        "TARGET_MODEL_REQUIRED",
        "The Target Workload has no Model Manifest.",
        "Provide target_workload.model with an immutable SHA-256 reference.");
    return false;
  }
  const ArtifactLoadPolicy model_policy = {
      "TARGET_MODEL_REFERENCE_INVALID",
      "The Target Model reference is invalid.",
      "Provide target_workload.model.path and its SHA-256 digest.",
      "TARGET_MODEL_NOT_FOUND",
      "The Target Model Manifest could not be read.",
      "Provide a readable public Target Model Manifest.",
      "TARGET_MODEL_DIGEST_MISMATCH",
      "The Target Model does not match its declared digest.",
      "Use the intended immutable Model Manifest or update its digest.",
      "TARGET_MODEL_INVALID_JSON",
      "The Target Model Manifest is not valid JSON.",
      "Correct the Model Manifest JSON and retry."};
  LoadedArtifact model_artifact;
  if (!LoadArtifact(
          *model_reference,
          model_policy,
          contract,
          &model_artifact,
          kMaximumTargetModelArtifactBytes,
          "TARGET_MODEL_ARTIFACT_TOO_LARGE",
          "The Target Model Manifest exceeds its byte limit.",
          "Keep the canonical Target Model Manifest within 256 KiB.")) {
    return false;
  }
  contract->target_model_sha256 = model_artifact.sha256;
  ValidatedTargetModel model;
  if (!ValidateTargetModel(model_artifact.document, contract, &model)) {
    return false;
  }
  contract->target_model_ready = true;
  contract->target_logical_trainable_parameters =
      model.logical_trainable_parameters;
  contract->target_checkpoint_auxiliary_elements =
      model.checkpoint_auxiliary_elements;
  contract->target_checkpoint_quant_scale_elements =
      model.checkpoint_quant_scale_elements;
  contract->target_checkpoint_routing_table_elements =
      model.checkpoint_routing_table_elements;
  contract->target_checkpoint_storage_bytes = model.checkpoint_storage_bytes;
  contract->target_active_main_blocks_parameters =
      model.active_main_blocks_parameters;
  contract->target_active_main_forward_parameters =
      model.active_main_forward_parameters;
  contract->target_active_training_graph_parameters =
      model.active_training_graph_parameters;
  contract->target_routed_experts = model.routed_experts;
  contract->target_top_k = model.top_k;
  contract->target_expert_intermediate_size = model.expert_intermediate_size;
  contract->target_shared_experts = model.shared_experts;
  contract->target_model_evidence_level = model.evidence_level;
  contract->target_model_field_readiness = model.field_readiness;
  return true;
}

struct ValidatedTargetStep {
  uint64_t sequence_tokens = 0;
  uint64_t micro_batch_sequences = 0;
  uint64_t data_parallel_replicas = 0;
  uint64_t gradient_accumulation = 0;
  uint64_t configured_gts = 0;
  uint64_t routed_assignment_slots = 0;
  std::string evidence_level;
  std::string field_readiness;
};

bool ValidateTargetStep(
    const JsonValue& step,
    const std::string& model_digest,
    AnalyticalRunContract* contract,
    ValidatedTargetStep* validated) {
  std::string api_version;
  std::string kind;
  std::string schema_semver;
  std::string referenced_model_digest;
  std::string unit;
  std::string formula;
  std::string metadata_id;
  const JsonValue* metadata = Member(step, "metadata");
  const JsonValue* spec = Member(step, "spec");
  if (!ObjectHasExactKeys(
          step,
          {"apiVersion", "kind", "schemaSemver", "metadata", "spec"}) ||
      !StringMember(step, "apiVersion", &api_version) ||
      api_version != "simai.target.step/v1alpha1" ||
      !StringMember(step, "kind", &kind) ||
      kind != "OptimizerStepManifest" ||
      !StringMember(step, "schemaSemver", &schema_semver) ||
      schema_semver != "0.1.0" || metadata == nullptr ||
      !ObjectHasExactKeys(*metadata, {"id"}) ||
      !StringMember(*metadata, "id", &metadata_id) ||
      metadata_id != "target-500m-global-token-step" || spec == nullptr ||
      !ObjectHasExactKeys(
          *spec,
          {"modelDigest",
           "sequenceTokens",
           "microBatchSequences",
           "dataParallelReplicas",
           "gradientAccumulation",
           "configuredGlobalTokens",
           "globalTokenUnit",
           "formula",
           "configuredRoutedAssignmentSlotsUpperBound",
           "routedLayersInScope",
           "evidenceRef",
           "evidenceClass",
           "readiness",
           "evidence"}) ||
      !TargetEvidenceHasExactShape(*spec, "runtimeExecuted") ||
      !StringMember(*spec, "modelDigest", &referenced_model_digest) ||
      !StringMember(*spec, "globalTokenUnit", &unit) || unit != "count" ||
      !StringMember(*spec, "formula", &formula) ||
      formula !=
          "sequenceTokens * microBatchSequences * dataParallelReplicas * gradientAccumulation" ||
      !ValidateTargetResourceEvidence(
          *spec,
          &validated->evidence_level,
          &validated->field_readiness)) {
    Reject(
        contract,
        "TARGET_STEP_SCHEMA_INVALID",
        "The Target Step Manifest schema or evidence is invalid.",
        "Provide the v1alpha1 optimizer-step GTS contract.");
    return false;
  }
  if (referenced_model_digest != model_digest) {
    Reject(
        contract,
        "TARGET_STEP_MODEL_DIGEST_MISMATCH",
        "The Target Step references a different Model Manifest.",
        "Bind the Step and Target Workload to the same model digest.");
    return false;
  }
  if (!ExactPositiveUint64Member(
          *spec, "sequenceTokens", &validated->sequence_tokens) ||
      !ExactPositiveUint64Member(
          *spec,
          "microBatchSequences",
          &validated->micro_batch_sequences) ||
      !ExactPositiveUint64Member(
          *spec,
          "dataParallelReplicas",
          &validated->data_parallel_replicas) ||
      !ExactPositiveUint64Member(
          *spec,
          "gradientAccumulation",
          &validated->gradient_accumulation)) {
    Reject(
        contract,
        "TARGET_STEP_FACTOR_INVALID",
        "A GTS factor is zero, negative, non-integer, or unsafe.",
        "Use positive JSON-safe integers for sequence, MBS, DP, and GA.");
    return false;
  }
  uint64_t sequence_micro_batch = 0U;
  uint64_t sequence_micro_batch_dp = 0U;
  if (!SafeMultiply(
          validated->sequence_tokens,
          validated->micro_batch_sequences,
          &sequence_micro_batch) ||
      !SafeMultiply(
          sequence_micro_batch,
          validated->data_parallel_replicas,
          &sequence_micro_batch_dp) ||
      !SafeMultiply(
          sequence_micro_batch_dp,
          validated->gradient_accumulation,
          &validated->configured_gts)) {
    Reject(
        contract,
        "TARGET_STEP_GTS_OVERFLOW",
        "The configured GTS multiplication overflows uint64.",
        "Reduce sequence, MBS, DP, or GA before retrying.");
    return false;
  }
  uint64_t declared_gts = 0U;
  if (!ExactPositiveUint64Member(
          *spec, "configuredGlobalTokens", &declared_gts) ||
      declared_gts != validated->configured_gts) {
    Reject(
        contract,
        "TARGET_STEP_GTS_MISMATCH",
        "The declared GTS differs from sequence times MBS times DP times GA.",
        "Recompute configuredGlobalTokens from the four canonical factors.");
    return false;
  }
  if (validated->configured_gts > 500000000U) {
    Reject(
        contract,
        "TARGET_STEP_GTS_LIMIT_EXCEEDED",
        "The configured GTS exceeds 500,000,000 tokens per optimizer step.",
        "Use a sequence, MBS, DP, and GA product no greater than 500M.");
    return false;
  }
  uint64_t routed_layers = 0U;
  uint64_t declared_assignment_slots = 0U;
  uint64_t gts_times_top_k = 0U;
  if (!ExactPositiveUint64Member(
          *spec, "routedLayersInScope", &routed_layers) ||
      routed_layers != 62U ||
      !ExactPositiveUint64Member(
          *spec,
          "configuredRoutedAssignmentSlotsUpperBound",
          &declared_assignment_slots) ||
      !SafeMultiply(validated->configured_gts, 16U, &gts_times_top_k) ||
      !SafeMultiply(
          gts_times_top_k,
          routed_layers,
          &validated->routed_assignment_slots) ||
      declared_assignment_slots != validated->routed_assignment_slots) {
    Reject(
        contract,
        "TARGET_STEP_ASSIGNMENT_MISMATCH",
        "The routed assignment-slot upper bound is inconsistent.",
        "Use configured GTS times TopK 16 times 62 routed layers.");
    return false;
  }
  return true;
}

bool LoadTargetStep(
    const JsonValue& target_workload,
    AnalyticalRunContract* contract) {
  const JsonValue* step_reference = Member(target_workload, "step");
  if (step_reference == nullptr) {
    RejectUnsupported(
        contract,
        "TARGET_STEP_REQUIRED",
        "The Target Workload has no Step/GTS Manifest.",
        "Provide target_workload.step with an immutable SHA-256 reference.");
    return false;
  }
  const ArtifactLoadPolicy step_policy = {
      "TARGET_STEP_REFERENCE_INVALID",
      "The Target Step reference is invalid.",
      "Provide target_workload.step.path and its SHA-256 digest.",
      "TARGET_STEP_NOT_FOUND",
      "The Target Step Manifest could not be read.",
      "Provide a readable public Step/GTS Manifest.",
      "TARGET_STEP_DIGEST_MISMATCH",
      "The Target Step does not match its declared digest.",
      "Use the intended immutable Step Manifest or update its digest.",
      "TARGET_STEP_INVALID_JSON",
      "The Target Step Manifest is not valid JSON.",
      "Correct the Step Manifest JSON and retry."};
  LoadedArtifact step_artifact;
  if (!LoadArtifact(
          *step_reference,
          step_policy,
          contract,
          &step_artifact,
          kMaximumTargetStepArtifactBytes,
          "TARGET_STEP_ARTIFACT_TOO_LARGE",
          "The Target Step Manifest exceeds its byte limit.",
          "Keep the canonical Target Step Manifest within 64 KiB.")) {
    return false;
  }
  contract->target_step_sha256 = step_artifact.sha256;
  ValidatedTargetStep step;
  if (!ValidateTargetStep(
          step_artifact.document,
          contract->target_model_sha256,
          contract,
          &step)) {
    return false;
  }
  contract->target_step_ready = true;
  contract->target_sequence_tokens = step.sequence_tokens;
  contract->target_micro_batch_sequences = step.micro_batch_sequences;
  contract->target_data_parallel_replicas = step.data_parallel_replicas;
  contract->target_gradient_accumulation = step.gradient_accumulation;
  contract->target_configured_gts = step.configured_gts;
  contract->target_routed_assignment_slots = step.routed_assignment_slots;
  contract->target_step_evidence_level = step.evidence_level;
  contract->target_step_field_readiness = step.field_readiness;
  return true;
}

struct ValidatedTargetRouting {
  std::string evidence_level;
  std::string field_readiness;
};

bool ValidateTargetRouting(
    const JsonValue& routing,
    const AnalyticalRunContract& loaded_contract,
    AnalyticalRunContract* contract,
    ValidatedTargetRouting* validated) {
  std::string api_version;
  std::string kind;
  std::string schema_semver;
  std::string model_digest;
  std::string step_digest;
  std::string mode;
  std::string counts;
  std::string metadata_id;
  int routed_experts = 0;
  int top_k = 0;
  int hash_layers = 0;
  const JsonValue* metadata = Member(routing, "metadata");
  const JsonValue* spec = Member(routing, "spec");
  const JsonValue* policy = spec == nullptr ? nullptr : Member(*spec, "policy");
  if (!ObjectHasExactKeys(
          routing,
          {"apiVersion", "kind", "schemaSemver", "metadata", "spec"}) ||
      !StringMember(routing, "apiVersion", &api_version) ||
      api_version != "simai.target.routing/v1alpha1" ||
      !StringMember(routing, "kind", &kind) ||
      kind != "TargetRoutingArtifact" ||
      !StringMember(routing, "schemaSemver", &schema_semver) ||
      schema_semver != "0.1.0" || metadata == nullptr ||
      !ObjectHasExactKeys(*metadata, {"id"}) ||
      !StringMember(*metadata, "id", &metadata_id) ||
      metadata_id != "target-hash-then-topk-routing" || spec == nullptr ||
      policy == nullptr ||
      !ObjectHasExactKeys(
          *spec,
          {"modelDigest",
           "stepDigest",
           "policy",
           "evidenceRef",
           "evidenceClass",
           "readiness",
           "evidence"}) ||
      !ObjectHasExactKeys(
          *policy,
          {"mode",
           "routedExperts",
           "topK",
           "hashRoutedMainLayers",
           "counts"}) ||
      !TargetEvidenceHasExactShape(*spec, "observedCountsAvailable") ||
      !StringMember(*spec, "modelDigest", &model_digest) ||
      !StringMember(*spec, "stepDigest", &step_digest) ||
      !StringMember(*policy, "mode", &mode) ||
      mode != "HASH_FIRST_THREE_THEN_TOPK" ||
      !ExactPositiveIntMember(*policy, "routedExperts", &routed_experts) ||
      !ExactPositiveIntMember(*policy, "topK", &top_k) ||
      !ExactPositiveIntMember(
          *policy, "hashRoutedMainLayers", &hash_layers) ||
      !StringMember(*policy, "counts", &counts) ||
      counts != "SYMBOLIC_UNMATERIALIZED" ||
      !ValidateTargetResourceEvidence(
          *spec,
          &validated->evidence_level,
          &validated->field_readiness)) {
    Reject(
        contract,
        "TARGET_ROUTING_SCHEMA_INVALID",
        "The Target Routing Artifact schema or evidence is invalid.",
        "Provide the v1alpha1 external hash-then-TopK routing artifact.");
    return false;
  }
  if (model_digest != loaded_contract.target_model_sha256) {
    Reject(
        contract,
        "TARGET_ROUTING_MODEL_DIGEST_MISMATCH",
        "The Target Routing Artifact references a different model.",
        "Bind routing.modelDigest to the selected Model Manifest.");
    return false;
  }
  if (step_digest != loaded_contract.target_step_sha256) {
    Reject(
        contract,
        "TARGET_ROUTING_STEP_DIGEST_MISMATCH",
        "The Target Routing Artifact references a different step.",
        "Bind routing.stepDigest to the selected Step Manifest.");
    return false;
  }
  if (routed_experts != 2048 || top_k != 16 || hash_layers != 3) {
    Reject(
        contract,
        "TARGET_ROUTING_MODEL_IDENTITY_MISMATCH",
        "Routing policy differs from the frozen Target Model identity.",
        "Use 2048 routed experts, TopK 16, and three hash-routed layers.");
    return false;
  }
  return true;
}

bool LoadTargetRouting(
    const JsonValue& target_workload,
    AnalyticalRunContract* contract) {
  const JsonValue* routing_reference = Member(target_workload, "routing");
  if (routing_reference == nullptr) {
    RejectUnsupported(
        contract,
        "TARGET_ROUTING_REQUIRED",
        "The Target Workload has no external Routing Artifact.",
        "Provide target_workload.routing with an immutable SHA-256 reference.");
    return false;
  }
  const ArtifactLoadPolicy routing_policy = {
      "TARGET_ROUTING_REFERENCE_INVALID",
      "The Target Routing reference is invalid.",
      "Provide target_workload.routing.path and its SHA-256 digest.",
      "TARGET_ROUTING_NOT_FOUND",
      "The Target Routing Artifact could not be read.",
      "Provide a readable public Target Routing Artifact.",
      "TARGET_ROUTING_DIGEST_MISMATCH",
      "The Target Routing Artifact does not match its declared digest.",
      "Use the intended immutable Routing Artifact or update its digest.",
      "TARGET_ROUTING_INVALID_JSON",
      "The Target Routing Artifact is not valid JSON.",
      "Correct the Routing Artifact JSON and retry."};
  LoadedArtifact routing_artifact;
  if (!LoadArtifact(
          *routing_reference,
          routing_policy,
          contract,
          &routing_artifact,
          kMaximumTargetRoutingArtifactBytes,
          "TARGET_ROUTING_ARTIFACT_TOO_LARGE",
          "The Target Routing Artifact exceeds its byte limit.",
          "Keep the symbolic Target Routing Artifact within 64 KiB.")) {
    return false;
  }
  contract->target_routing_sha256 = routing_artifact.sha256;
  ValidatedTargetRouting routing;
  if (!ValidateTargetRouting(
          routing_artifact.document, *contract, contract, &routing)) {
    return false;
  }
  contract->target_routing_ready = true;
  contract->target_routing_evidence_level = routing.evidence_level;
  contract->target_routing_field_readiness = routing.field_readiness;
  return true;
}

struct ValidatedTargetMemoryPlan {
  std::string evidence_level;
  std::string field_readiness;
  bool symbolic = false;
  bool materialized = false;
  std::map<std::string, std::string> binding_digests;
  std::map<std::string, uint64_t> component_bytes;
  uint64_t peak_bytes = 0;
  uint64_t base_hbm_bytes = 0;
  uint64_t reserve_hbm_bytes = 0;
  uint64_t usable_hbm_bytes = 0;
  uint64_t search_limit_bytes = 0;
  bool execution_peak_known = false;
  uint64_t execution_peak_bytes = 0;
  uint64_t execution_boundary_bytes = 0;
  uint64_t execution_maximum_accepted_bytes = 0;
  std::string search_gate = "UNKNOWN";
  std::string execution_gate = "UNKNOWN";
};

uint64_t FloorPercent(uint64_t value, uint64_t percent) {
  return (value / 100U) * percent + ((value % 100U) * percent) / 100U;
}

bool TargetMemoryComponentsHaveExactShape(const JsonValue& components) {
  if (components.type != JsonValue::Type::Array) {
    return false;
  }
  for (const JsonValue& component : components.array) {
    const std::vector<std::string> symbolic_keys = {
        "category", "allocateAt", "releaseAt", "expression", "requires"};
    const std::vector<std::string> materialized_keys = {
        "category",
        "allocateAt",
        "releaseAt",
        "expression",
        "requires",
        "peakBytes"};
    if (!ObjectHasExactKeys(component, symbolic_keys) &&
        !ObjectHasExactKeys(component, materialized_keys)) {
      return false;
    }
  }
  return true;
}

bool ValidateTargetMemoryPlan(
    const JsonValue& memory,
    const AnalyticalRunContract& loaded_contract,
    AnalyticalRunContract* contract,
    ValidatedTargetMemoryPlan* validated) {
  std::string api_version;
  std::string kind;
  std::string schema_semver;
  std::string model_digest;
  std::string step_digest;
  std::string routing_digest;
  std::string unit;
  std::string aggregation;
  std::string metadata_id;
  const JsonValue* metadata = Member(memory, "metadata");
  const JsonValue* spec = Member(memory, "spec");
  const JsonValue* bindings =
      spec == nullptr ? nullptr : Member(*spec, "bindings");
  const JsonValue* components =
      spec == nullptr ? nullptr : Member(*spec, "components");
  const JsonValue* capacity =
      spec == nullptr ? nullptr : Member(*spec, "capacity");
  if (!ObjectHasExactKeys(
          memory,
          {"apiVersion", "kind", "schemaSemver", "metadata", "spec"}) ||
      !StringMember(memory, "apiVersion", &api_version) ||
      api_version != "simai.target.memory/v1alpha1" ||
      !StringMember(memory, "kind", &kind) ||
      kind != "SymbolicMemoryEventPlan" ||
      !StringMember(memory, "schemaSemver", &schema_semver) ||
      schema_semver != "0.1.0" || metadata == nullptr ||
      !ObjectHasExactKeys(*metadata, {"id"}) ||
      !StringMember(*metadata, "id", &metadata_id) ||
      metadata_id != "target-symbolic-training-memory" || spec == nullptr ||
      !ObjectHasExactKeys(
          *spec,
          {"modelDigest",
           "stepDigest",
           "routingDigest",
           "unit",
           "aggregation",
           "bindings",
           "components",
           "capacity",
           "evidenceRef",
           "evidenceClass",
           "readiness",
           "evidence"}) ||
      bindings == nullptr ||
      bindings->type != JsonValue::Type::Object || components == nullptr ||
      components->type != JsonValue::Type::Array || capacity == nullptr ||
      capacity->type != JsonValue::Type::Object ||
      !ObjectHasExactKeys(
          *bindings,
          {"precision",
           "optimizer",
           "placement",
           "recomputation",
           "runtime"}) ||
      !TargetMemoryComponentsHaveExactShape(*components) ||
      !ObjectHasExactKeys(
          *capacity,
          {"baseHbmB",
           "reserveHbmB",
           "scenarioUsableHbmB",
           "plannedPeakHbmB",
           "observedExecutionPeakHbmB"}) ||
      !TargetEvidenceHasExactShape(*spec, "materializationPoliciesBound") ||
      !StringMember(*spec, "modelDigest", &model_digest) ||
      !StringMember(*spec, "stepDigest", &step_digest) ||
      !StringMember(*spec, "routingDigest", &routing_digest) ||
      !StringMember(*spec, "unit", &unit) || unit != "B" ||
      !StringMember(*spec, "aggregation", &aggregation) ||
      aggregation != "CONSERVATIVE_COMPONENT_PEAK_SUM" ||
      !ValidateTargetResourceEvidence(
          *spec,
          &validated->evidence_level,
          &validated->field_readiness)) {
    Reject(
        contract,
        "TARGET_MEMORY_SCHEMA_INVALID",
        "The Memory Event Plan schema or evidence is invalid.",
        "Provide the v1alpha1 symbolic lifetime plan in bytes.");
    return false;
  }
  if (model_digest != loaded_contract.target_model_sha256 ||
      step_digest != loaded_contract.target_step_sha256 ||
      routing_digest != loaded_contract.target_routing_sha256) {
    Reject(
        contract,
        "TARGET_MEMORY_RESOURCE_DIGEST_MISMATCH",
        "The Memory Event Plan references a different Target resource.",
        "Bind memory model/step/routing digests to this Target Workload.");
    return false;
  }
  const std::vector<std::string> binding_names = {
      "precision", "optimizer", "placement", "recomputation", "runtime"};
  size_t unbound_bindings = 0U;
  size_t bound_bindings = 0U;
  for (const std::string& binding_name : binding_names) {
    std::string binding_state;
    if (StringMember(*bindings, binding_name, &binding_state) &&
        binding_state == "UNBOUND") {
      ++unbound_bindings;
      continue;
    }
    const JsonValue* binding = Member(*bindings, binding_name);
    std::string digest;
    if (binding == nullptr || binding->type != JsonValue::Type::Object ||
        !ObjectHasExactKeys(*binding, {"state", "sha256"}) ||
        !StringMember(*binding, "state", &binding_state) ||
        binding_state != "BOUND" ||
        !StringMember(*binding, "sha256", &digest) ||
        !IsSha256Identifier(digest)) {
      Reject(
          contract,
          "TARGET_MEMORY_BINDING_INVALID",
          "The Memory Event Plan has an invalid materialization binding.",
          "Use all UNBOUND values or content-address every BOUND policy.");
      return false;
    }
    ++bound_bindings;
    validated->binding_digests[binding_name] = digest;
  }
  if (unbound_bindings != 0U && bound_bindings != 0U) {
    Reject(
        contract,
        "TARGET_MEMORY_BINDINGS_INCOMPLETE",
        "Memory materialization bindings are only partially bound.",
        "Bind all precision/optimizer/placement/recompute/runtime policies or none.");
    return false;
  }
  validated->symbolic = unbound_bindings == binding_names.size();
  validated->materialized = bound_bindings == binding_names.size();
  struct ExpectedMemoryComponent {
    const char* allocate_at;
    const char* release_at;
    const char* expression;
    std::vector<std::string> requirements;
  };
  const std::map<std::string, ExpectedMemoryComponent> expected_components = {
      {"PARAMETERS",
       {"JOB_INIT",
        "JOB_END",
        "logical trainable tensors * training parameter precision / parameter shards",
        {"precision", "placement"}}},
      {"GRADIENTS",
       {"BACKWARD_START_OR_PREALLOC",
        "OPTIMIZER_STEP_END",
        "logical trainable tensors * gradient precision / gradient shards",
        {"precision", "placement"}}},
      {"OPTIMIZER_STATES",
       {"OPTIMIZER_INIT",
        "JOB_END",
        "optimizer state tensors and optional master weights / optimizer shards",
        {"optimizer", "placement"}}},
      {"ACTIVATIONS",
       {"FORWARD_TENSOR_PRODUCED",
        "BACKWARD_TENSOR_CONSUMED",
        "saved activation shape trace after recomputation selection",
        {"precision", "placement", "recomputation"}}},
      {"COMMUNICATION_BUFFERS",
       {"COLLECTIVE_START",
        "COLLECTIVE_END",
        "dispatch combine collective and runtime scratch buffers",
        {"precision", "placement", "runtime"}}},
      {"EXPERT_PLACEMENT",
       {"EXPERT_DISPATCH_START",
        "EXPERT_COMBINE_END",
        "local routed expert weights and maximum local expert load",
        {"placement", "precision"}}},
      {"RECOMPUTATION",
       {"BACKWARD_RECOMPUTE_START",
        "BACKWARD_RECOMPUTE_END",
        "recomputed activation and observed kernel workspace",
        {"recomputation", "runtime"}}}};
  std::map<std::string, bool> observed_categories;
  bool components_valid = components->array.size() == expected_components.size();
  for (const JsonValue& component : components->array) {
    std::string category;
    std::string allocate_at;
    std::string release_at;
    std::string expression;
    const JsonValue* requirements = Member(component, "requires");
    if (!StringMember(component, "category", &category) ||
        expected_components.count(category) != 1U ||
        observed_categories.count(category) != 0U ||
        !StringMember(component, "allocateAt", &allocate_at) ||
        !StringMember(component, "releaseAt", &release_at) ||
        !StringMember(component, "expression", &expression) ||
        requirements == nullptr || requirements->type != JsonValue::Type::Array) {
      components_valid = false;
      break;
    }
    const ExpectedMemoryComponent& expected = expected_components.at(category);
    if (allocate_at != expected.allocate_at || release_at != expected.release_at ||
        expression != expected.expression ||
        requirements->array.size() != expected.requirements.size()) {
      components_valid = false;
      break;
    }
    for (size_t index = 0U; index < requirements->array.size(); ++index) {
      if (requirements->array[index].type != JsonValue::Type::String ||
          requirements->array[index].string != expected.requirements[index]) {
        components_valid = false;
        break;
      }
    }
    if (validated->materialized) {
      uint64_t peak_bytes = 0U;
      if (!ExactNonNegativeUint64Member(
              component, "peakBytes", &peak_bytes) ||
          !SafeMultiplyAdd(peak_bytes, 1U, &validated->peak_bytes)) {
        components_valid = false;
        break;
      }
      validated->component_bytes[category] = peak_bytes;
    } else if (Member(component, "peakBytes") != nullptr) {
      components_valid = false;
      break;
    }
    observed_categories[category] = true;
  }
  if (!components_valid ||
      observed_categories.size() != expected_components.size()) {
    Reject(
        contract,
        "TARGET_MEMORY_COMPONENTS_INVALID",
        "The Memory Event Plan component lifetimes are incomplete.",
        "Declare all seven memory categories with expressions and bindings.");
    return false;
  }
  if (validated->symbolic) {
    std::string base_hbm;
    std::string reserve_hbm;
    std::string usable_hbm;
    std::string planned_peak;
    std::string observed_peak;
    if (!StringMember(*capacity, "baseHbmB", &base_hbm) ||
        !StringMember(*capacity, "reserveHbmB", &reserve_hbm) ||
        !StringMember(*capacity, "scenarioUsableHbmB", &usable_hbm) ||
        !StringMember(*capacity, "plannedPeakHbmB", &planned_peak) ||
        !StringMember(
            *capacity, "observedExecutionPeakHbmB", &observed_peak) ||
        base_hbm != "UNKNOWN" || reserve_hbm != "UNKNOWN" ||
        usable_hbm != "UNKNOWN" || planned_peak != "UNKNOWN" ||
        observed_peak != "UNKNOWN") {
      Reject(
          contract,
          "TARGET_MEMORY_SYMBOLIC_CAPACITY_INVALID",
          "Unbound memory capacity fields must remain UNKNOWN.",
          "Do not fabricate HBM numbers before all policies are bound.");
      return false;
    }
    return true;
  }

  uint64_t declared_usable_hbm = 0U;
  uint64_t declared_peak = 0U;
  if (!ExactPositiveUint64Member(
          *capacity, "baseHbmB", &validated->base_hbm_bytes) ||
      !ExactNonNegativeUint64Member(
          *capacity, "reserveHbmB", &validated->reserve_hbm_bytes) ||
      !ExactPositiveUint64Member(
          *capacity, "scenarioUsableHbmB", &declared_usable_hbm) ||
      !ExactNonNegativeUint64Member(
          *capacity, "plannedPeakHbmB", &declared_peak) ||
      validated->reserve_hbm_bytes >= validated->base_hbm_bytes ||
      declared_usable_hbm !=
          validated->base_hbm_bytes - validated->reserve_hbm_bytes ||
      declared_peak != validated->peak_bytes) {
    Reject(
        contract,
        "TARGET_MEMORY_CAPACITY_MISMATCH",
        "Memory capacity or component peak accounting is inconsistent.",
        "Use usable=base-reserve and planned peak=sum(component peaks)." );
    return false;
  }
  validated->usable_hbm_bytes = declared_usable_hbm;
  validated->search_limit_bytes = FloorPercent(declared_usable_hbm, 95U);
  validated->search_gate = validated->peak_bytes <= validated->search_limit_bytes
      ? "PASS"
      : "FAIL";

  const JsonValue* observed_execution_peak =
      Member(*capacity, "observedExecutionPeakHbmB");
  if (observed_execution_peak == nullptr) {
    Reject(
        contract,
        "TARGET_MEMORY_CAPACITY_MISMATCH",
        "The execution peak field is missing.",
        "Use UNKNOWN or a nonnegative observed execution peak in bytes.");
    return false;
  }
  if (observed_execution_peak->type == JsonValue::Type::String) {
    if (observed_execution_peak->string != "UNKNOWN") {
      Reject(
          contract,
          "TARGET_MEMORY_CAPACITY_MISMATCH",
          "The execution peak sentinel is invalid.",
          "Use exactly UNKNOWN until an execution peak is available.");
      return false;
    }
  } else if (!ExactNonNegativeUint64Member(
                 *capacity,
                 "observedExecutionPeakHbmB",
                 &validated->execution_peak_bytes)) {
    Reject(
        contract,
        "TARGET_MEMORY_CAPACITY_MISMATCH",
        "The observed execution peak is invalid.",
        "Use UNKNOWN or a nonnegative JSON-safe integer byte count.");
    return false;
  } else {
    validated->execution_peak_known = true;
  }
  validated->execution_boundary_bytes =
      FloorPercent(validated->base_hbm_bytes, 85U);
  const uint64_t fractional_numerator =
      (validated->base_hbm_bytes % 100U) * 85U;
  const bool boundary_is_exact = fractional_numerator % 100U == 0U;
  validated->execution_maximum_accepted_bytes =
      boundary_is_exact
          ? validated->execution_boundary_bytes - 1U
          : validated->execution_boundary_bytes;
  if (validated->execution_peak_known) {
    validated->execution_gate =
        validated->execution_peak_bytes <=
                validated->execution_maximum_accepted_bytes
            ? "PASS"
            : "INVALID_ACCURACY_EXECUTION";
  }
  return true;
}

bool LoadTargetMemoryPlan(
    const JsonValue& target_workload,
    AnalyticalRunContract* contract) {
  const JsonValue* memory_reference =
      Member(target_workload, "memory_event_plan");
  if (memory_reference == nullptr) {
    RejectUnsupported(
        contract,
        "TARGET_MEMORY_EVENT_PLAN_REQUIRED",
        "The Target Workload has no Memory Event Plan.",
        "Provide target_workload.memory_event_plan with a SHA-256 reference.");
    return false;
  }
  const ArtifactLoadPolicy memory_policy = {
      "TARGET_MEMORY_REFERENCE_INVALID",
      "The Target Memory Event Plan reference is invalid.",
      "Provide memory_event_plan.path and its SHA-256 digest.",
      "TARGET_MEMORY_NOT_FOUND",
      "The Target Memory Event Plan could not be read.",
      "Provide a readable public Memory Event Plan.",
      "TARGET_MEMORY_DIGEST_MISMATCH",
      "The Target Memory Event Plan does not match its declared digest.",
      "Use the intended immutable Memory Event Plan or update its digest.",
      "TARGET_MEMORY_INVALID_JSON",
      "The Target Memory Event Plan is not valid JSON.",
      "Correct the Memory Event Plan JSON and retry."};
  LoadedArtifact memory_artifact;
  if (!LoadArtifact(
          *memory_reference,
          memory_policy,
          contract,
          &memory_artifact,
          kMaximumTargetMemoryArtifactBytes,
          "TARGET_MEMORY_ARTIFACT_TOO_LARGE",
          "The Target Memory Event Plan exceeds its byte limit.",
          "Keep the canonical Memory Event Plan within 128 KiB.")) {
    return false;
  }
  contract->target_memory_event_plan_sha256 = memory_artifact.sha256;
  ValidatedTargetMemoryPlan memory;
  if (!ValidateTargetMemoryPlan(
          memory_artifact.document, *contract, contract, &memory)) {
    return false;
  }
  contract->target_memory_event_plan_ready = true;
  contract->target_memory_evidence_level = memory.evidence_level;
  contract->target_memory_field_readiness = memory.field_readiness;
  contract->target_memory_symbolic = memory.symbolic;
  contract->target_memory_materialized = memory.materialized;
  if (memory.materialized) {
    contract->target_precision_policy_sha256 =
        memory.binding_digests.at("precision");
    contract->target_optimizer_policy_sha256 =
        memory.binding_digests.at("optimizer");
    contract->target_placement_sha256 = memory.binding_digests.at("placement");
    contract->target_recomputation_policy_sha256 =
        memory.binding_digests.at("recomputation");
    contract->target_runtime_profile_sha256 =
        memory.binding_digests.at("runtime");
    contract->target_memory_parameters_B =
        memory.component_bytes.at("PARAMETERS");
    contract->target_memory_gradients_B =
        memory.component_bytes.at("GRADIENTS");
    contract->target_memory_optimizer_states_B =
        memory.component_bytes.at("OPTIMIZER_STATES");
    contract->target_memory_activations_B =
        memory.component_bytes.at("ACTIVATIONS");
    contract->target_memory_communication_buffers_B =
        memory.component_bytes.at("COMMUNICATION_BUFFERS");
    contract->target_memory_expert_placement_B =
        memory.component_bytes.at("EXPERT_PLACEMENT");
    contract->target_memory_recomputation_B =
        memory.component_bytes.at("RECOMPUTATION");
    contract->target_memory_peak_B = memory.peak_bytes;
    contract->target_memory_base_hbm_B = memory.base_hbm_bytes;
    contract->target_memory_reserve_hbm_B = memory.reserve_hbm_bytes;
    contract->target_memory_scenario_usable_hbm_B = memory.usable_hbm_bytes;
    contract->target_memory_search_limit_B = memory.search_limit_bytes;
    contract->target_memory_execution_peak_known = memory.execution_peak_known;
    contract->target_memory_execution_peak_B = memory.execution_peak_bytes;
    contract->target_memory_execution_boundary_B =
        memory.execution_boundary_bytes;
    contract->target_memory_execution_maximum_accepted_B =
        memory.execution_maximum_accepted_bytes;
    contract->target_memory_search_gate = memory.search_gate;
    contract->target_memory_execution_gate = memory.execution_gate;
  }
  return true;
}

bool ValidateTargetWorkloadComposition(
    const JsonValue& target_workload,
    AnalyticalRunContract* contract) {
  std::string schema_version;
  std::string composition;
  std::string declared_composite;
  if (!StringMember(target_workload, "schema_version", &schema_version) ||
      schema_version != "simai.target.workload/v1" ||
      !StringMember(target_workload, "composition", &composition) ||
      composition != "SHA256_NEWLINE_DELIMITED_RESOURCE_DIGESTS_V1" ||
      !StringMember(target_workload, "sha256", &declared_composite) ||
      !IsSha256Identifier(declared_composite)) {
    Reject(
        contract,
        "TARGET_WORKLOAD_SCHEMA_INVALID",
        "The Target Workload composition envelope is invalid.",
        "Use simai.target.workload/v1 and the declared digest algorithm.");
    return false;
  }
  const std::string digest_input =
      contract->target_model_sha256 + "\n" +
      contract->target_step_sha256 + "\n" +
      contract->target_routing_sha256 + "\n" +
      contract->target_memory_event_plan_sha256;
  const std::string computed_composite = "sha256:" + Sha256Hex(digest_input);
  contract->target_workload_sha256 = computed_composite;
  if (declared_composite != computed_composite) {
    Reject(
        contract,
        "TARGET_WORKLOAD_DIGEST_MISMATCH",
        "The Target Workload composite digest is inconsistent.",
        "Hash model, step, routing, and memory digests in canonical order.");
    return false;
  }
  std::istringstream workload_input(contract->workload_snapshot);
  std::string header;
  std::string layer_count_line;
  if (!workload_input || !std::getline(workload_input, header) ||
      !std::getline(workload_input, layer_count_line)) {
    Reject(
        contract,
        "TARGET_AICB_BINDING_INVALID",
        "The AICB workload header cannot be decoded.",
        "Provide a complete target-bound AICB workload header.");
    return false;
  }

  AstraSim::WorkloadLayerRecordFormat record_format;
  AstraSim::TargetWorkloadEventBinding header_binding;
  if (!AstraSim::DecodeWorkloadHeader(
          header, &record_format, &header_binding)) {
    Reject(
        contract,
        "TARGET_AICB_BINDING_INVALID",
        "The AICB target binding is malformed or incomplete.",
        "Bind all four resource digests and the composite digest.");
    return false;
  }
  if (!header_binding.present) {
    Reject(
        contract,
        "TARGET_AICB_BINDING_MISSING",
        "The AICB workload has no Target Workload binding.",
        "Generate the workload with all target digest metadata.");
    return false;
  }
  AstraSim::TargetWorkloadEventBinding expected_binding;
  expected_binding.present = true;
  expected_binding.model_sha256 = contract->target_model_sha256;
  expected_binding.step_sha256 = contract->target_step_sha256;
  expected_binding.routing_sha256 = contract->target_routing_sha256;
  expected_binding.memory_event_plan_sha256 =
      contract->target_memory_event_plan_sha256;
  expected_binding.target_workload_sha256 = computed_composite;
  if (!AstraSim::TargetWorkloadBindingsEqual(
          header_binding, expected_binding)) {
    Reject(
        contract,
        "TARGET_AICB_BINDING_MISMATCH",
        "The AICB header is bound to different Target Workload resources.",
        "Regenerate the workload from the selected four resources.");
    return false;
  }

  std::istringstream count_input(layer_count_line);
  int layer_count = 0;
  std::string count_suffix;
  if (!(count_input >> layer_count) || layer_count < 1 ||
      (count_input >> count_suffix)) {
    Reject(
        contract,
        "TARGET_AICB_EVENT_BINDING_MISSING",
        "The AICB workload has no decodable target-bound events.",
        "Provide a positive exact layer count and bound event records.");
    return false;
  }
  for (int layer = 0; layer < layer_count; ++layer) {
    std::string layer_line;
    if (!std::getline(workload_input, layer_line)) {
      Reject(
          contract,
          "TARGET_AICB_EVENT_BINDING_MISSING",
          "A declared AICB event record is missing.",
          "Emit one fully bound event record per declared layer.");
      return false;
    }
    std::istringstream layer_input(layer_line);
    AstraSim::DecodedWorkloadLayerRecord record;
    std::string suffix;
    if (!AstraSim::DecodeWorkloadLayerRecord(
            layer_input, record_format, &record) ||
        (layer_input >> suffix)) {
      Reject(
          contract,
          "TARGET_AICB_EVENT_BINDING_MISSING",
          "An AICB event lacks an exact five-digest target binding.",
          "Propagate the target binding to every AICB event.");
      return false;
    }
    if (record_format ==
            AstraSim::WorkloadLayerRecordFormat::
                HybridCustomizedTargetBound18 &&
        AstraSim::DecodeWorkloadParallelismPolicyToken(
            record.specific_parallelism) ==
            AstraSim::WorkloadParallelismPolicyToken::Invalid) {
      Reject(
          contract,
          "TARGET_AICB_SPECIFIC_PARALLELISM_INVALID",
          "A target-bound customized event has an unknown parallelism policy.",
          "Use a controlled AICB specific_parallelism policy token.");
      return false;
    }
    if (!AstraSim::TargetWorkloadBindingsEqual(
            record.target_binding, expected_binding)) {
      Reject(
          contract,
          "TARGET_AICB_EVENT_BINDING_MISMATCH",
          "An AICB event is bound to different Target Workload resources.",
          "Propagate the same verified binding to every AICB event.");
      return false;
    }
  }
  std::string trailing_line;
  while (std::getline(workload_input, trailing_line)) {
    if (trailing_line.find_first_not_of(" \t\r") != std::string::npos) {
      Reject(
          contract,
          "TARGET_AICB_EVENT_BINDING_MISSING",
          "The AICB workload contains undeclared event records.",
          "Make the layer count cover every target-bound event.");
      return false;
    }
  }
  contract->target_workload_ready = true;
  return true;
}

void RejectAccuracyExecution(
    AnalyticalRunContract* contract,
    const std::string& reject_code,
    const std::string& message,
    const std::string& remediation) {
  contract->accepted = false;
  contract->exit_code = 5;
  contract->status = "INVALID_ACCURACY_EXECUTION";
  contract->reject_code = reject_code;
  contract->message = message;
  contract->remediation = remediation;
}

bool ApplyTargetMemoryGates(AnalyticalRunContract* contract) {
  if (!contract->target_memory_materialized) {
    return true;
  }
  if (contract->target_memory_search_gate == "FAIL") {
    contract->target_memory_gate_failed = true;
    Reject(
        contract,
        "HBM_SEARCH_LIMIT_EXCEEDED",
        "The planned peak exceeds 95 percent of Scenario Usable HBM.",
        "Reduce component peaks or provide a larger explicit usable budget.");
    return false;
  }
  if (contract->target_memory_execution_gate ==
      "INVALID_ACCURACY_EXECUTION") {
    contract->target_memory_gate_failed = true;
    RejectAccuracyExecution(
        contract,
        "HBM_EXECUTION_LIMIT_REACHED",
        "The observed A2/A3 peak is not strictly below 85 percent of base HBM.",
        "Lower execution occupancy before admitting this accuracy sample.");
    return false;
  }
  return true;
}

bool ParsePositiveUint64Array(
    const JsonValue& object,
    const std::string& key,
    std::vector<uint64_t>* values) {
  const JsonValue* array = Member(object, key);
  if (array == nullptr || array->type != JsonValue::Type::Array ||
      array->array.empty()) {
    return false;
  }
  values->clear();
  for (const JsonValue& element : array->array) {
    uint64_t value = 0U;
    if (!ExactUnsignedDecimal(
            element, 9007199254740991ULL, false, &value)) {
      return false;
    }
    values->push_back(value);
  }
  return true;
}

bool GroundTruthRunHasFrozenContract(
    const JsonValue& root,
    AnalyticalRunContract* contract) {
  const JsonValue* metadata = Member(root, "metadata");
  const JsonValue* spec = Member(root, "spec");
  const JsonValue* source = spec == nullptr
      ? nullptr
      : Member(*spec, "sourceIdentity");
  const JsonValue* runtime = spec == nullptr
      ? nullptr
      : Member(*spec, "runtimeIdentity");
  const JsonValue* model = spec == nullptr ? nullptr : Member(*spec, "model");
  const JsonValue* parallel = spec == nullptr
      ? nullptr
      : Member(*spec, "parallelism");
  const JsonValue* topology = spec == nullptr
      ? nullptr
      : Member(*spec, "rankTopology");
  const JsonValue* metrics = spec == nullptr
      ? nullptr
      : Member(*spec, "metricContract");
  const JsonValue* scenarios = spec == nullptr
      ? nullptr
      : Member(*spec, "scenarios");
  const JsonValue* bindings = spec == nullptr
      ? nullptr
      : Member(*spec, "bindings");
  const JsonValue* evidence = spec == nullptr
      ? nullptr
      : Member(*spec, "evidence");
  std::string api_version;
  std::string kind;
  std::string id;
  std::string generation;
  bool frozen = false;
  std::string llm_commit;
  std::string mindspeed_commit;
  std::string megatron_commit;
  std::string python_version;
  std::string pytorch_version;
  std::string torch_npu_version;
  std::string cann_version;
  std::string driver_version;
  std::string abi_digest;
  int active_layers = 0;
  int experts = 0;
  int top_k = 0;
  int expert_width = 0;
  int shared_experts = 0;
  int mbs = 0;
  int tp = 0;
  int pp = 0;
  int ep = 0;
  int dp = 0;
  int world_size = 0;
  int ranks_per_device = 0;
  std::string rank_mapping_digest;
  std::string step_unit;
  std::string hbm_unit;
  std::string hccl_unit;
  bool warmup_excluded = false;
  int initial_samples = 0;
  int extended_samples = 0;
  double cv_threshold = 0.0;
  std::string high_variation_statistic;
  int hbm_percent = 0;
  std::string hbm_comparison;
  std::string evidence_ref;
  std::string evidence_id;
  std::string evidence_class;
  std::string evidence_readiness;
  bool evidence_hardware_available = false;
  A2SemanticBindings semantic_bindings;
  if (!ObjectHasExactKeys(root, {"apiVersion", "kind", "metadata", "spec"}) ||
      !StringMember(root, "apiVersion", &api_version) ||
      api_version != "simai.ground-truth/v1" ||
      !StringMember(root, "kind", &kind) || kind != "GroundTruthRun" ||
      metadata == nullptr ||
      !ObjectHasExactKeys(*metadata, {"id", "generation", "frozen"}) ||
      !StringMember(*metadata, "id", &id) || id.empty() ||
      !StringMember(*metadata, "generation", &generation) ||
      generation != "A2" || !BooleanMember(*metadata, "frozen", &frozen) ||
      !frozen || spec == nullptr ||
      !ObjectHasExactKeys(
          *spec,
          {"sourceIdentity", "runtimeIdentity", "model", "parallelism",
           "rankTopology", "bindings", "metricContract", "scenarios",
           "evidenceRef", "evidence"}) ||
      source == nullptr ||
      !ObjectHasExactKeys(
          *source,
          {"mindSpeedLlmCommit", "mindSpeedCommit", "megatronLmCommit"}) ||
      !StringMember(*source, "mindSpeedLlmCommit", &llm_commit) ||
      llm_commit != "2b7130ca7bea7083a91ed66812eec95067d057a2" ||
      !StringMember(*source, "mindSpeedCommit", &mindspeed_commit) ||
      mindspeed_commit != "81570f17ee091e783fa68428c04fa536da122dc1" ||
      !StringMember(*source, "megatronLmCommit", &megatron_commit) ||
      megatron_commit != "a845aa7e12b3a117e24c2352b9e3e60bad2e3a17" ||
      runtime == nullptr ||
      !ObjectHasExactKeys(
          *runtime,
          {"pythonVersion", "pytorchVersion", "torchNpuVersion",
           "cannVersion", "driverVersion", "abiDigest"}) ||
      !StringMember(*runtime, "pythonVersion", &python_version) ||
      python_version != "3.10" ||
      !StringMember(*runtime, "pytorchVersion", &pytorch_version) ||
      pytorch_version != "2.7.1" ||
      !StringMember(*runtime, "torchNpuVersion", &torch_npu_version) ||
      torch_npu_version != "7.3.0" ||
      !StringMember(*runtime, "cannVersion", &cann_version) ||
      cann_version != "8.5.0" ||
      !StringMember(*runtime, "driverVersion", &driver_version) ||
      driver_version.empty() ||
      !StringMember(*runtime, "abiDigest", &abi_digest) ||
      !IsSha256Identifier(abi_digest) || model == nullptr ||
      !ObjectHasExactKeys(
          *model,
          {"activeTransformerLayers", "routedExperts", "topK",
           "expertIntermediateSize", "sharedExperts", "microBatchSequences"}) ||
      !ExactPositiveIntMember(*model, "activeTransformerLayers", &active_layers) ||
      active_layers != 4 ||
      !ExactPositiveIntMember(*model, "routedExperts", &experts) ||
      experts != 32 || !ExactPositiveIntMember(*model, "topK", &top_k) ||
      top_k != 16 ||
      !ExactPositiveIntMember(*model, "expertIntermediateSize", &expert_width) ||
      expert_width != 3072 ||
      !ExactPositiveIntMember(*model, "sharedExperts", &shared_experts) ||
      shared_experts != 1 ||
      !ExactPositiveIntMember(*model, "microBatchSequences", &mbs) || mbs != 1 ||
      parallel == nullptr ||
      !ObjectHasExactKeys(*parallel, {"tensor", "pipeline", "expert", "data"}) ||
      !ExactPositiveIntMember(*parallel, "tensor", &tp) || tp != 1 ||
      !ExactPositiveIntMember(*parallel, "pipeline", &pp) || pp != 2 ||
      !ExactPositiveIntMember(*parallel, "expert", &ep) || ep != 4 ||
      !ExactPositiveIntMember(*parallel, "data", &dp) || dp != 4 ||
      topology == nullptr ||
      !ObjectHasExactKeys(
          *topology,
          {"worldSize", "trainingRanksPerDevice", "rankMappingDigest"}) ||
      !ExactPositiveIntMember(*topology, "worldSize", &world_size) ||
      world_size != 8 ||
      !ExactPositiveIntMember(
          *topology, "trainingRanksPerDevice", &ranks_per_device) ||
      ranks_per_device != 1 ||
      !StringMember(*topology, "rankMappingDigest", &rank_mapping_digest) ||
      !IsSha256Identifier(rank_mapping_digest) || metrics == nullptr ||
      !ObjectHasExactKeys(
          *metrics,
          {"stepTimeUnit", "peakHbmUnit", "hcclTimeUnit", "warmupExcluded",
           "initialSampleCount", "extendedSampleCount", "cvThreshold",
           "highVariationStatistic", "hbmSafetyPercent", "hbmComparison"}) ||
      !StringMember(*metrics, "stepTimeUnit", &step_unit) || step_unit != "ns" ||
      !StringMember(*metrics, "peakHbmUnit", &hbm_unit) || hbm_unit != "B" ||
      !StringMember(*metrics, "hcclTimeUnit", &hccl_unit) || hccl_unit != "ns" ||
      !BooleanMember(*metrics, "warmupExcluded", &warmup_excluded) ||
      !warmup_excluded ||
      !ExactPositiveIntMember(*metrics, "initialSampleCount", &initial_samples) ||
      initial_samples != 5 ||
      !ExactPositiveIntMember(*metrics, "extendedSampleCount", &extended_samples) ||
      extended_samples != 10 ||
      !NumberMember(*metrics, "cvThreshold", &cv_threshold) ||
      std::fabs(cv_threshold - 0.1) > 1e-12 ||
      !StringMember(
          *metrics, "highVariationStatistic", &high_variation_statistic) ||
      high_variation_statistic != "LINEAR_TYPE7_P90" ||
      !ExactPositiveIntMember(*metrics, "hbmSafetyPercent", &hbm_percent) ||
      hbm_percent != 85 ||
      !StringMember(*metrics, "hbmComparison", &hbm_comparison) ||
      hbm_comparison != "STRICTLY_LESS_THAN" || scenarios == nullptr ||
      scenarios->type != JsonValue::Type::Array || scenarios->array.size() != 3U ||
      bindings == nullptr ||
      !ParseA2SemanticBindings(*bindings, &semantic_bindings) ||
      semantic_bindings.world_size != world_size ||
      semantic_bindings.topology_sha256 != rank_mapping_digest ||
      evidence == nullptr ||
      !StringMember(*spec, "evidenceRef", &evidence_ref) ||
      !A2EvidenceRecordIsExact(
          *evidence,
          &evidence_id,
          &evidence_class,
          &evidence_readiness,
          &evidence_hardware_available) ||
      evidence_ref != evidence_id || evidence_class != "USER_INPUT") {
    Reject(
        contract,
        "A2_GROUND_TRUTH_RUN_INVALID",
        "The A2 GroundTruth Run does not match the frozen source, runtime, shape, topology, or metric contract.",
        "Use the exact A2 reduced-MoE GroundTruth Run v1 contract.");
    return false;
  }
  contract->a2_run_evidence_level = evidence_class;
  contract->a2_run_field_readiness = evidence_readiness;
  contract->a2_run_evidence_ref = evidence_ref;
  contract->a2_run_hardware_available = evidence_hardware_available;
  contract->a2_bound_profile_id = semantic_bindings.profile_id;
  contract->a2_bound_profile_sha256 = semantic_bindings.profile_sha256;
  contract->a2_bound_profile_evidence_level =
      semantic_bindings.profile_evidence_class;
  contract->a2_bound_profile_field_readiness =
      semantic_bindings.profile_readiness;
  contract->a2_bound_profile_evidence_ref =
      semantic_bindings.profile_evidence_ref;
  contract->a2_bound_workload_sha256 = semantic_bindings.workload_sha256;
  contract->a2_bound_topology_sha256 = semantic_bindings.topology_sha256;
  contract->a2_bound_group_id = semantic_bindings.group_id;
  contract->a2_bound_group_type = semantic_bindings.group_type;
  contract->a2_bound_group_rank_count = semantic_bindings.group_rank_count;
  contract->a2_bound_group_members = semantic_bindings.group_members;
  contract->a2_bound_group_membership_sha256 =
      semantic_bindings.group_membership_sha256;
  const std::array<std::string, 3> ids = {{
      "A2-CAL-BALANCED", "A2-CAL-COMM", "A2-CAL-LONG"}};
  const std::array<uint64_t, 3> sequences = {{2048U, 1024U, 4096U}};
  const std::array<uint64_t, 3> batches = {{8U, 16U, 8U}};
  const std::array<uint64_t, 3> accumulation = {{2U, 4U, 2U}};
  const std::array<uint64_t, 3> tokens = {{16384U, 16384U, 32768U}};
  for (size_t index = 0U; index < ids.size(); ++index) {
    const JsonValue& scenario = scenarios->array[index];
    std::string scenario_id;
    uint64_t sequence = 0U;
    uint64_t batch = 0U;
    uint64_t ga = 0U;
    uint64_t gts = 0U;
    if (!ObjectHasExactKeys(
            scenario,
            {"id", "sequenceTokens", "globalBatchSequences",
             "gradientAccumulation", "configuredGlobalTokens"}) ||
        !StringMember(scenario, "id", &scenario_id) || scenario_id != ids[index] ||
        !ExactPositiveUint64Member(scenario, "sequenceTokens", &sequence) ||
        sequence != sequences[index] ||
        !ExactPositiveUint64Member(scenario, "globalBatchSequences", &batch) ||
        batch != batches[index] ||
        !ExactPositiveUint64Member(scenario, "gradientAccumulation", &ga) ||
        ga != accumulation[index] ||
        !ExactPositiveUint64Member(
            scenario, "configuredGlobalTokens", &gts) || gts != tokens[index] ||
        batch != static_cast<uint64_t>(mbs * dp) * ga || gts != sequence * batch) {
      Reject(
          contract,
          "A2_GROUND_TRUTH_SCENARIO_DRIFT",
          "An A2 calibration scenario differs from the frozen sequence/GBS/GA/GTS contract.",
          "Restore the three preregistered A2 calibration scenarios.");
      return false;
    }
  }
  return true;
}

bool ValidateA2GroundTruth(
    const JsonValue& envelope,
    const JsonValue& manifest_root,
    AnalyticalRunContract* contract) {
  std::string schema_version;
  const JsonValue* run_reference = Member(envelope, "run");
  const JsonValue* result_reference = Member(envelope, "result");
  if (!ObjectHasExactKeys(envelope, {"schema_version", "run", "result"}) ||
      !StringMember(envelope, "schema_version", &schema_version) ||
      schema_version != "simai.a2.calibration/v1" || run_reference == nullptr ||
      result_reference == nullptr) {
    Reject(
        contract,
        "A2_GROUND_TRUTH_ENVELOPE_INVALID",
        "The A2 GroundTruth envelope is invalid.",
        "Provide exact content-addressed GroundTruth Run and Result references.");
    return false;
  }
  if (!ObjectHasExactKeys(*run_reference, {"path", "sha256"})) {
    Reject(
        contract,
        "A2_GROUND_TRUTH_RUN_REFERENCE_INVALID",
        "The GroundTruth Run reference has unknown or missing fields.",
        "Provide exactly path and SHA-256.");
    return false;
  }
  if (!ObjectHasExactKeys(*result_reference, {"path", "sha256"})) {
    Reject(
        contract,
        "A2_GROUND_TRUTH_RESULT_REFERENCE_INVALID",
        "The GroundTruth Result reference has unknown or missing fields.",
        "Provide exactly path and SHA-256.");
    return false;
  }
  const ArtifactLoadPolicy run_policy = {
      "A2_GROUND_TRUTH_RUN_REFERENCE_INVALID",
      "The GroundTruth Run reference is invalid.",
      "Provide path and SHA-256.",
      "A2_GROUND_TRUTH_RUN_NOT_FOUND",
      "The GroundTruth Run cannot be read.",
      "Provide a readable sanitized GroundTruth Run.",
      "A2_GROUND_TRUTH_RUN_DIGEST_MISMATCH",
      "The GroundTruth Run digest does not match.",
      "Use the intended immutable GroundTruth Run.",
      "A2_GROUND_TRUTH_RUN_INVALID_JSON",
      "The GroundTruth Run is not valid JSON.",
      "Correct the GroundTruth Run JSON."};
  const ArtifactLoadPolicy result_policy = {
      "A2_GROUND_TRUTH_RESULT_REFERENCE_INVALID",
      "The GroundTruth Result reference is invalid.",
      "Provide path and SHA-256.",
      "A2_GROUND_TRUTH_RESULT_NOT_FOUND",
      "The GroundTruth Result cannot be read.",
      "Provide a readable sanitized GroundTruth Result.",
      "A2_GROUND_TRUTH_RESULT_DIGEST_MISMATCH",
      "The GroundTruth Result digest does not match.",
      "Use the intended immutable GroundTruth Result.",
      "A2_GROUND_TRUTH_RESULT_INVALID_JSON",
      "The GroundTruth Result is not valid JSON.",
      "Correct the GroundTruth Result JSON."};
  LoadedArtifact run;
  LoadedArtifact result;
  if (!LoadArtifact(
          *run_reference,
          run_policy,
          contract,
          &run,
          256U * 1024U,
          "A2_GROUND_TRUTH_RUN_TOO_LARGE",
          "The GroundTruth Run exceeds 256 KiB.",
          "Publish only the bounded sanitized contract.") ||
      !LoadArtifact(
          *result_reference,
          result_policy,
          contract,
          &result,
          512U * 1024U,
          "A2_GROUND_TRUTH_RESULT_TOO_LARGE",
          "The GroundTruth Result exceeds 512 KiB.",
          "Publish bounded aggregate samples instead of raw host logs.")) {
    return false;
  }
  contract->a2_ground_truth_run_sha256 = run.sha256;
  contract->a2_ground_truth_result_sha256 = result.sha256;
  if (!GroundTruthRunHasFrozenContract(run.document, contract)) {
    return false;
  }
  const JsonValue* metadata = Member(result.document, "metadata");
  const JsonValue* spec = Member(result.document, "spec");
  const JsonValue* block = spec == nullptr ? nullptr : Member(*spec, "block");
  const JsonValue* raw_observations = spec == nullptr
      ? nullptr
      : Member(*spec, "rawObservations");
  const JsonValue* derived = spec == nullptr
      ? nullptr
      : Member(*spec, "derivedCostModel");
  const JsonValue* scenarios = spec == nullptr
      ? nullptr
      : Member(*spec, "scenarios");
  const JsonValue* bindings = spec == nullptr
      ? nullptr
      : Member(*spec, "bindings");
  const JsonValue* evidence = spec == nullptr
      ? nullptr
      : Member(*spec, "evidence");
  std::string api_version;
  std::string kind;
  std::string result_id;
  std::string generation;
  std::string run_digest;
  std::string status;
  std::string block_reason;
  std::string block_remediation;
  std::string evidence_class;
  std::string evidence_readiness;
  std::string evidence_ref;
  std::string evidence_id;
  bool evidence_hardware_available = false;
  A2SemanticBindings result_bindings;
  A2SemanticBindings run_bindings;
  run_bindings.profile_id = contract->a2_bound_profile_id;
  run_bindings.profile_sha256 = contract->a2_bound_profile_sha256;
  run_bindings.profile_evidence_class =
      contract->a2_bound_profile_evidence_level;
  run_bindings.profile_readiness =
      contract->a2_bound_profile_field_readiness;
  run_bindings.profile_evidence_ref =
      contract->a2_bound_profile_evidence_ref;
  run_bindings.workload_sha256 = contract->a2_bound_workload_sha256;
  run_bindings.world_size = 8;
  run_bindings.topology_sha256 = contract->a2_bound_topology_sha256;
  run_bindings.group_id = contract->a2_bound_group_id;
  run_bindings.group_type = contract->a2_bound_group_type;
  run_bindings.group_rank_count = contract->a2_bound_group_rank_count;
  run_bindings.group_members = contract->a2_bound_group_members;
  run_bindings.group_membership_sha256 =
      contract->a2_bound_group_membership_sha256;
  if (!ObjectHasExactKeys(
          result.document, {"apiVersion", "kind", "metadata", "spec"}) ||
      !StringMember(result.document, "apiVersion", &api_version) ||
      api_version != "simai.ground-truth/v1" ||
      !StringMember(result.document, "kind", &kind) ||
      kind != "GroundTruthResult" || metadata == nullptr ||
      !ObjectHasExactKeys(*metadata, {"id", "generation"}) ||
      !StringMember(*metadata, "id", &result_id) || result_id.empty() ||
      !StringMember(*metadata, "generation", &generation) || generation != "A2" ||
      spec == nullptr ||
      !ObjectHasExactKeys(
          *spec,
          {"groundTruthRunDigest", "status", "block", "rawObservations",
           "derivedCostModel", "bindings", "scenarios", "evidenceRef",
           "evidence"}) ||
      !StringMember(*spec, "groundTruthRunDigest", &run_digest) ||
      run_digest != run.sha256 || !StringMember(*spec, "status", &status) ||
      block == nullptr ||
      !ObjectHasExactKeys(*block, {"reason", "remediation"}) ||
      !StringMember(*block, "reason", &block_reason) ||
      !StringMember(*block, "remediation", &block_remediation) ||
      raw_observations == nullptr ||
      raw_observations->type != JsonValue::Type::Array || derived == nullptr ||
      scenarios == nullptr || scenarios->type != JsonValue::Type::Array ||
      bindings == nullptr ||
      !ParseA2SemanticBindings(*bindings, &result_bindings) ||
      !A2BindingsEqual(run_bindings, result_bindings) || evidence == nullptr ||
      !StringMember(*spec, "evidenceRef", &evidence_ref) ||
      !A2EvidenceRecordIsExact(
          *evidence,
          &evidence_id,
          &evidence_class,
          &evidence_readiness,
          &evidence_hardware_available) ||
      evidence_ref != evidence_id ||
      (evidence_class != "USER_INPUT" && evidence_class != "MEASURED")) {
    Reject(
        contract,
        "A2_GROUND_TRUTH_RESULT_INVALID",
        "The A2 GroundTruth Result schema or provenance is invalid.",
        "Provide a complete sanitized GroundTruth Result bound to its Run.");
    return false;
  }
  contract->a2_ground_truth_status = status;
  contract->a2_ground_truth_evidence_level = evidence_class;
  contract->a2_ground_truth_field_readiness = evidence_readiness;
  contract->a2_result_evidence_ref = evidence_ref;
  contract->a2_result_hardware_available = evidence_hardware_available;
  if (status == "BLOCKED_ENV") {
    const bool supported_block =
        (block_reason == "A2_HCCL_ABI_UNAVAILABLE" &&
         block_remediation ==
             "Install the pinned CANN 8.5 HCCL runtime in the isolated lane.") ||
        (block_reason == "A2_TORCH_NPU_IMPORT_FAILED" &&
         block_remediation ==
             "Install the pinned Python 3.10, PyTorch 2.7.1, and TorchNPU 7.3.0 lane.") ||
        (block_reason == "A2_SOURCE_PIN_UNAVAILABLE" &&
         block_remediation ==
             "Provide clean checkouts of the three pinned training source revisions.") ||
        (block_reason == "A2_RANK_TOPOLOGY_UNAVAILABLE" &&
         block_remediation ==
             "Restore eight complete A2 training ranks before sampling.");
    if (!supported_block || !raw_observations->array.empty() ||
        derived->type != JsonValue::Type::Null ||
        !scenarios->array.empty()) {
      Reject(
          contract,
          "A2_BLOCKED_ENV_RESULT_INVALID",
          "A BLOCKED_ENV result must use a controlled reason, actionable remediation, and contain no observations or fitted model.",
          "Remove samples and the model, then use the minimum remediation for the observed environment failure.");
      return false;
    }
    contract->accepted = false;
    contract->exit_code = 6;
    contract->status = "BLOCKED_ENV";
    contract->reject_code = block_reason.empty()
        ? "A2_ENVIRONMENT_BLOCKED"
        : block_reason;
    contract->message = "The pinned A2 environment could not be established.";
    contract->remediation = block_remediation;
    return false;
  }
  if (status == "INVALID_ACCURACY_EXECUTION") {
    const bool supported_subtype =
        block_reason == "A2_OOM" ||
        block_reason == "A2_HBM_LIMIT_REACHED" ||
        block_reason == "A2_RANK_LOSS" ||
        block_reason == "A2_NON_FINITE" ||
        block_reason == "A2_TOKEN_LOSS" ||
        block_reason == "A2_TOKEN_REPLAY" ||
        block_reason == "A2_PROVENANCE_DRIFT";
    if (!supported_subtype ||
        block_remediation !=
            "Correct the execution and repeat the unchanged frozen scenario." ||
        !raw_observations->array.empty() ||
        derived->type != JsonValue::Type::Null ||
        !scenarios->array.empty()) {
      Reject(
          contract,
          "A2_INVALID_ACCURACY_RESULT_INVALID",
          "An INVALID_ACCURACY_EXECUTION result must preserve one controlled subtype and contain no fitted data.",
          "Use one frozen execution subtype and remove raw observations, scenarios, and model.");
      return false;
    }
    RejectAccuracyExecution(
        contract,
        block_reason,
        "The A2 GroundTruth Result is not eligible for calibration.",
        block_remediation);
    return false;
  }
  if (status != "VALID" || block_reason != "NONE" ||
      block_remediation != "NONE") {
    Reject(
        contract,
        "A2_GROUND_TRUTH_RESULT_INVALID",
        "The A2 GroundTruth Result discriminated state is invalid.",
        "Use VALID, BLOCKED_ENV, or INVALID_ACCURACY_EXECUTION with its exact payload.");
    return false;
  }
  if (raw_observations->array.empty() ||
      derived->type != JsonValue::Type::Object) {
    Reject(
        contract,
        "A2_GROUND_TRUTH_RESULT_INVALID",
        "A valid A2 GroundTruth Result requires immutable observations and a DerivedCostModel.",
        "Provide the complete observation set and its fitted model.");
    return false;
  }
  std::vector<std::string> raw_digests;
  for (const JsonValue& raw_reference : raw_observations->array) {
    std::string raw_path;
    std::string raw_digest;
    std::string raw_content;
    if (!ObjectHasExactKeys(raw_reference, {"path", "sha256"}) ||
        !ParseArtifactReference(raw_reference, &raw_path, &raw_digest) ||
        ReadFileWithMaximumBytes(raw_path, 256U * 1024U, &raw_content) !=
            BoundedReadResult::Success) {
      Reject(
          contract,
          "A2_RAW_OBSERVATION_INVALID",
          "An A2 RawObservation reference is invalid or unreadable.",
          "Provide bounded immutable RawObservation artifacts.");
      return false;
    }
    if ("sha256:" + Sha256Hex(raw_content) != raw_digest) {
      Reject(
          contract,
          "A2_RAW_OBSERVATION_DIGEST_MISMATCH",
          "An A2 RawObservation digest does not match.",
          "Use the immutable observation consumed by the DerivedCostModel.");
      return false;
    }
    raw_digests.push_back(raw_digest);
  }
  std::string derived_path;
  std::string derived_digest;
  const JsonValue* manifest_model_reference =
      Member(manifest_root, "collective_cost_model");
  std::string manifest_model_path;
  std::string manifest_model_digest;
  if (!ObjectHasExactKeys(*derived, {"path", "sha256"}) ||
      !ParseArtifactReference(*derived, &derived_path, &derived_digest) ||
      manifest_model_reference == nullptr ||
      !ParseArtifactReference(
          *manifest_model_reference,
          &manifest_model_path,
          &manifest_model_digest) ||
      derived_digest != manifest_model_digest) {
    Reject(
        contract,
        "A2_DERIVED_MODEL_BINDING_MISMATCH",
        "The A2 DerivedCostModel is not the model selected by this Analytical run.",
        "Select the exact model derived from the declared RawObservations.");
    return false;
  }
  std::string model_content;
  JsonValue model_document;
  if (ReadFileWithMaximumBytes(
          derived_path, 512U * 1024U, &model_content) !=
          BoundedReadResult::Success ||
      "sha256:" + Sha256Hex(model_content) != derived_digest ||
      !ParseJsonDocument(model_content, &model_document)) {
    Reject(
        contract,
        "A2_DERIVED_MODEL_INVALID",
        "The A2 DerivedCostModel cannot be verified.",
        "Provide the bounded model matching its declared digest.");
    return false;
  }
  const JsonValue* model_spec = Member(model_document, "spec");
  const JsonValue* input_samples = model_spec == nullptr
      ? nullptr
      : Member(*model_spec, "inputSamples");
  if (input_samples == nullptr || input_samples->type != JsonValue::Type::Array ||
      input_samples->array.size() != raw_digests.size()) {
    Reject(
        contract,
        "A2_DERIVED_MODEL_RAW_SET_MISMATCH",
        "The DerivedCostModel raw input set differs from the GroundTruth Result.",
        "Bind every and only the declared immutable RawObservations.");
    return false;
  }
  for (size_t index = 0U; index < raw_digests.size(); ++index) {
    std::string sample_digest;
    if (!StringMember(input_samples->array[index], "sha256", &sample_digest) ||
        sample_digest != raw_digests[index]) {
      Reject(
          contract,
          "A2_DERIVED_MODEL_RAW_SET_MISMATCH",
          "The DerivedCostModel raw input digest differs from the GroundTruth Result.",
          "Regenerate the model from the exact observation set.");
      return false;
    }
  }
  const std::array<std::string, 3> ids = {{
      "A2-CAL-BALANCED", "A2-CAL-COMM", "A2-CAL-LONG"}};
  const std::array<uint64_t, 3> configured_tokens = {{16384U, 16384U, 32768U}};
  if (scenarios->array.size() != ids.size()) {
    Reject(
        contract,
        "A2_GROUND_TRUTH_SCENARIO_SET_INVALID",
        "The GroundTruth Result must contain exactly three frozen scenarios.",
        "Report balanced, communication-heavy, and long-sequence exactly once.");
    return false;
  }
  contract->a2_scenarios.clear();
  for (size_t index = 0U; index < ids.size(); ++index) {
    const JsonValue& scenario = scenarios->array[index];
    std::string id;
    std::string execution_status;
    std::vector<uint64_t> step_times;
    std::vector<uint64_t> hbm_peaks;
    uint64_t base_hbm = 0U;
    int completed_ranks = 0;
    int expected_ranks = 0;
    bool loss_finite = false;
    bool gradients_finite = false;
    bool oom = false;
    uint64_t configured = 0U;
    uint64_t consumed = 0U;
    uint64_t dropped = 0U;
    uint64_t replayed = 0U;
    std::string provenance_digest;
    if (!ObjectHasExactKeys(
            scenario,
            {"id", "status", "stepTimeNs", "peakHbmB", "baseHbmB",
             "completedRanks", "expectedRanks", "lossFinite",
             "gradientsFinite", "oom", "configuredTokens", "consumedTokens",
             "droppedTokens", "replayedTokens", "provenanceDigest"}) ||
        !StringMember(scenario, "id", &id) || id != ids[index] ||
        !StringMember(scenario, "status", &execution_status) ||
        execution_status != "VALID" ||
        !ParsePositiveUint64Array(scenario, "stepTimeNs", &step_times) ||
        !ParsePositiveUint64Array(scenario, "peakHbmB", &hbm_peaks) ||
        hbm_peaks.size() != step_times.size() ||
        !ExactPositiveUint64Member(scenario, "baseHbmB", &base_hbm) ||
        !ExactPositiveIntMember(scenario, "completedRanks", &completed_ranks) ||
        !ExactPositiveIntMember(scenario, "expectedRanks", &expected_ranks) ||
        !BooleanMember(scenario, "lossFinite", &loss_finite) ||
        !BooleanMember(scenario, "gradientsFinite", &gradients_finite) ||
        !BooleanMember(scenario, "oom", &oom) ||
        !ExactPositiveUint64Member(scenario, "configuredTokens", &configured) ||
        !ExactPositiveUint64Member(scenario, "consumedTokens", &consumed) ||
        !ExactNonNegativeUint64Member(scenario, "droppedTokens", &dropped) ||
        !ExactNonNegativeUint64Member(scenario, "replayedTokens", &replayed) ||
        !StringMember(scenario, "provenanceDigest", &provenance_digest)) {
      Reject(
          contract,
          "A2_GROUND_TRUTH_SCENARIO_INVALID",
          "An A2 scenario is incomplete or malformed.",
          "Provide exact typed fields for the frozen scenario.");
      return false;
    }
    A2GroundTruthScenarioInput scenario_input;
    scenario_input.id = id;
    scenario_input.step_time_ns = step_times;
    scenario_input.peak_hbm_B = hbm_peaks;
    scenario_input.base_hbm_B = base_hbm;
    scenario_input.completed_ranks = completed_ranks;
    scenario_input.expected_ranks = expected_ranks;
    scenario_input.loss_finite = loss_finite;
    scenario_input.gradients_finite = gradients_finite;
    scenario_input.oom = oom;
    scenario_input.configured_tokens = configured;
    scenario_input.consumed_tokens = consumed;
    scenario_input.dropped_tokens = dropped;
    scenario_input.replayed_tokens = replayed;
    scenario_input.provenance_digest = provenance_digest;
    const A2GroundTruthScenarioValidation validation =
        ValidateA2GroundTruthScenario(
            scenario_input, configured_tokens[index], run.sha256);
    if (validation.classification ==
        A2GroundTruthValidationClass::InvalidAccuracyExecution) {
      RejectAccuracyExecution(
          contract,
          validation.reject_code,
          validation.message,
          validation.remediation);
      return false;
    }
    if (validation.classification ==
        A2GroundTruthValidationClass::InvalidInput) {
      Reject(
          contract,
          validation.reject_code,
          validation.message,
          validation.remediation);
      return false;
    }
    contract->a2_scenarios.push_back(validation.summary);
  }
  bool hardware_available = false;
  const JsonValue* conditions = Member(*evidence, "conditions");
  if (conditions != nullptr) {
    BooleanMember(*conditions, "hardwareAvailable", &hardware_available);
  }
  contract->a2_calibration_eligible =
      contract->a2_run_evidence_level == "USER_INPUT" &&
      contract->a2_run_field_readiness == "FIELD_VERIFIED" &&
      contract->a2_run_hardware_available &&
      evidence_class == "MEASURED" &&
      evidence_readiness == "FIELD_VERIFIED" && hardware_available;
  contract->a2_derived_cost_model_sha256 = derived_digest;
  contract->a2_raw_observation_count = static_cast<int>(raw_digests.size());
  contract->a2_ground_truth_ready = true;
  return true;
}

struct ValidatedAscendProfile {
  std::string id;
  int rank_count = 0;
  std::string topology_domain;
  std::string topology_digest;
  std::string evidence_level;
  std::string field_readiness;
  std::string evidence_ref;
  bool hardware_available = false;
};

bool ValidateAscendProfile(
    const JsonValue& profile,
    AnalyticalRunContract* contract,
    ValidatedAscendProfile* validated) {
  if (!UnitsAreCanonical(profile)) {
    Reject(
        contract,
        "DEVICE_PROFILE_UNIT_INVALID",
        "The Ascend Profile contains a noncanonical unit.",
        "Use only B, B/s, FLOP/s, ns, or count for consumed values.");
    return false;
  }

  std::string api_version;
  std::string kind;
  std::string schema_semver;
  const JsonValue* metadata = Member(profile, "metadata");
  const JsonValue* spec = Member(profile, "spec");
  const JsonValue* identity =
      spec == nullptr ? nullptr : Member(*spec, "identity");
  const JsonValue* physical_chip_count =
      identity == nullptr ? nullptr : Member(*identity, "physicalChipCount");
  const JsonValue* management_device_count =
      identity == nullptr ? nullptr : Member(*identity, "managementDeviceCount");
  const JsonValue* rank_granularity =
      spec == nullptr ? nullptr : Member(*spec, "rankGranularity");
  const JsonValue* ranks_per_unit = rank_granularity == nullptr
      ? nullptr
      : Member(*rank_granularity, "trainingRanksPerUnit");
  const JsonValue* compute =
      spec == nullptr ? nullptr : Member(*spec, "compute");
  const JsonValue* compute_capability =
      compute == nullptr ? nullptr : FirstArrayObject(*compute, "capabilities");
  const JsonValue* peak_flops = compute_capability == nullptr
      ? nullptr
      : Member(*compute_capability, "peakFLOPsPerS");
  const JsonValue* memory =
      spec == nullptr ? nullptr : Member(*spec, "memory");
  const JsonValue* hbm = memory == nullptr ? nullptr : Member(*memory, "hbm");
  const JsonValue* hbm_capacity =
      hbm == nullptr ? nullptr : Member(*hbm, "installedCapacity");
  const JsonValue* hbm_bandwidth =
      hbm == nullptr ? nullptr : FirstArrayObject(*hbm, "bandwidth");
  const JsonValue* topology =
      spec == nullptr ? nullptr : Member(*spec, "topology");
  const JsonValue* topology_level =
      topology == nullptr ? nullptr : FirstArrayObject(*topology, "levels");

  if (spec != nullptr && topology_level == nullptr) {
    RejectUnsupported(
        contract,
        "HCCL_TOPOLOGY_REQUIRED",
        "Ascend Analytical requires an explicit topology domain.",
        "Provide Profile spec.topology.levels with rank count and digest.");
    return false;
  }

  std::string status;
  std::string vendor;
  std::string generation;
  std::string rank_unit;
  std::string physical_unit;
  std::string management_unit;
  std::string ranks_per_unit_unit;
  std::string peak_flops_unit;
  std::string hbm_capacity_unit;
  std::string hbm_bandwidth_unit;
  uint64_t physical_count = 0;
  uint64_t management_count = 0;
  uint64_t ranks_per_unit_count = 0;
  uint64_t peak_flops_per_s = 0;
  uint64_t hbm_capacity_bytes = 0;
  double hbm_bandwidth_Bps = 0.0;
  if (!StringMember(profile, "apiVersion", &api_version) ||
      api_version != "simai.ascend.profile/v1alpha1" ||
      !StringMember(profile, "kind", &kind) ||
      kind != "AscendHardwareProfile" ||
      !StringMember(profile, "schemaSemver", &schema_semver) ||
      (schema_semver != "0.1.0" && schema_semver != "0.2.0") ||
      metadata == nullptr ||
      !StringMember(*metadata, "id", &validated->id) ||
      validated->id.empty() || spec == nullptr ||
      !StringMember(*spec, "status", &status) ||
      status != "READY_FOR_ANALYTICAL" || identity == nullptr ||
      !StringMember(*identity, "vendor", &vendor) ||
      vendor != "HUAWEI_ASCEND" ||
      !StringMember(*identity, "generation", &generation) ||
      (generation != "A2" && generation != "A3" && generation != "A5") ||
      physical_chip_count == nullptr ||
      !PositiveUint64Member(*physical_chip_count, "value", &physical_count) ||
      !StringMember(*physical_chip_count, "unit", &physical_unit) ||
      physical_unit != "count" || management_device_count == nullptr ||
      !PositiveUint64Member(
          *management_device_count, "value", &management_count) ||
      !StringMember(*management_device_count, "unit", &management_unit) ||
      management_unit != "count" || rank_granularity == nullptr ||
      !StringMember(*rank_granularity, "trainingRankUnit", &rank_unit) ||
      (rank_unit != "CHIP" && rank_unit != "MANAGEMENT_DEVICE" &&
       rank_unit != "PACKAGE") || ranks_per_unit == nullptr ||
      !PositiveUint64Member(
          *ranks_per_unit, "value", &ranks_per_unit_count) ||
      !StringMember(*ranks_per_unit, "unit", &ranks_per_unit_unit) ||
      ranks_per_unit_unit != "count" || peak_flops == nullptr ||
      !PositiveUint64Member(*peak_flops, "value", &peak_flops_per_s) ||
      !StringMember(*peak_flops, "unit", &peak_flops_unit) ||
      peak_flops_unit != "FLOP/s" || hbm_capacity == nullptr ||
      !PositiveUint64Member(
          *hbm_capacity, "value", &hbm_capacity_bytes) ||
      !StringMember(*hbm_capacity, "unit", &hbm_capacity_unit) ||
      hbm_capacity_unit != "B" || hbm_bandwidth == nullptr ||
      !NumberMember(*hbm_bandwidth, "value", &hbm_bandwidth_Bps) ||
      hbm_bandwidth_Bps <= 0.0 ||
      !StringMember(*hbm_bandwidth, "unit", &hbm_bandwidth_unit) ||
      hbm_bandwidth_unit != "B/s" || topology_level == nullptr ||
      !StringMember(
          *topology_level, "scope", &validated->topology_domain) ||
      !PositiveIntMember(
          *topology_level, "rankCount", &validated->rank_count) ||
      physical_count != static_cast<uint64_t>(validated->rank_count) ||
      management_count < 1U || ranks_per_unit_count < 1U ||
      !StringMember(
          *topology_level,
          "topologyDigest",
          &validated->topology_digest) ||
      !IsSha256Identifier(validated->topology_digest)) {
    Reject(
        contract,
        "DEVICE_PROFILE_SCHEMA_INVALID",
        "The Ascend Profile schema or canonical units are invalid.",
        "Use simai.ascend.profile/v1alpha1 with complete typed fields.");
    return false;
  }

  const bool exact_v02 = schema_semver == "0.2.0";
  std::map<std::string, ProfileEvidenceRecord> evidence_records;
  std::set<std::string> referenced_evidence;
  bool has_unverified_field = false;
  if (!BuildProfileEvidenceIndex(*spec, exact_v02, &evidence_records) ||
      !ConsumedFieldEvidenceIsValid(
          *physical_chip_count,
          exact_v02,
          evidence_records,
          &has_unverified_field,
          &referenced_evidence) ||
      !ConsumedFieldEvidenceIsValid(
          *management_device_count,
          exact_v02,
          evidence_records,
          &has_unverified_field,
          &referenced_evidence) ||
      !ConsumedFieldEvidenceIsValid(
          *ranks_per_unit,
          exact_v02,
          evidence_records,
          &has_unverified_field,
          &referenced_evidence) ||
      !ConsumedFieldEvidenceIsValid(
          *peak_flops,
          exact_v02,
          evidence_records,
          &has_unverified_field,
          &referenced_evidence) ||
      !ConsumedFieldEvidenceIsValid(
          *hbm_capacity,
          exact_v02,
          evidence_records,
          &has_unverified_field,
          &referenced_evidence) ||
      !ConsumedFieldEvidenceIsValid(
          *hbm_bandwidth,
          exact_v02,
          evidence_records,
          &has_unverified_field,
          &referenced_evidence) ||
      !ConsumedEvidenceIsValid(
          *topology_level,
          exact_v02,
          evidence_records,
          &has_unverified_field,
          &referenced_evidence) ||
      referenced_evidence.empty()) {
    Reject(
        contract,
        "DEVICE_PROFILE_FIELD_EVIDENCE_INVALID",
        "A consumed Profile field lacks resolved evidence or readiness.",
        "Use KNOWN values with valid readiness and a resolved evidenceRef.");
    return false;
  }
  validated->field_readiness =
      has_unverified_field ? "FIELD_UNVERIFIED" : "FIELD_VERIFIED";
  bool all_measured = true;
  bool all_user_input = true;
  validated->hardware_available = true;
  for (const std::string& evidence_ref : referenced_evidence) {
    const ProfileEvidenceRecord& evidence = evidence_records.at(evidence_ref);
    all_measured = all_measured && evidence.evidence_class == "MEASURED";
    all_user_input =
        all_user_input && evidence.evidence_class == "USER_INPUT";
    validated->hardware_available =
        validated->hardware_available && evidence.hardware_available;
  }
  validated->evidence_level = all_measured
      ? "MEASURED"
      : (all_user_input ? "USER_INPUT" : "MIXED");
  validated->evidence_ref = *referenced_evidence.begin();
  return true;
}

struct ValidatedHcclCostModel {
  HcclCostModelConfig config;
  JsonValue raw_reference;
  std::string input_sample_id;
  std::vector<JsonValue> raw_references;
  std::vector<std::string> input_sample_ids;
  std::string collective;
  std::string dtype;
  std::string reduction;
  std::string evidence_level;
  std::string field_readiness;
  std::string evidence_ref;
  bool hardware_available = false;
  std::string group_id;
  std::string group_scope;
  std::vector<int> group_members;
  std::string group_membership_sha256;
  std::string routing_digest;
  std::string raw_metadata_compatibility;
};

bool SupportedCollective(const std::string& collective) {
  return collective == "ALL_REDUCE" || collective == "ALL_GATHER" ||
      collective == "REDUCE_SCATTER" || collective == "ALL_TO_ALL" ||
      collective == "ALL_TO_ALL_V";
}

std::string ExpectedReduction(const std::string& collective) {
  return collective == "ALL_REDUCE" || collective == "REDUCE_SCATTER"
      ? "SUM"
      : "NONE";
}

std::string ExpectedPayloadSemantics(const std::string& collective) {
  if (collective == "ALL_REDUCE") {
    return "HCCL_ALLREDUCE_IN_PLACE_BUFFER_BYTES";
  }
  if (collective == "ALL_GATHER") {
    return "HCCL_ALLGATHER_SEND_BYTES";
  }
  if (collective == "REDUCE_SCATTER") {
    return "HCCL_REDUCESCATTER_INPUT_BYTES";
  }
  if (collective == "ALL_TO_ALL") {
    return "HCCL_ALLTOALL_TOTAL_SEND_BYTES";
  }
  if (collective == "ALL_TO_ALL_V") {
    return "HCCL_ALLTOALLV_COUNTS_MATRIX";
  }
  return "UNKNOWN";
}

std::string ExpectedTrafficAlgorithm(const std::string& collective) {
  if (collective == "ALL_REDUCE") {
    return "RING";
  }
  if (collective == "ALL_GATHER") {
    return "RING_ALL_GATHER";
  }
  if (collective == "REDUCE_SCATTER") {
    return "RING_REDUCE_SCATTER";
  }
  if (collective == "ALL_TO_ALL") {
    return "UNIFORM_DIRECT_EXCHANGE";
  }
  if (collective == "ALL_TO_ALL_V") {
    return "VARIABLE_DIRECT_EXCHANGE";
  }
  return "UNKNOWN";
}

bool ParseLegacyBusbwAdapter(
    const JsonValue& fit,
    const JsonValue& spec,
    AnalyticalRunContract* contract,
    HcclCostModelConfig* config) {
  const JsonValue* adapter = Member(fit, "adapter");
  if (adapter == nullptr) {
    Reject(
        contract,
        "LEGACY_BUSBW_ADAPTER_REQUIRED",
        "Legacy bus bandwidth is accepted only through an explicit adapter.",
        "Provide fit.adapter using simai.legacy.busbw/v1.");
    return false;
  }

  const JsonValue* columns = Member(*adapter, "columns");
  const std::vector<std::string> required_columns = {
      "collective", "message_B", "rank_count", "bus_bandwidth_Bps"};
  if (columns == nullptr || columns->type != JsonValue::Type::Array) {
    Reject(
        contract,
        "LEGACY_BUSBW_COLUMN_MISSING",
        "The legacy bus bandwidth adapter has no typed column declaration.",
        "Declare collective, message_B, rank_count, and bus_bandwidth_Bps.");
    return false;
  }
  std::vector<std::string> declared_columns;
  for (const JsonValue& column : columns->array) {
    if (column.type != JsonValue::Type::String ||
        std::find(
            declared_columns.begin(),
            declared_columns.end(),
            column.string) != declared_columns.end()) {
      Reject(
          contract,
          "LEGACY_BUSBW_ADAPTER_INVALID",
          "The legacy bus bandwidth column declaration is ambiguous.",
          "Declare each required typed column exactly once.");
      return false;
    }
    declared_columns.push_back(column.string);
  }
  for (const std::string& column : required_columns) {
    if (std::find(
            declared_columns.begin(), declared_columns.end(), column) ==
        declared_columns.end()) {
      Reject(
          contract,
          "LEGACY_BUSBW_COLUMN_MISSING",
          "A required legacy bus bandwidth column is missing.",
          "Declare collective, message_B, rank_count, and bus_bandwidth_Bps.");
      return false;
    }
  }
  if (declared_columns.size() != required_columns.size() ||
      Member(fit, "bandwidth") != nullptr) {
    Reject(
        contract,
        "LEGACY_BUSBW_ADAPTER_INVALID",
        "The legacy bus bandwidth source is ambiguous.",
        "Use only the four declared adapter columns and no direct algbw.");
    return false;
  }

  const JsonValue* adapter_domain = Member(*adapter, "messageDomainBytes");
  const JsonValue* model_domain = Member(spec, "messageDomainBytes");
  const JsonValue* group_domain = Member(spec, "groupDomain");
  const JsonValue* bus_bandwidth = Member(*adapter, "busBandwidth");
  std::string schema;
  std::string conversion;
  std::string adapter_collective;
  std::string model_collective;
  std::string adapter_unit;
  std::string adapter_domain_unit;
  std::string model_domain_unit;
  int adapter_rank_count = 0;
  int model_rank_count = 0;
  uint64_t adapter_minimum = 0;
  uint64_t adapter_maximum = 0;
  uint64_t model_minimum = 0;
  uint64_t model_maximum = 0;
  double bus_bandwidth_Bps = 0.0;
  if (!StringMember(*adapter, "schema", &schema) ||
      schema != "simai.legacy.busbw/v1" ||
      !StringMember(*adapter, "conversion", &conversion) ||
      conversion != "HCCL_RING_BUSBW_TO_ALGBW" ||
      !StringMember(*adapter, "collective", &adapter_collective) ||
      !StringMember(spec, "collective", &model_collective) ||
      !PositiveIntMember(*adapter, "rankCount", &adapter_rank_count) ||
      group_domain == nullptr ||
      !FirstArrayPositiveInt(
          *group_domain, "rankCounts", &model_rank_count) ||
      adapter_domain == nullptr || model_domain == nullptr ||
      !PositiveUint64Member(
          *adapter_domain, "min", &adapter_minimum) ||
      !PositiveUint64Member(
          *adapter_domain, "max", &adapter_maximum) ||
      !PositiveUint64Member(*model_domain, "min", &model_minimum) ||
      !PositiveUint64Member(*model_domain, "max", &model_maximum) ||
      !StringMember(*adapter_domain, "unit", &adapter_domain_unit) ||
      !StringMember(*model_domain, "unit", &model_domain_unit) ||
      bus_bandwidth == nullptr ||
      !NumberMember(
          *bus_bandwidth, "value", &bus_bandwidth_Bps) ||
      bus_bandwidth_Bps <= 0.0 ||
      !StringMember(*bus_bandwidth, "unit", &adapter_unit)) {
    Reject(
        contract,
        "LEGACY_BUSBW_ADAPTER_INVALID",
        "The explicit legacy bus bandwidth adapter is malformed.",
        "Use the documented typed adapter schema and conversion.");
    return false;
  }
  if (adapter_unit != "B/s" || adapter_domain_unit != "B" ||
      model_domain_unit != "B") {
    Reject(
        contract,
        "LEGACY_BUSBW_UNIT_AMBIGUOUS",
        "The legacy bus bandwidth adapter uses ambiguous units.",
        "Use message bytes (B) and bus bandwidth bytes per second (B/s).");
    return false;
  }
  if (adapter_collective != model_collective ||
      adapter_rank_count != model_rank_count ||
      adapter_minimum != model_minimum || adapter_maximum != model_maximum) {
    Reject(
        contract,
        "LEGACY_BUSBW_DOMAIN_MISMATCH",
        "The legacy bus bandwidth row is outside the model domain.",
        "Match collective, rank count, and exact message-byte domain.");
    return false;
  }
  if (adapter_rank_count < 2 || model_collective == "ALL_TO_ALL_V") {
    Reject(
        contract,
        "LEGACY_BUSBW_ADAPTER_INVALID",
        "The legacy bus bandwidth conversion is undefined for this domain.",
        "Use a fixed-size HCCL collective with at least two ranks.");
    return false;
  }

  const double rank_count = static_cast<double>(adapter_rank_count);
  const double ring_denominator = rank_count - 1.0;
  config->bandwidth_Bps = model_collective == "ALL_REDUCE"
      ? bus_bandwidth_Bps * rank_count / (2.0 * ring_denominator)
      : bus_bandwidth_Bps * rank_count / ring_denominator;
  if (!std::isfinite(config->bandwidth_Bps) ||
      config->bandwidth_Bps <= 0.0) {
    Reject(
        contract,
        "LEGACY_BUSBW_ADAPTER_INVALID",
        "The legacy bus bandwidth conversion is not finite.",
        "Use a finite positive bus bandwidth in B/s.");
    return false;
  }
  config->source_adapter = "EXPLICIT_LEGACY_BUSBW";
  return true;
}

enum class SegmentParseFailure {
  None,
  NonMonotonic,
};

bool ParsePiecewiseSegments(
    const JsonValue& fit,
    uint64_t model_minimum,
    uint64_t model_maximum,
    std::vector<HcclCostSegment>* parsed,
    SegmentParseFailure* failure) {
  const JsonValue* segments = Member(fit, "segments");
  if (segments == nullptr || segments->type != JsonValue::Type::Array ||
      segments->array.size() < 2U) {
    return false;
  }
  parsed->clear();
  for (size_t index = 0; index < segments->array.size(); ++index) {
    const JsonValue& value = segments->array[index];
    const JsonValue* startup = Member(value, "startup");
    const JsonValue* bandwidth = Member(value, "bandwidth");
    HcclCostSegment segment;
    std::string upper_bound;
    std::string startup_unit;
    std::string bandwidth_unit;
    if (value.type != JsonValue::Type::Object || startup == nullptr ||
        bandwidth == nullptr ||
        !PositiveUint64Member(
            value, "min", &segment.minimum_message_bytes) ||
        !PositiveUint64Member(
            value, "max", &segment.maximum_message_bytes) ||
        segment.minimum_message_bytes >= segment.maximum_message_bytes ||
        !StringMember(value, "upperBound", &upper_bound) ||
        !PositiveUint64Member(*startup, "value", &segment.startup_ns) ||
        !StringMember(*startup, "unit", &startup_unit) ||
        startup_unit != "ns" ||
        !NumberMember(*bandwidth, "value", &segment.bandwidth_Bps) ||
        segment.bandwidth_Bps <= 0.0 ||
        !StringMember(*bandwidth, "unit", &bandwidth_unit) ||
        bandwidth_unit != "B/s") {
      return false;
    }
    const bool is_last = index + 1U == segments->array.size();
    if ((!is_last && upper_bound != "EXCLUSIVE") ||
        (is_last && upper_bound != "INCLUSIVE")) {
      return false;
    }
    segment.maximum_inclusive = is_last;
    if ((index == 0U && segment.minimum_message_bytes != model_minimum) ||
        (is_last && segment.maximum_message_bytes != model_maximum) ||
        (!parsed->empty() &&
         parsed->back().maximum_message_bytes !=
             segment.minimum_message_bytes)) {
      return false;
    }
    if (!parsed->empty()) {
      const HcclCostSegment& previous = parsed->back();
      const double boundary =
          static_cast<double>(segment.minimum_message_bytes);
      const double previous_duration =
          static_cast<double>(previous.startup_ns) +
          boundary * 1000000000.0 / previous.bandwidth_Bps;
      const double current_duration =
          static_cast<double>(segment.startup_ns) +
          boundary * 1000000000.0 / segment.bandwidth_Bps;
      if (!std::isfinite(previous_duration) ||
          !std::isfinite(current_duration) ||
          std::fabs(previous_duration - current_duration) > 1.0) {
        return false;
      }
      const double previous_discrete_duration =
          static_cast<double>(previous.startup_ns) +
          static_cast<double>(segment.minimum_message_bytes - 1U) *
              1000000000.0 / previous.bandwidth_Bps;
      const uint64_t previous_discrete_ns = static_cast<uint64_t>(
          std::llround(previous_discrete_duration));
      const uint64_t current_discrete_ns = static_cast<uint64_t>(
          std::llround(current_duration));
      if (current_discrete_ns < previous_discrete_ns) {
        *failure = SegmentParseFailure::NonMonotonic;
        return false;
      }
    }
    const double maximum_duration =
        static_cast<double>(segment.startup_ns) +
        static_cast<double>(segment.maximum_message_bytes) * 1000000000.0 /
            segment.bandwidth_Bps;
    const double llround_upper_exclusive = std::ldexp(
        1.0, std::numeric_limits<long long>::digits);
    if (!std::isfinite(maximum_duration) || maximum_duration < 0.0 ||
        maximum_duration >= llround_upper_exclusive) {
      return false;
    }
    parsed->push_back(segment);
  }
  return true;
}

bool ValidateHcclCostModel(
    const JsonValue& model,
    const std::string& profile_digest,
    const ValidatedAscendProfile& profile,
    AnalyticalRunContract* contract,
    ValidatedHcclCostModel* validated) {
  std::string api_version;
  std::string kind;
  std::string schema_semver;
  const JsonValue* metadata = Member(model, "metadata");
  const JsonValue* spec = Member(model, "spec");
  const JsonValue* group_domain =
      spec == nullptr ? nullptr : Member(*spec, "groupDomain");
  const JsonValue* message_domain =
      spec == nullptr ? nullptr : Member(*spec, "messageDomainBytes");
  const JsonValue* fit = spec == nullptr ? nullptr : Member(*spec, "fit");
  const JsonValue* startup = fit == nullptr ? nullptr : Member(*fit, "startup");
  const JsonValue* bandwidth =
      fit == nullptr ? nullptr : Member(*fit, "bandwidth");
  const JsonValue* input_sample =
      spec == nullptr ? nullptr : FirstArrayObject(*spec, "inputSamples");
  const JsonValue* input_samples =
      spec == nullptr ? nullptr : Member(*spec, "inputSamples");
  const JsonValue* extrapolation =
      spec == nullptr ? nullptr : Member(*spec, "extrapolation");
  const JsonValue* traffic =
      spec == nullptr ? nullptr : Member(*spec, "traffic");

  std::string model_profile_digest;
  std::string timing_scope;
  std::string group_type;
  std::string model_topology_domain;
  std::string model_topology_digest;
  std::string message_unit;
  std::string family;
  std::string formula;
  std::string interpolation;
  std::string startup_unit;
  std::string bandwidth_unit;
  std::string traffic_algorithm;
  std::string traffic_semantics;
  std::string payload_semantics;
  int rank_count = 0;
  bool extrapolation_allowed = true;
  SegmentParseFailure segment_failure = SegmentParseFailure::None;
  const std::string supported_formula =
      "round(startup_ns + message_B / bandwidth_Bps * 1000000000)";
  if (fit != nullptr &&
      (!StringMember(*fit, "formula", &formula) ||
       formula != supported_formula)) {
    Reject(
        contract,
        "HCCL_COST_MODEL_FORMULA_INVALID",
        "The HCCL cost model formula is missing or unsupported.",
        "Use the exact documented ALPHA_BETA nanosecond formula.");
    return false;
  }
  if (fit != nullptr && StringMember(*fit, "family", &family) &&
      family == "LEGACY_BUSBW_ADAPTER" && spec != nullptr &&
      !ParseLegacyBusbwAdapter(
          *fit, *spec, contract, &validated->config)) {
    return false;
  }
  std::string declared_readiness;
  if (spec != nullptr &&
      StringMember(*spec, "readiness", &declared_readiness) &&
      declared_readiness == "FIELD_UNVERIFIED" &&
      ContainsString(model, "MEASURED")) {
    Reject(
        contract,
        "EVIDENCE_READINESS_CONFLICT",
        "A FIELD_UNVERIFIED cost model cannot claim MEASURED evidence.",
        "Use DERIVED evidence or verify every consumed model field.");
    return false;
  }
  const bool schema_declared =
      StringMember(model, "schemaSemver", &schema_semver);
  const bool a2_extended_schema = schema_semver == "0.2.0";
  if (!StringMember(model, "apiVersion", &api_version) ||
      api_version != "simai.ascend.costmodel/v1alpha1" ||
      !StringMember(model, "kind", &kind) || kind != "HcclCostModel" ||
      !schema_declared ||
      (schema_semver != "0.1.0" && !a2_extended_schema) ||
      metadata == nullptr ||
      !StringMember(*metadata, "id", &validated->config.model_id) ||
      validated->config.model_id.empty() || spec == nullptr ||
      !StringMember(*spec, "profileDigest", &model_profile_digest) ||
      model_profile_digest != profile_digest ||
      !StringMember(*spec, "collective", &validated->collective) ||
      !SupportedCollective(validated->collective) || group_domain == nullptr ||
      (Member(*spec, "rawMetadataCompatibility") != nullptr &&
       (!StringMember(
            *spec,
            "rawMetadataCompatibility",
            &validated->raw_metadata_compatibility) ||
        validated->collective != "ALL_REDUCE" ||
        validated->raw_metadata_compatibility !=
            "simai.issue17.allreduce-metadata-absent/v1")) ||
      !StringMember(*spec, "dtype", &validated->dtype) ||
      validated->dtype != "BF16" ||
      !StringMember(*spec, "reduction", &validated->reduction) ||
      validated->reduction != ExpectedReduction(validated->collective) ||
      (validated->collective != "ALL_REDUCE" &&
       (!StringMember(*spec, "payloadSemantics", &payload_semantics) ||
        payload_semantics !=
            ExpectedPayloadSemantics(validated->collective))) ||
      (validated->collective == "ALL_TO_ALL_V" &&
       (!StringMember(*spec, "routingDigest", &validated->routing_digest) ||
        !IsSha256Identifier(validated->routing_digest))) ||
      !StringMember(*spec, "timingScope", &timing_scope) ||
      timing_scope != "DEVICE_ONLY" ||
      !FirstArrayPositiveInt(*group_domain, "rankCounts", &rank_count) ||
      !FirstArrayString(*group_domain, "groupTypes", &group_type) ||
      !FirstArrayString(
          *group_domain, "scopes", &model_topology_domain) ||
      !FirstArrayString(
          *group_domain, "topologyDigests", &model_topology_digest) ||
      (a2_extended_schema
           ? (rank_count > profile.rank_count || rank_count < 1)
           : rank_count != profile.rank_count) ||
      (a2_extended_schema
           ? model_topology_domain != group_type + "_SUBGROUP"
           : model_topology_domain != profile.topology_domain) ||
      model_topology_digest != profile.topology_digest ||
      message_domain == nullptr ||
      !PositiveUint64Member(
          *message_domain,
          "min",
          &validated->config.minimum_message_bytes) ||
      !PositiveUint64Member(
          *message_domain,
          "max",
          &validated->config.maximum_message_bytes) ||
      validated->config.minimum_message_bytes >
          validated->config.maximum_message_bytes ||
      !StringMember(*message_domain, "unit", &message_unit) ||
      message_unit != "B" || fit == nullptr ||
      !StringMember(*fit, "family", &family) ||
      !StringMember(*fit, "interpolation", &interpolation) ||
      !((family == "ALPHA_BETA" && interpolation == "NONE" &&
         startup != nullptr &&
         PositiveUint64Member(
             *startup, "value", &validated->config.startup_ns) &&
         StringMember(*startup, "unit", &startup_unit) &&
         startup_unit == "ns" && bandwidth != nullptr &&
         NumberMember(
             *bandwidth, "value", &validated->config.bandwidth_Bps) &&
         validated->config.bandwidth_Bps > 0.0 &&
         StringMember(*bandwidth, "unit", &bandwidth_unit) &&
         bandwidth_unit == "B/s") ||
        (family == "PIECEWISE_ALPHA_BETA" &&
         interpolation == "SEGMENT_LOCAL" &&
         ParsePiecewiseSegments(
             *fit,
             validated->config.minimum_message_bytes,
             validated->config.maximum_message_bytes,
             &validated->config.segments,
             &segment_failure)) ||
        (family == "LEGACY_BUSBW_ADAPTER" &&
         interpolation == "NONE" && startup != nullptr &&
         PositiveUint64Member(
             *startup, "value", &validated->config.startup_ns) &&
         StringMember(*startup, "unit", &startup_unit) &&
         startup_unit == "ns" &&
         validated->config.bandwidth_Bps > 0.0)) ||
      input_sample == nullptr ||
      input_samples == nullptr ||
      input_samples->type != JsonValue::Type::Array ||
      input_samples->array.empty() ||
      !StringMember(
          *input_sample, "id", &validated->input_sample_id) ||
      validated->input_sample_id.empty() || traffic == nullptr ||
      !StringMember(*traffic, "algorithm", &traffic_algorithm) ||
      traffic_algorithm != ExpectedTrafficAlgorithm(validated->collective) ||
      !StringMember(*traffic, "semantics", &traffic_semantics) ||
      traffic_semantics != "ALGORITHM_TOTAL_GROUP_BYTES" ||
      !StringMember(*spec, "evidenceClass", &validated->evidence_level) ||
      validated->evidence_level != "DERIVED" ||
      !StringMember(*spec, "readiness", &validated->field_readiness) ||
      (a2_extended_schema
           ? (validated->field_readiness != "FIELD_UNVERIFIED" &&
              validated->field_readiness != "FIELD_VERIFIED")
           : validated->field_readiness != "FIELD_UNVERIFIED") ||
      extrapolation == nullptr ||
      !BooleanMember(
          *extrapolation, "allowed", &extrapolation_allowed) ||
      extrapolation_allowed || !UnitsAreCanonical(model) ||
      (!a2_extended_schema && ContainsString(model, "MEASURED"))) {
    if (segment_failure == SegmentParseFailure::NonMonotonic) {
      Reject(
          contract,
          "HCCL_COST_MODEL_NON_MONOTONIC",
          "The piecewise HCCL model decreases at an integer-byte boundary.",
          "Make each segment boundary nondecreasing after ns rounding.");
      return false;
    }
    Reject(
        contract,
        "HCCL_COST_MODEL_SCHEMA_INVALID",
        "The HCCL DerivedCostModel or its domain is invalid.",
        "Provide a non-extrapolating collective model matching the Profile.");
    return false;
  }

  if (a2_extended_schema) {
    const JsonValue* group_ids = Member(*group_domain, "groupIds");
    const JsonValue* member_ranks = Member(*group_domain, "memberRanks");
    const JsonValue* membership_digests =
        Member(*group_domain, "membershipDigests");
    const JsonValue* evidence_records = Member(*spec, "evidence");
    const JsonValue* evidence_record =
        evidence_records == nullptr || evidence_records->type != JsonValue::Type::Array ||
            evidence_records->array.size() != 1U
        ? nullptr
        : &evidence_records->array.front();
    std::string evidence_id;
    std::string evidence_class;
    std::string evidence_readiness;
    bool evidence_hardware_available = false;
    const bool exact_schema =
        ObjectHasExactKeys(
            model, {"apiVersion", "kind", "schemaSemver", "metadata", "spec"}) &&
        ObjectHasExactKeys(*metadata, {"id"}) &&
        ObjectHasExactKeys(
            *spec,
            {"profileDigest", "collective", "dtype", "reduction",
             "timingScope", "groupDomain", "messageDomainBytes",
             "inputSamples", "fit", "traffic", "evidenceClass",
             "readiness", "evidenceRef", "evidence", "extrapolation"}) &&
        ObjectHasExactKeys(
            *group_domain,
            {"rankCounts", "groupTypes", "scopes", "topologyDigests",
             "groupIds", "memberRanks", "membershipDigests"}) &&
        ObjectHasExactKeys(*message_domain, {"min", "max", "unit"}) &&
        ObjectHasExactKeys(
            *fit,
            {"family", "formula", "startup", "bandwidth", "interpolation"}) &&
        startup != nullptr &&
        ObjectHasExactKeys(*startup, {"value", "unit"}) &&
        bandwidth != nullptr &&
        ObjectHasExactKeys(*bandwidth, {"value", "unit"}) &&
        ObjectHasExactKeys(*traffic, {"algorithm", "semantics"}) &&
        ObjectHasExactKeys(*extrapolation, {"allowed", "policy"}) &&
        input_samples->array.size() == 1U &&
        ObjectHasExactKeys(
            input_samples->array.front(), {"id", "path", "sha256"}) &&
        evidence_record != nullptr &&
        A2EvidenceRecordIsExact(
            *evidence_record,
            &evidence_id,
            &evidence_class,
            &evidence_readiness,
            &evidence_hardware_available) &&
        StringMember(*spec, "evidenceRef", &validated->evidence_ref) &&
        validated->evidence_ref == evidence_id &&
        evidence_class == validated->evidence_level &&
        evidence_readiness == validated->field_readiness &&
        group_ids != nullptr && group_ids->type == JsonValue::Type::Array &&
        group_ids->array.size() == 1U &&
        member_ranks != nullptr && member_ranks->type == JsonValue::Type::Array &&
        member_ranks->array.size() == 1U &&
        membership_digests != nullptr &&
        membership_digests->type == JsonValue::Type::Array &&
        membership_digests->array.size() == 1U;
    if (!exact_schema) {
      Reject(
          contract,
          "HCCL_COST_MODEL_SCHEMA_INVALID",
          "The A2 HCCL cost model v0.2 schema is not exact.",
          "Remove unknown fields and resolve every evidence reference.");
      return false;
    }
    std::string membership_digest;
    if (group_ids->array.front().type == JsonValue::Type::String) {
      validated->group_id = group_ids->array.front().string;
    }
    if (membership_digests->array.front().type == JsonValue::Type::String) {
      membership_digest = membership_digests->array.front().string;
    }
    const int expected_group_rank_count = group_type == "TP" ? 1 : 4;
    if (validated->group_id.empty() ||
        !ParseExactRankMembers(
            member_ranks->array.front(),
            profile.rank_count,
            &validated->group_members) ||
        validated->group_members.size() != static_cast<size_t>(rank_count) ||
        rank_count != expected_group_rank_count ||
        !IsSha256Identifier(membership_digest) ||
        membership_digest !=
            A2MembershipDigest(
                validated->group_id,
                group_type,
                validated->group_members,
                model_topology_digest)) {
      Reject(
          contract,
          "HCCL_COST_MODEL_GROUP_INVALID",
          "The A2 model group is not a legal frozen-topology subgroup.",
          "Use a legal TP, DP, or EP membership and its canonical digest.");
      return false;
    }
    validated->group_membership_sha256 = membership_digest;
    validated->group_scope = model_topology_domain;
    validated->hardware_available = evidence_hardware_available;
  }

  if (validated->config.segments.empty()) {
    const double llround_upper_exclusive = std::ldexp(
        1.0, std::numeric_limits<long long>::digits);
    const double maximum_duration_ns =
        static_cast<double>(validated->config.startup_ns) +
        static_cast<double>(validated->config.maximum_message_bytes) *
            1000000000.0 / validated->config.bandwidth_Bps;
    if (!std::isfinite(maximum_duration_ns) ||
        maximum_duration_ns >= llround_upper_exclusive) {
      Reject(
          contract,
          "HCCL_COST_MODEL_NUMERIC_OVERFLOW",
          "The HCCL model duration exceeds the supported rounding range.",
          "Use finite model parameters whose duration is below 2^63 ns.");
      return false;
    }
  }

  for (const JsonValue& sample : input_samples->array) {
    std::string sample_id;
    if (sample.type != JsonValue::Type::Object ||
        !StringMember(sample, "id", &sample_id) || sample_id.empty()) {
      Reject(
          contract,
          "HCCL_COST_MODEL_SCHEMA_INVALID",
          "Every inputSamples entry must identify one RawObservation.",
          "Provide immutable RawObservation id/path/SHA-256 references.");
      return false;
    }
    validated->raw_references.push_back(sample);
    validated->input_sample_ids.push_back(sample_id);
  }

  if (group_type == "TP") {
    validated->config.group_type = AstraSim::CostedGroupType::TP;
  } else if (group_type == "DP") {
    validated->config.group_type = AstraSim::CostedGroupType::DP;
  } else if (group_type == "EP") {
    validated->config.group_type = AstraSim::CostedGroupType::EP;
  } else if (group_type == "DP_EP") {
    validated->config.group_type = AstraSim::CostedGroupType::DP_EP;
  } else {
    Reject(
        contract,
        "HCCL_COST_MODEL_SCHEMA_INVALID",
        "The HCCL group type is unsupported.",
        "Use TP, DP, EP, or DP_EP.");
    return false;
  }
  if (validated->collective == "ALL_REDUCE") {
    validated->config.collective = AstraSim::CostedCollective::AllReduce;
  } else if (validated->collective == "ALL_GATHER") {
    validated->config.collective = AstraSim::CostedCollective::AllGather;
  } else if (validated->collective == "REDUCE_SCATTER") {
    validated->config.collective = AstraSim::CostedCollective::ReduceScatter;
  } else if (validated->collective == "ALL_TO_ALL") {
    validated->config.collective = AstraSim::CostedCollective::AllToAll;
  } else {
    validated->config.collective = AstraSim::CostedCollective::AllToAllV;
    validated->config.routing_digest = validated->routing_digest;
  }
  validated->config.payload_semantics =
      ExpectedPayloadSemantics(validated->collective);
  validated->config.traffic_algorithm = traffic_algorithm;
  validated->config.rank_count = rank_count;
  validated->config.topology_domain = profile.topology_domain;
  validated->config.topology_digest = profile.topology_digest;
  validated->raw_reference = *input_sample;
  return true;
}

struct ValidatedRawObservation {
  std::string evidence_level;
  std::string field_readiness;
  std::string evidence_ref;
  bool hardware_available = false;
  std::string group_id;
  std::vector<int> group_members;
  std::string group_membership_sha256;
};

bool ValidateRawObservation(
    const JsonValue& raw,
    const std::string& profile_digest,
    const ValidatedAscendProfile& profile,
    const ValidatedHcclCostModel& model,
    AnalyticalRunContract* contract,
    ValidatedRawObservation* validated) {
  std::string api_version;
  std::string kind;
  std::string schema_semver;
  const JsonValue* spec = Member(raw, "spec");
  const JsonValue* metadata = Member(raw, "metadata");
  const JsonValue* group =
      spec == nullptr ? nullptr : Member(*spec, "group");
  const JsonValue* payload =
      spec == nullptr ? nullptr : Member(*spec, "payload");
  const JsonValue* bytes_per_rank =
      payload == nullptr ? nullptr : Member(*payload, "bytesPerRank");
  const JsonValue* normalized =
      spec == nullptr ? nullptr : Member(*spec, "normalized");
  const JsonValue* average_time =
      normalized == nullptr ? nullptr : Member(*normalized, "averageTime");
  const JsonValue* alg_bandwidth =
      normalized == nullptr ? nullptr : Member(*normalized, "algBandwidth");
  const JsonValue* first_evidence =
      spec == nullptr ? nullptr : FirstArrayObject(*spec, "evidence");
  const JsonValue* correctness =
      spec == nullptr ? nullptr : Member(*spec, "correctness");
  const JsonValue* eligibility =
      spec == nullptr ? nullptr : Member(*spec, "eligibility");
  const JsonValue* algorithm =
      spec == nullptr ? nullptr : Member(*spec, "algorithm");
  const JsonValue* statistics =
      spec == nullptr ? nullptr : Member(*spec, "statistics");

  std::string profile_ref;
  std::string raw_id;
  std::string raw_profile_digest;
  std::string collective;
  std::string scope;
  std::string group_type;
  std::string topology_digest;
  std::string byte_semantics;
  std::string byte_unit;
  std::string time_unit;
  std::string bandwidth_unit;
  std::string dtype;
  std::string reduction;
  std::string correctness_status;
  std::string raw_routing_digest;
  std::string algorithm_name;
  std::string algorithm_version;
  std::string timing_statistic;
  int rank_count = 0;
  int sample_count = 0;
  uint64_t message_bytes = 0;
  uint64_t time_ns = 0;
  double normalized_bandwidth_Bps = 0.0;
  bool eligible_for_fit = false;
  bool warmup_excluded = false;
  const bool schema_declared =
      StringMember(raw, "schemaSemver", &schema_semver);
  const bool a2_extended_schema = schema_semver == "0.2.0";
  const bool payload_value_valid = bytes_per_rank != nullptr &&
      (model.collective == "ALL_TO_ALL_V"
           ? (PositiveUint64Member(
                  *bytes_per_rank, "maximumValue", &message_bytes) &&
              payload != nullptr &&
              StringMember(*payload, "routingDigest", &raw_routing_digest) &&
              raw_routing_digest == model.routing_digest)
           : PositiveUint64Member(
                 *bytes_per_rank, "uniformValue", &message_bytes));
  const bool has_canonical_metadata =
      (algorithm != nullptr && statistics != nullptr &&
       StringMember(*algorithm, "name", &algorithm_name) &&
       algorithm_name == model.config.traffic_algorithm &&
       StringMember(*algorithm, "version", &algorithm_version) &&
       !algorithm_version.empty() && statistics != nullptr &&
       StringMember(*statistics, "timingStatistic", &timing_statistic) &&
       timing_statistic == "ARITHMETIC_MEAN" &&
       PositiveIntMember(*statistics, "sampleCount", &sample_count) &&
       BooleanMember(*statistics, "warmupExcluded", &warmup_excluded) &&
       warmup_excluded);
  const bool uses_issue17_metadata_compatibility =
      model.collective == "ALL_REDUCE" && algorithm == nullptr &&
      statistics == nullptr &&
      model.raw_metadata_compatibility ==
          "simai.issue17.allreduce-metadata-absent/v1";
  if (!StringMember(raw, "apiVersion", &api_version) ||
      api_version != "simai.ascend.observation/v1alpha1" ||
      !StringMember(raw, "kind", &kind) || kind != "HcclRawSample" ||
      !schema_declared ||
      (schema_semver != "0.1.0" && !a2_extended_schema) ||
      metadata == nullptr ||
      !StringMember(*metadata, "id", &raw_id) ||
      raw_id != model.input_sample_id || spec == nullptr ||
      !StringMember(*spec, "profileRef", &profile_ref) ||
      profile_ref != profile.id ||
      !StringMember(*spec, "profileDigest", &raw_profile_digest) ||
      raw_profile_digest != profile_digest ||
      !StringMember(*spec, "collective", &collective) ||
      collective != model.collective || group == nullptr ||
      !PositiveIntMember(*group, "rankCount", &rank_count) ||
      (!a2_extended_schema && rank_count != model.config.rank_count) ||
      !StringMember(*group, "scope", &scope) ||
      scope != (a2_extended_schema ? model.group_scope
                                   : model.config.topology_domain) ||
      !StringMember(*group, "groupType", &group_type) ||
      group_type != AstraSim::CostedGroupTypeName(model.config.group_type) ||
      !StringMember(*group, "topologyDigest", &topology_digest) ||
      topology_digest != profile.topology_digest || bytes_per_rank == nullptr ||
      !StringMember(*bytes_per_rank, "semantics", &byte_semantics) ||
      (model.collective == "ALL_REDUCE"
           ? byte_semantics != "API_INPUT_BYTES"
           : byte_semantics != model.config.payload_semantics) ||
      !payload_value_valid ||
      message_bytes < model.config.minimum_message_bytes ||
      message_bytes > model.config.maximum_message_bytes ||
      !StringMember(*bytes_per_rank, "unit", &byte_unit) || byte_unit != "B" ||
      !StringMember(*payload, "dtype", &dtype) || dtype != model.dtype ||
      !StringMember(*payload, "reduction", &reduction) ||
      reduction != model.reduction || average_time == nullptr ||
      !PositiveUint64Member(*average_time, "value", &time_ns) ||
      !StringMember(*average_time, "unit", &time_unit) || time_unit != "ns" ||
      alg_bandwidth == nullptr ||
      !NumberMember(
          *alg_bandwidth, "value", &normalized_bandwidth_Bps) ||
      normalized_bandwidth_Bps <= 0.0 ||
      !StringMember(*alg_bandwidth, "unit", &bandwidth_unit) ||
      bandwidth_unit != "B/s" || first_evidence == nullptr ||
      !StringMember(
          *first_evidence, "class", &validated->evidence_level) ||
      !StringMember(
          *first_evidence, "readiness", &validated->field_readiness) ||
      (a2_extended_schema
           ? (validated->field_readiness != "FIELD_UNVERIFIED" &&
              validated->field_readiness != "FIELD_VERIFIED")
           : validated->field_readiness != "FIELD_UNVERIFIED") ||
      !EvidenceRecordIsComplete(*first_evidence) || correctness == nullptr ||
      !StringMember(*correctness, "status", &correctness_status) ||
      correctness_status != "PASS" || eligibility == nullptr ||
      !BooleanMember(*eligibility, "fit", &eligible_for_fit) ||
      !eligible_for_fit ||
      (!has_canonical_metadata && !uses_issue17_metadata_compatibility) ||
      !UnitsAreCanonical(raw) ||
      (!a2_extended_schema && ContainsString(raw, "MEASURED"))) {
    Reject(
        contract,
        "RAW_OBSERVATION_SCHEMA_INVALID",
        "The immutable HCCL RawObservation is invalid or out of domain.",
        "Use a canonical-unit HCCL observation matching Profile and model.");
    return false;
  }

  if (a2_extended_schema) {
    const JsonValue* members = Member(*group, "members");
    const JsonValue* evidence_records = Member(*spec, "evidence");
    const JsonValue* evidence_record =
        evidence_records == nullptr || evidence_records->type != JsonValue::Type::Array ||
            evidence_records->array.size() != 1U
        ? nullptr
        : &evidence_records->array.front();
    const JsonValue* reasons = Member(*eligibility, "reasons");
    std::string declared_evidence_class;
    std::string declared_readiness;
    std::string evidence_id;
    std::string evidence_class;
    std::string evidence_readiness;
    bool evidence_hardware_available = false;
    const bool exact_schema =
        ObjectHasExactKeys(
            raw, {"apiVersion", "kind", "schemaSemver", "metadata", "spec"}) &&
        ObjectHasExactKeys(*metadata, {"id"}) &&
        ObjectHasExactKeys(
            *spec,
            {"profileRef", "profileDigest", "collective", "group", "payload",
             "normalized", "correctness", "eligibility", "algorithm",
             "statistics", "evidenceClass", "readiness", "evidenceRef",
             "evidence"}) &&
        ObjectHasExactKeys(
            *group,
            {"id", "rankCount", "scope", "groupType", "members",
             "membershipDigest", "topologyDigest"}) &&
        ObjectHasExactKeys(*payload, {"bytesPerRank", "dtype", "reduction"}) &&
        ObjectHasExactKeys(
            *bytes_per_rank, {"semantics", "uniformValue", "unit"}) &&
        ObjectHasExactKeys(*normalized, {"averageTime", "algBandwidth"}) &&
        ObjectHasExactKeys(*average_time, {"value", "unit"}) &&
        ObjectHasExactKeys(*alg_bandwidth, {"value", "unit"}) &&
        ObjectHasExactKeys(*correctness, {"status"}) &&
        ObjectHasExactKeys(*eligibility, {"fit", "reasons"}) &&
        reasons != nullptr && reasons->type == JsonValue::Type::Array &&
        reasons->array.empty() &&
        ObjectHasExactKeys(*algorithm, {"name", "version"}) &&
        ObjectHasExactKeys(
            *statistics,
            {"timingStatistic", "sampleCount", "warmupExcluded"}) &&
        evidence_record != nullptr &&
        A2EvidenceRecordIsExact(
            *evidence_record,
            &evidence_id,
            &evidence_class,
            &evidence_readiness,
            &evidence_hardware_available) &&
        StringMember(*spec, "evidenceClass", &declared_evidence_class) &&
        declared_evidence_class == evidence_class &&
        StringMember(*spec, "readiness", &declared_readiness) &&
        declared_readiness == evidence_readiness &&
        StringMember(*spec, "evidenceRef", &validated->evidence_ref) &&
        validated->evidence_ref == evidence_id &&
        ((evidence_class == "USER_INPUT" &&
          evidence_readiness == "FIELD_UNVERIFIED") ||
         (evidence_class == "MEASURED" &&
          evidence_readiness == "FIELD_VERIFIED"));
    if (!exact_schema) {
      Reject(
          contract,
          "RAW_OBSERVATION_SCHEMA_INVALID",
          "The A2 RawObservation v0.2 schema is not exact.",
          "Remove unknown fields and resolve every evidence reference.");
      return false;
    }
    std::string group_id;
    std::string membership_digest;
    if (!StringMember(*group, "id", &group_id) || group_id.empty() ||
        members == nullptr ||
        !ParseExactRankMembers(
            *members, profile.rank_count, &validated->group_members) ||
        validated->group_members.size() != static_cast<size_t>(rank_count) ||
        rank_count != model.config.rank_count ||
        !StringMember(*group, "membershipDigest", &membership_digest) ||
        membership_digest !=
            A2MembershipDigest(
                group_id, group_type, validated->group_members, topology_digest) ||
        group_id != model.group_id ||
        validated->group_members != model.group_members ||
        membership_digest != model.group_membership_sha256) {
      Reject(
          contract,
          "RAW_OBSERVATION_GROUP_INVALID",
          "The A2 RawObservation group does not match the legal model subgroup.",
          "Use the exact group id, rank count, membership, and membership digest.");
      return false;
    }
    validated->group_id = group_id;
    validated->group_membership_sha256 = membership_digest;
    validated->hardware_available = evidence_hardware_available;
  }

  uint64_t startup_ns = model.config.startup_ns;
  double bandwidth_Bps = model.config.bandwidth_Bps;
  if (!model.config.segments.empty()) {
    bool matched_segment = false;
    for (const HcclCostSegment& segment : model.config.segments) {
      const bool below_upper = segment.maximum_inclusive
          ? message_bytes <= segment.maximum_message_bytes
          : message_bytes < segment.maximum_message_bytes;
      if (message_bytes >= segment.minimum_message_bytes && below_upper) {
        startup_ns = segment.startup_ns;
        bandwidth_Bps = segment.bandwidth_Bps;
        matched_segment = true;
        break;
      }
    }
    if (!matched_segment) {
      Reject(
          contract,
          "RAW_OBSERVATION_MODEL_INCONSISTENT",
          "The RawObservation is not covered by a model segment.",
          "Cover every referenced observation with exactly one segment.");
      return false;
    }
  }
  const double predicted_ns =
      static_cast<double>(startup_ns) +
      static_cast<double>(message_bytes) * 1000000000.0 / bandwidth_Bps;
  const uint64_t rounded_prediction_ns =
      static_cast<uint64_t>(std::llround(predicted_ns));
  const uint64_t timing_residual_ns = time_ns > rounded_prediction_ns
      ? time_ns - rounded_prediction_ns
      : rounded_prediction_ns - time_ns;
  const double implied_bandwidth_Bps =
      static_cast<double>(message_bytes) * 1000000000.0 /
      static_cast<double>(time_ns);
  const double bandwidth_relative_residual =
      std::fabs(normalized_bandwidth_Bps - implied_bandwidth_Bps) /
      implied_bandwidth_Bps;
  if (timing_residual_ns > 1U ||
      !std::isfinite(bandwidth_relative_residual) ||
      bandwidth_relative_residual > 0.00001) {
    Reject(
        contract,
        "RAW_OBSERVATION_MODEL_INCONSISTENT",
        "The RawObservation timing or normalized bandwidth is inconsistent.",
        "Keep timing within 1 ns and bandwidth within 10 ppm of bytes/time.");
    return false;
  }
  return true;
}

struct ValidatedRouting {
  uint64_t total_network_bytes = 0;
  uint64_t maximum_send_bytes = 0;
  uint64_t maximum_receive_bytes = 0;
  std::string evidence_level;
  std::string field_readiness;
};

bool NonnegativeJsonInteger(const JsonValue& value, uint64_t* parsed) {
  const double maximum_exact_json_integer = 9007199254740991.0;
  if (value.type != JsonValue::Type::Number || value.number < 0.0 ||
      value.number > maximum_exact_json_integer ||
      std::floor(value.number) != value.number) {
    return false;
  }
  *parsed = static_cast<uint64_t>(value.number);
  return true;
}

bool ValidateA2AVRouting(
    const JsonValue& routing,
    const std::string& profile_digest,
    const ValidatedAscendProfile& profile,
    const ValidatedHcclCostModel& model,
    AnalyticalRunContract* contract,
    ValidatedRouting* validated) {
  std::string api_version;
  std::string kind;
  std::string schema_semver;
  std::string routing_profile_digest;
  std::string topology_digest;
  std::string count_semantics;
  std::string unit;
  int rank_count = 0;
  const JsonValue* metadata = Member(routing, "metadata");
  const JsonValue* spec = Member(routing, "spec");
  const JsonValue* counts = spec == nullptr ? nullptr : Member(*spec, "sendCounts");
  const JsonValue* evidence =
      spec == nullptr ? nullptr : FirstArrayObject(*spec, "evidence");
  int declared_rank_count = 0;
  if (spec != nullptr &&
      PositiveIntMember(*spec, "rankCount", &declared_rank_count) &&
      declared_rank_count > kMaximumDenseRoutingRanks) {
    Reject(
        contract,
        "HCCL_ROUTING_CAPACITY_EXCEEDED",
        "The dense HCCL routing rank count exceeds the bounded parser limit.",
        "Use at most 256 ranks for this dense Analytical routing artifact.");
    return false;
  }
  if (counts != nullptr && counts->type == JsonValue::Type::Array) {
    size_t declared_cells = 0U;
    if (counts->array.size() >
        static_cast<size_t>(kMaximumDenseRoutingRanks)) {
      Reject(
          contract,
          "HCCL_ROUTING_CAPACITY_EXCEEDED",
          "The dense HCCL routing matrix exceeds the bounded cell limit.",
          "Use at most 256 rows, 256 columns, and 65,536 cells.");
      return false;
    }
    for (const JsonValue& row : counts->array) {
      if (row.type == JsonValue::Type::Array &&
          (row.array.size() >
               static_cast<size_t>(kMaximumDenseRoutingRanks) ||
           declared_cells >
               kMaximumDenseRoutingCells - row.array.size())) {
        Reject(
            contract,
            "HCCL_ROUTING_CAPACITY_EXCEEDED",
            "The dense HCCL routing matrix exceeds the bounded cell limit.",
            "Use at most 256 rows, 256 columns, and 65,536 cells.");
        return false;
      }
      if (row.type == JsonValue::Type::Array) {
        declared_cells += row.array.size();
      }
    }
  }
  if (!StringMember(routing, "apiVersion", &api_version) ||
      api_version != "simai.ascend.routing/v1alpha1" ||
      !StringMember(routing, "kind", &kind) ||
      kind != "HcclAllToAllVRouting" ||
      !StringMember(routing, "schemaSemver", &schema_semver) ||
      schema_semver != "0.1.0" || metadata == nullptr || spec == nullptr ||
      !StringMember(*spec, "profileDigest", &routing_profile_digest) ||
      routing_profile_digest != profile_digest ||
      !StringMember(*spec, "topologyDigest", &topology_digest) ||
      topology_digest != profile.topology_digest ||
      !PositiveIntMember(*spec, "rankCount", &rank_count) ||
      rank_count != model.config.rank_count ||
      !StringMember(*spec, "countSemantics", &count_semantics) ||
      count_semantics != "HCCL_SEND_COUNTS_BYTES" ||
      !StringMember(*spec, "unit", &unit) || unit != "B" ||
      counts == nullptr || counts->type != JsonValue::Type::Array ||
      counts->array.size() != static_cast<size_t>(rank_count) ||
      evidence == nullptr ||
      !StringMember(*evidence, "class", &validated->evidence_level) ||
      validated->evidence_level != "USER_INPUT" ||
      !StringMember(
          *evidence, "readiness", &validated->field_readiness) ||
      validated->field_readiness != "FIELD_UNVERIFIED" ||
      !EvidenceRecordIsComplete(*evidence) || !UnitsAreCanonical(routing) ||
      ContainsString(routing, "MEASURED")) {
    Reject(
        contract,
        "HCCL_ROUTING_SCHEMA_INVALID",
        "The HCCL AllToAllV routing counts are invalid.",
        "Provide a canonical immutable rank-by-rank send-count matrix.");
    return false;
  }

  std::vector<uint64_t> receives(static_cast<size_t>(rank_count), 0U);
  for (int source = 0; source < rank_count; ++source) {
    const JsonValue& row = counts->array[static_cast<size_t>(source)];
    if (row.type != JsonValue::Type::Array ||
        row.array.size() != static_cast<size_t>(rank_count)) {
      Reject(
          contract,
          "HCCL_ROUTING_SCHEMA_INVALID",
          "The HCCL AllToAllV routing matrix shape is invalid.",
          "Provide exactly rankCount rows and columns.");
      return false;
    }
    uint64_t send_total = 0U;
    for (int destination = 0; destination < rank_count; ++destination) {
      uint64_t bytes = 0U;
      if (!NonnegativeJsonInteger(
              row.array[static_cast<size_t>(destination)], &bytes) ||
          (source == destination && bytes != 0U) ||
          send_total > std::numeric_limits<uint64_t>::max() - bytes ||
          receives[static_cast<size_t>(destination)] >
              std::numeric_limits<uint64_t>::max() - bytes ||
          validated->total_network_bytes >
              std::numeric_limits<uint64_t>::max() - bytes) {
        Reject(
            contract,
            "HCCL_ROUTING_SCHEMA_INVALID",
            "The HCCL AllToAllV routing counts are invalid or overflow.",
            "Use finite nonnegative byte counts and zero self traffic.");
        return false;
      }
      send_total += bytes;
      receives[static_cast<size_t>(destination)] += bytes;
      validated->total_network_bytes += bytes;
    }
    validated->maximum_send_bytes =
        std::max(validated->maximum_send_bytes, send_total);
  }
  for (const uint64_t receive_total : receives) {
    validated->maximum_receive_bytes =
        std::max(validated->maximum_receive_bytes, receive_total);
  }
  if (validated->maximum_send_bytes != model.config.minimum_message_bytes ||
      model.config.minimum_message_bytes != model.config.maximum_message_bytes) {
    Reject(
        contract,
        "HCCL_ROUTING_MODEL_DOMAIN_MISMATCH",
        "The routing payload is outside the exact AllToAllV model domain.",
        "Match the model message point to the maximum per-rank send bytes.");
    return false;
  }
  return true;
}

bool LoadAscendResources(
    const JsonValue& root,
    const JsonValue& profile_reference,
    AnalyticalRunContract* contract) {
  const ArtifactLoadPolicy profile_policy = {
      "DEVICE_PROFILE_REFERENCE_INVALID",
      "The Ascend Profile reference is invalid.",
      "Provide device_profile.path and its SHA-256 digest.",
      "DEVICE_PROFILE_NOT_FOUND",
      "The Ascend Profile could not be read.",
      "Provide a readable public Ascend Profile artifact.",
      "DEVICE_PROFILE_DIGEST_MISMATCH",
      "The Ascend Profile does not match its declared digest.",
      "Use the intended immutable Profile or update its digest.",
      "DEVICE_PROFILE_INVALID_JSON",
      "The Ascend Profile is not valid JSON.",
      "Correct the Profile JSON and retry."};
  LoadedArtifact profile_artifact;
  if (!LoadArtifact(
          profile_reference, profile_policy, contract, &profile_artifact)) {
    return false;
  }
  contract->device_profile_sha256 = profile_artifact.sha256;
  contract->topology_required = true;

  ValidatedAscendProfile profile;
  if (!ValidateAscendProfile(
          profile_artifact.document, contract, &profile)) {
    return false;
  }
  contract->topology_readiness = "READY";

  const JsonValue* model_reference = Member(root, "collective_cost_model");
  if (model_reference == nullptr) {
    RejectUnsupported(
        contract,
        "HCCL_COST_MODEL_REQUIRED",
        "Ascend Analytical requires an explicit HCCL cost model.",
        "Provide collective_cost_model.path and its SHA-256 digest.");
    return false;
  }
  const ArtifactLoadPolicy model_policy = {
      "HCCL_COST_MODEL_REFERENCE_INVALID",
      "The HCCL cost model reference is invalid.",
      "Provide collective_cost_model.path and its SHA-256 digest.",
      "HCCL_COST_MODEL_NOT_FOUND",
      "The HCCL cost model could not be read.",
      "Provide a readable public HCCL cost model artifact.",
      "HCCL_COST_MODEL_DIGEST_MISMATCH",
      "The HCCL cost model does not match its declared digest.",
      "Use the intended DerivedCostModel or update its digest.",
      "HCCL_COST_MODEL_INVALID_JSON",
      "The HCCL cost model is not valid JSON.",
      "Correct the cost model JSON and retry."};
  LoadedArtifact model_artifact;
  if (!LoadArtifact(
          *model_reference, model_policy, contract, &model_artifact)) {
    return false;
  }
  contract->cost_model_sha256 = model_artifact.sha256;

  ValidatedHcclCostModel model;
  if (!ValidateHcclCostModel(
          model_artifact.document,
          profile_artifact.sha256,
          profile,
          contract,
          &model)) {
    return false;
  }

  if (model.collective == "ALL_TO_ALL_V") {
    contract->routing_required = true;
    const JsonValue* routing_reference = Member(root, "routing");
    if (routing_reference == nullptr) {
      RejectUnsupported(
          contract,
          "HCCL_ROUTING_REQUIRED",
          "AllToAllV Analytical requires immutable routing counts.",
          "Provide routing.path and its SHA-256 digest.");
      return false;
    }
    const ArtifactLoadPolicy routing_policy = {
        "HCCL_ROUTING_REFERENCE_INVALID",
        "The HCCL routing reference is invalid.",
        "Provide routing.path and its SHA-256 digest.",
        "HCCL_ROUTING_NOT_FOUND",
        "The HCCL routing artifact could not be read.",
        "Provide a readable public routing artifact.",
        "HCCL_ROUTING_DIGEST_MISMATCH",
        "The HCCL routing artifact does not match its declared digest.",
        "Use the intended immutable routing artifact.",
        "HCCL_ROUTING_INVALID_JSON",
        "The HCCL routing artifact is not valid JSON.",
        "Correct the routing JSON and retry."};
    LoadedArtifact routing_artifact;
    if (!LoadArtifact(
            *routing_reference,
            routing_policy,
            contract,
            &routing_artifact,
            kMaximumRoutingArtifactBytes,
            "HCCL_ROUTING_ARTIFACT_TOO_LARGE",
            "The dense HCCL routing artifact exceeds the pre-parse byte limit.",
            "Use a JSON routing artifact no larger than 1 MiB.")) {
      return false;
    }
    contract->routing_sha256 = routing_artifact.sha256;
    if (routing_artifact.sha256 != model.routing_digest) {
      Reject(
          contract,
          "HCCL_ROUTING_MODEL_DIGEST_MISMATCH",
          "The DerivedCostModel references a different routing artifact.",
          "Bind the model and Run Manifest to the same routing digest.");
      return false;
    }
    ValidatedRouting routing;
    if (!ValidateA2AVRouting(
            routing_artifact.document,
            profile_artifact.sha256,
            profile,
            model,
            contract,
            &routing)) {
      return false;
    }
    model.config.routing_digest = routing_artifact.sha256;
    model.config.routing_total_traffic_bytes =
        routing.total_network_bytes;
    model.config.routing_max_receive_bytes =
        routing.maximum_receive_bytes;
    contract->routing_evidence_level = routing.evidence_level;
    contract->routing_field_readiness = routing.field_readiness;
  }

  const ArtifactLoadPolicy raw_policy = {
      "RAW_OBSERVATION_REFERENCE_INVALID",
      "The DerivedCostModel RawObservation reference is invalid.",
      "Reference one immutable RawObservation by path and SHA-256.",
      "RAW_OBSERVATION_NOT_FOUND",
      "The referenced RawObservation could not be read.",
      "Provide the immutable public RawObservation artifact.",
      "RAW_OBSERVATION_DIGEST_MISMATCH",
      "The RawObservation does not match its declared digest.",
      "Restore the immutable RawObservation or derive a new model.",
      "RAW_OBSERVATION_INVALID_JSON",
      "The RawObservation is not valid JSON.",
      "Correct the RawObservation JSON and derive a new model."};
  ValidatedRawObservation raw;
  for (size_t index = 0; index < model.raw_references.size(); ++index) {
    LoadedArtifact raw_artifact;
    if (!LoadArtifact(
            model.raw_references[index],
            raw_policy,
            contract,
            &raw_artifact)) {
      return false;
    }
    if (index == 0U) {
      contract->raw_observation_sha256 = raw_artifact.sha256;
    }
    model.input_sample_id = model.input_sample_ids[index];
    ValidatedRawObservation current_raw;
    if (!ValidateRawObservation(
            raw_artifact.document,
            profile_artifact.sha256,
            profile,
            model,
            contract,
            &current_raw)) {
      return false;
    }
    if (index == 0U) {
      raw = current_raw;
    }
  }

  contract->hccl_cost_model = model.config;
  contract->ascend_rank_count = profile.rank_count;
  contract->topology_domain = profile.topology_domain;
  contract->topology_digest = profile.topology_digest;
  contract->profile_evidence_level = profile.evidence_level;
  contract->profile_field_readiness = profile.field_readiness;
  contract->profile_evidence_ref = profile.evidence_ref;
  contract->ascend_profile_id = profile.id;
  contract->profile_hardware_available = profile.hardware_available;
  contract->raw_observation_evidence_level = raw.evidence_level;
  contract->raw_observation_field_readiness = raw.field_readiness;
  contract->raw_observation_evidence_ref = raw.evidence_ref;
  contract->raw_observation_hardware_available = raw.hardware_available;
  contract->cost_model_evidence_level = model.evidence_level;
  contract->cost_model_field_readiness = model.field_readiness;
  contract->cost_model_evidence_ref = model.evidence_ref;
  contract->cost_model_hardware_available = model.hardware_available;
  contract->hccl_group_id = model.group_id;
  contract->hccl_group_members = model.group_members;
  contract->hccl_group_membership_sha256 =
      model.group_membership_sha256;
  contract->ascend_profiled = true;
  return true;
}
std::string JsonEscape(const std::string& input) {
  std::ostringstream escaped;
  for (const unsigned char character : input) {
    switch (character) {
      case '"':
        escaped << "\\\"";
        break;
      case '\\':
        escaped << "\\\\";
        break;
      case '\b':
        escaped << "\\b";
        break;
      case '\f':
        escaped << "\\f";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        if (character < 0x20U) {
          escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<int>(character) << std::dec;
        } else {
          escaped << static_cast<char>(character);
        }
    }
  }
  return escaped.str();
}

std::string Quote(const std::string& value) {
  return "\"" + JsonEscape(value) + "\"";
}

enum class LegacyWorkloadCollectiveCheck {
  NoAllToAllV,
  HasAllToAllV,
  InvalidAllToAllVVariant,
  Malformed,
};

LegacyWorkloadCollectiveCheck CheckLegacyWorkloadCollectives(
    const std::string& workload_snapshot) {
  std::istringstream input(workload_snapshot);
  std::string header;
  std::string layer_count_line;
  if (!input || !std::getline(input, header) ||
      !std::getline(input, layer_count_line)) {
    return LegacyWorkloadCollectiveCheck::Malformed;
  }
  AstraSim::WorkloadLayerRecordFormat record_format;
  AstraSim::TargetWorkloadEventBinding target_binding;
  if (!AstraSim::DecodeWorkloadHeader(
          header, &record_format, &target_binding) ||
      target_binding.present) {
    return LegacyWorkloadCollectiveCheck::Malformed;
  }
  std::istringstream count_input(layer_count_line);
  int layer_count = 0;
  std::string count_suffix;
  if (!(count_input >> layer_count) || layer_count < 0 ||
      (count_input >> count_suffix)) {
    return LegacyWorkloadCollectiveCheck::Malformed;
  }

  bool has_alltoallv = false;
  for (int layer = 0; layer < layer_count; ++layer) {
    std::string layer_line;
    if (!std::getline(input, layer_line)) {
      return LegacyWorkloadCollectiveCheck::Malformed;
    }
    std::istringstream layer_input(layer_line);
    AstraSim::DecodedWorkloadLayerRecord record;
    std::string layer_suffix;
    if (!AstraSim::DecodeWorkloadLayerRecord(
            layer_input, record_format, &record) ||
        (layer_input >> layer_suffix)) {
      return LegacyWorkloadCollectiveCheck::Malformed;
    }
    const std::array<std::string, 3> collective_fields = {{
        record.forward_collective,
        record.input_gradient_collective,
        record.weight_gradient_collective,
    }};
    for (const std::string& field : collective_fields) {
      const AstraSim::WorkloadAllToAllVToken decoded =
          AstraSim::DecodeWorkloadAllToAllVToken(field);
      if (decoded == AstraSim::WorkloadAllToAllVToken::InvalidVariant) {
        return LegacyWorkloadCollectiveCheck::InvalidAllToAllVVariant;
      }
      has_alltoallv = has_alltoallv ||
          AstraSim::IsSupportedWorkloadAllToAllVToken(decoded);
    }
  }
  return has_alltoallv ? LegacyWorkloadCollectiveCheck::HasAllToAllV
                       : LegacyWorkloadCollectiveCheck::NoAllToAllV;
}

}  // namespace

AnalyticalRunContract LoadAnalyticalRunContract(int argc, char* argv[]) {
  AnalyticalRunContract contract;
  contract.binary_path = ResolveCurrentExecutable(argc > 0 ? argv[0] : "");

  bool contract_requested = false;
  for (int index = 1; index < argc; ++index) {
    if (std::string(argv[index]) == "--run-manifest") {
      contract_requested = true;
      break;
    }
  }
  if (!contract_requested) {
    return contract;
  }
  contract.enabled = true;
  bool invalid_cli = false;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--run-manifest" && index + 1 < argc &&
        contract.run_manifest_path.empty()) {
      contract.run_manifest_path = argv[++index];
    } else if (argument == "--result-manifest" && index + 1 < argc &&
               contract.result_manifest_path.empty()) {
      contract.result_manifest_path = argv[++index];
    } else {
      invalid_cli = true;
    }
  }
  contract.binary_sha256 = FileSha256(contract.binary_path);
  if (invalid_cli || contract.run_manifest_path.empty() ||
      contract.result_manifest_path.empty()) {
    Reject(
        &contract,
        "RUN_CONTRACT_CLI_INVALID",
        "The Run Contract invocation is invalid.",
        "Use only --run-manifest <file> and --result-manifest <file>.");
    return contract;
  }

  std::string manifest_content;
  if (!ReadFile(contract.run_manifest_path, &manifest_content)) {
    Reject(
        &contract,
        "RUN_MANIFEST_NOT_FOUND",
        "The Run Manifest could not be read.",
        "Provide a readable public Run Manifest file.");
    return contract;
  }
  contract.run_manifest_sha256 = "sha256:" + Sha256Hex(manifest_content);

  JsonValue root;
  try {
    root = JsonParser(manifest_content).Parse();
  } catch (const std::exception&) {
    Reject(
        &contract,
        "RUN_MANIFEST_INVALID_JSON",
        "The Run Manifest is not valid JSON.",
        "Correct the JSON syntax and retry.");
    return contract;
  }
  if (root.type != JsonValue::Type::Object) {
    Reject(
        &contract,
        "RUN_MANIFEST_INVALID_JSON",
        "The Run Manifest root must be an object.",
        "Use a JSON object as the Run Manifest root.");
    return contract;
  }

  std::string declared_schema_version;
  if (!StringMember(root, "schema_version", &declared_schema_version)) {
    Reject(
        &contract,
        "RUN_SCHEMA_VERSION_MISSING",
        "The Run Manifest has no string schema_version.",
        "Set schema_version to simai.run/v1.");
    return contract;
  }
  if (declared_schema_version != kRunSchemaVersion) {
    Reject(
        &contract,
        "RUN_SCHEMA_VERSION_UNSUPPORTED",
        "The Run Manifest schema version is unsupported.",
        "Use schema_version simai.run/v1.");
    return contract;
  }
  contract.schema_version = declared_schema_version;
  if (!StringMember(root, "run_id", &contract.run_id) ||
      !IsSafeRunId(contract.run_id)) {
    contract.run_id = "UNKNOWN";
    Reject(
        &contract,
        "RUN_ID_INVALID",
        "run_id is missing or unsafe.",
        "Use 1-64 ASCII letters, digits, dot, underscore, or hyphen.");
    return contract;
  }
  if (!StringMember(root, "backend", &contract.backend) ||
      contract.backend != "analytical") {
    contract.backend = "analytical";
    Reject(
        &contract,
        "BACKEND_UNSUPPORTED",
        "This binary accepts only the analytical backend.",
        "Set backend to analytical for SimAI_analytical.");
    return contract;
  }

  const JsonValue* workload = Member(root, "workload");
  if (workload == nullptr ||
      !StringMember(*workload, "path", &contract.workload_path) ||
      contract.workload_path.empty()) {
    Reject(
        &contract,
        "WORKLOAD_REFERENCE_MISSING",
        "The Run Manifest has no workload path.",
        "Provide workload.path for the legacy SimAI workload.");
    return contract;
  }
  std::string declared_workload_sha256;
  if (!StringMember(*workload, "sha256", &declared_workload_sha256) ||
      !IsSha256Identifier(declared_workload_sha256)) {
    Reject(
        &contract,
        "WORKLOAD_DIGEST_INVALID",
        "The workload reference has no valid SHA-256 digest.",
        "Set workload.sha256 to sha256:<64 lowercase hex digits>.");
    return contract;
  }
  if (!ReadFile(contract.workload_path, &contract.workload_snapshot)) {
    Reject(
        &contract,
        "WORKLOAD_NOT_FOUND",
        "The referenced workload could not be read.",
        "Provide a readable public workload artifact.");
    return contract;
  }
  contract.workload_sha256 =
      "sha256:" + Sha256Hex(contract.workload_snapshot);
  if (declared_workload_sha256 != contract.workload_sha256) {
    Reject(
        &contract,
        "WORKLOAD_DIGEST_MISMATCH",
        "The workload content does not match its declared SHA-256 digest.",
        "Use the intended immutable workload or update its declared digest.");
    return contract;
  }
  contract.workload_digest_verified = true;

  const JsonValue* a2_ground_truth = Member(root, "a2_ground_truth");
  contract.a2_ground_truth_present = a2_ground_truth != nullptr;
  if (a2_ground_truth != nullptr &&
      (a2_ground_truth->type != JsonValue::Type::Object ||
       !ValidateA2GroundTruth(*a2_ground_truth, root, &contract))) {
    return contract;
  }

  const JsonValue* target_workload = Member(root, "target_workload");
  contract.target_workload_present = target_workload != nullptr;
  if (target_workload != nullptr) {
    if (target_workload->type != JsonValue::Type::Object ||
        !ValidateTargetWorkloadEnvelope(*target_workload, &contract) ||
        !LoadTargetModel(*target_workload, &contract) ||
        !LoadTargetStep(*target_workload, &contract) ||
        !LoadTargetRouting(*target_workload, &contract) ||
        !LoadTargetMemoryPlan(*target_workload, &contract) ||
        !ValidateTargetWorkloadComposition(*target_workload, &contract) ||
        !ApplyTargetMemoryGates(&contract)) {
      return contract;
    }
  }

  const JsonValue* device_profile = Member(root, "device_profile");
  const JsonValue* legacy_gpu = Member(root, "legacy_gpu");
  contract.device_profile_present = device_profile != nullptr;
  if (device_profile != nullptr) {
    std::string declared_profile_sha256;
    if (StringMember(
            *device_profile, "sha256", &declared_profile_sha256) &&
        IsSha256Identifier(declared_profile_sha256)) {
      contract.device_profile_sha256 = declared_profile_sha256;
    }
  }
  if (device_profile != nullptr && legacy_gpu != nullptr) {
    Reject(
        &contract,
        "DEVICE_SELECTOR_CONFLICT",
        "The Run Manifest selects both device_profile and legacy_gpu.",
        "Remove one device selector; the two modes are mutually exclusive.");
    return contract;
  }
  if (device_profile == nullptr && Member(root, "collective_cost_model") != nullptr) {
    Reject(
        &contract,
        "ASCEND_PROFILE_REQUIRED",
        "An HCCL cost model cannot run without an Ascend Profile.",
        "Provide device_profile.path and its SHA-256 digest.");
    return contract;
  }
  if (device_profile != nullptr) {
    if (!LoadAscendResources(root, *device_profile, &contract)) {
      return contract;
    }
    if (contract.a2_ground_truth_ready) {
      if (contract.a2_bound_workload_sha256 != contract.workload_sha256) {
        Reject(
            &contract,
            "A2_WORKLOAD_BINDING_MISMATCH",
            "The A2 Run is bound to a different workload digest.",
            "Select the exact frozen workload artifact.");
        return contract;
      }
      if (contract.a2_bound_profile_id != contract.ascend_profile_id ||
          contract.a2_bound_profile_sha256 != contract.device_profile_sha256 ||
          contract.a2_bound_profile_evidence_level !=
              contract.profile_evidence_level ||
          contract.a2_bound_profile_field_readiness !=
              contract.profile_field_readiness ||
          contract.a2_bound_profile_evidence_ref !=
              contract.profile_evidence_ref) {
        Reject(
            &contract,
            "A2_PROFILE_BINDING_MISMATCH",
            "The A2 Run Profile identity or evidence binding differs from the selected Profile.",
            "Bind the exact Profile digest, evidence class, readiness, and evidenceRef.");
        return contract;
      }
      if (contract.ascend_rank_count != 8 ||
          contract.a2_bound_topology_sha256 != contract.topology_digest) {
        Reject(
            &contract,
            "A2_TOPOLOGY_BINDING_MISMATCH",
            "The selected Profile is not the frozen world-eight topology.",
            "Use the frozen eight-rank mapping digest.");
        return contract;
      }
      if (contract.a2_bound_group_id != contract.hccl_group_id ||
          contract.a2_bound_group_type !=
              AstraSim::CostedGroupTypeName(
                  contract.hccl_cost_model.group_type) ||
          contract.a2_bound_group_rank_count !=
              contract.hccl_cost_model.rank_count ||
          contract.a2_bound_group_members != contract.hccl_group_members ||
          contract.a2_bound_group_membership_sha256 !=
              contract.hccl_group_membership_sha256) {
        Reject(
            &contract,
            "A2_HCCL_GROUP_BINDING_MISMATCH",
            "The model/raw HCCL subgroup differs from the frozen Run membership.",
            "Use one legal frozen TP, DP, or EP subgroup in Run, model, and raw data.");
        return contract;
      }
      contract.a2_calibration_eligible =
          contract.a2_calibration_eligible &&
          contract.a2_bound_profile_evidence_level == "MEASURED" &&
          contract.a2_bound_profile_field_readiness == "FIELD_VERIFIED" &&
          contract.profile_field_readiness == "FIELD_VERIFIED" &&
          contract.profile_evidence_level == "MEASURED" &&
          contract.profile_hardware_available &&
          contract.raw_observation_evidence_level == "MEASURED" &&
          contract.raw_observation_field_readiness == "FIELD_VERIFIED" &&
          contract.raw_observation_hardware_available &&
          contract.cost_model_evidence_level == "DERIVED" &&
          contract.cost_model_field_readiness == "FIELD_VERIFIED" &&
          contract.cost_model_hardware_available &&
          contract.a2_run_evidence_ref != "UNKNOWN" &&
          contract.a2_result_evidence_ref != "UNKNOWN" &&
          contract.profile_evidence_ref != "UNKNOWN" &&
          contract.raw_observation_evidence_ref != "UNKNOWN" &&
          contract.cost_model_evidence_ref != "UNKNOWN";
    }
    contract.accepted = true;
    contract.exit_code = 0;
    contract.status = "VALID";
    contract.reject_code = "NONE";
    contract.message = "The Ascend HCCL Analytical run completed.";
    contract.remediation = "NONE";
    return contract;
  }
  double nvlink_bandwidth = 0.0;
  double nic_bandwidth = 0.0;
  std::string declared_gpu_type;
  if (legacy_gpu == nullptr || legacy_gpu->type != JsonValue::Type::Object ||
      !PositiveIntMember(
          *legacy_gpu, "gpu_count", &contract.legacy_gpu.gpu_count) ||
      !PositiveIntMember(
          *legacy_gpu,
          "gpus_per_server",
          &contract.legacy_gpu.gpus_per_server) ||
      !PositiveIntMember(
          *legacy_gpu,
          "nics_per_server",
          &contract.legacy_gpu.nics_per_server) ||
      !StringMember(*legacy_gpu, "gpu_type", &declared_gpu_type) ||
      !NumberMember(*legacy_gpu, "nvlink_bandwidth_GBps", &nvlink_bandwidth) ||
      !NumberMember(*legacy_gpu, "nic_bandwidth_GBps", &nic_bandwidth) ||
      nvlink_bandwidth <= 0.0 || nic_bandwidth <= 0.0 ||
      !ParseGpuType(declared_gpu_type, &contract.legacy_gpu.gpu_type) ||
      contract.legacy_gpu.gpu_count % contract.legacy_gpu.gpus_per_server != 0) {
    Reject(
        &contract,
        "LEGACY_GPU_CONFIG_INVALID",
        "The legacy_gpu configuration is missing or invalid.",
        "Provide positive topology/bandwidth fields and a supported GPU type.");
    return contract;
  }
  contract.legacy_gpu.nvlink_bandwidth_GBps = nvlink_bandwidth;
  contract.legacy_gpu.nic_bandwidth_GBps = nic_bandwidth;
  const LegacyWorkloadCollectiveCheck collective_check =
      contract.target_workload_present
      ? LegacyWorkloadCollectiveCheck::NoAllToAllV
      : CheckLegacyWorkloadCollectives(contract.workload_snapshot);
  if (collective_check ==
      LegacyWorkloadCollectiveCheck::InvalidAllToAllVVariant) {
    Reject(
        &contract,
        "LEGACY_COLLECTIVE_TOKEN_INVALID",
        "The workload contains an unknown AllToAllV collective token.",
        "Use ALLTOALLV, ALLTOALLV_EP, or ALLTOALLV_DP_EP exactly.");
    return contract;
  }
  if (collective_check == LegacyWorkloadCollectiveCheck::Malformed) {
    Reject(
        &contract,
        "LEGACY_WORKLOAD_FORMAT_INVALID",
        "The legacy workload layer records are malformed.",
        "Provide exactly three communication fields per workload layer.");
    return contract;
  }
  if (collective_check == LegacyWorkloadCollectiveCheck::HasAllToAllV) {
    RejectUnsupported(
        &contract,
        "LEGACY_ALLTOALLV_UNSUPPORTED",
        "AllToAllV requires a validated HCCL model and routing artifact.",
        "Use an Ascend Profile, HCCL model, and immutable routing matrix.");
    return contract;
  }

  contract.accepted = true;
  contract.exit_code = 0;
  contract.status = "VALID";
  contract.reject_code = "NONE";
  contract.message = "The analytical legacy GPU run completed.";
  contract.remediation = "NONE";
  return contract;
}

bool WriteAnalyticalResultManifest(
    const AnalyticalRunContract& contract,
    bool execution_succeeded,
    const AstraSim::CollectiveCostModel* cost_model) {
  if (contract.result_manifest_path.empty()) {
    return false;
  }
  std::ofstream output(
      contract.result_manifest_path.c_str(),
      std::ios::out | std::ios::trunc | std::ios::binary);
  if (!output) {
    return false;
  }

  const AstraSim::CollectiveCostSummary cost_summary =
      cost_model == nullptr ? AstraSim::CollectiveCostSummary()
                            : cost_model->Summary();
  const bool ascend_cost_valid = !contract.ascend_profiled ||
      (cost_summary.has_estimate && !cost_summary.unsupported_request);
  const bool valid =
      contract.accepted && execution_succeeded && ascend_cost_valid;
  const bool runtime_domain_miss = contract.accepted &&
      contract.ascend_profiled && execution_succeeded && !ascend_cost_valid;
  const bool multiple_requests_unsupported = runtime_domain_miss &&
      cost_summary.unsupported_reason == "MULTIPLE_REQUESTS_UNSUPPORTED";
  const std::string status = runtime_domain_miss
      ? "UNSUPPORTED"
      : (valid ? "VALID" : contract.status);
  const std::string reject_code = runtime_domain_miss
      ? (multiple_requests_unsupported
             ? "HCCL_MULTIPLE_REQUESTS_UNSUPPORTED"
             : "HCCL_MODEL_DOMAIN_MISS")
      : (valid ? "NONE" : contract.reject_code);
  const std::string message = runtime_domain_miss
      ? (multiple_requests_unsupported
             ? "The Result schema supports one HCCL request per run."
             : "The workload collective is outside the HCCL model domain.")
      : (valid
             ? (contract.ascend_profiled
                    ? "The Ascend HCCL Analytical run completed."
                    : "The analytical legacy GPU run completed.")
             : contract.message);
  const std::string remediation = runtime_domain_miss
      ? (multiple_requests_unsupported
             ? "Split the workload into one-request Analytical runs."
             : "Use a workload collective covered by the exact model domain.")
      : (valid ? "NONE" : contract.remediation);
  const std::string accelerator =
      contract.ascend_profiled
          ? "ASCEND_PROFILED"
          : (contract.legacy_gpu.gpu_count > 0 ? "LEGACY_GPU" : "UNKNOWN");
  const std::string cost_model_identity =
      valid && contract.ascend_profiled
          ? "HCCL_DERIVED"
          : (valid && !contract.device_profile_present ? "LEGACY_CALBUSBW"
                                                       : "UNKNOWN");
  const std::string workload_readiness = contract.workload_digest_verified
      ? "READY"
      : (contract.workload_sha256 == "UNKNOWN" ? "UNKNOWN" : "BLOCKED");

  output << "{\n"
         << "  \"schema_version\": " << Quote(kResultSchemaVersion) << ",\n"
         << "  \"run_schema_version\": " << Quote(contract.schema_version)
         << ",\n"
         << "  \"run_id\": " << Quote(contract.run_id) << ",\n"
         << "  \"backend\": \"analytical\",\n"
         << "  \"status\": " << Quote(status) << ",\n"
         << "  \"reject_code\": " << Quote(reject_code) << ",\n"
         << "  \"message\": " << Quote(message) << ",\n"
         << "  \"remediation\": " << Quote(remediation) << ",\n"
         << "  \"input_summary\": {\n"
         << "    \"run_manifest_sha256\": "
         << Quote(contract.run_manifest_sha256) << ",\n"
         << "    \"workload_sha256\": " << Quote(contract.workload_sha256)
         << ",\n"
         << "    \"accelerator\": " << Quote(accelerator) << ",\n";
  if (contract.ascend_profiled) {
    output << "    \"gpu_count\": " << contract.ascend_rank_count << ",\n";
  } else if (contract.legacy_gpu.gpu_count > 0) {
    output << "    \"gpu_count\": " << contract.legacy_gpu.gpu_count << ",\n";
  } else {
    output << "    \"gpu_count\": \"UNKNOWN\",\n";
  }
  output << "    \"target_workload_sha256\": "
         << Quote(contract.target_workload_sha256) << "\n"
         << "  },\n"
         << "  \"provenance\": {\n"
         << "    \"source_repository\": \"SimAI-Ascend\",\n"
         << "    \"source_revision\": \"UNKNOWN\",\n"
         << "    \"backend_binary\": \"SimAI_analytical\",\n"
         << "    \"binary_sha256\": " << Quote(contract.binary_sha256) << ",\n"
         << "    \"workload_sha256\": " << Quote(contract.workload_sha256)
         << ",\n"
         << "    \"device_profile_sha256\": "
         << Quote(contract.device_profile_sha256) << ",\n"
         << "    \"cost_model\": " << Quote(cost_model_identity) << ",\n"
         << "    \"cost_model_adapter\": "
         << Quote(contract.ascend_profiled
                      ? contract.hccl_cost_model.source_adapter
                      : "NOT_APPLICABLE")
         << ",\n"
         << "    \"cost_model_sha256\": "
         << Quote(contract.cost_model_sha256) << ",\n"
         << "    \"raw_observation_sha256\": "
         << Quote(contract.raw_observation_sha256) << ",\n"
         << "    \"routing_sha256\": "
         << Quote(contract.routing_sha256) << ",\n"
         << "    \"target_model_sha256\": "
         << Quote(contract.target_model_sha256) << ",\n"
         << "    \"target_step_sha256\": "
         << Quote(contract.target_step_sha256) << ",\n"
         << "    \"target_routing_sha256\": "
         << Quote(contract.target_routing_sha256) << ",\n"
         << "    \"target_memory_event_plan_sha256\": "
         << Quote(contract.target_memory_event_plan_sha256) << ",\n"
         << "    \"target_workload_sha256\": "
         << Quote(contract.target_workload_sha256) << ",\n"
         << "    \"a2_ground_truth_run_sha256\": "
         << Quote(contract.a2_ground_truth_run_sha256) << ",\n"
         << "    \"a2_ground_truth_result_sha256\": "
         << Quote(contract.a2_ground_truth_result_sha256) << "\n"
         << "  },\n"
         << "  \"evidence\": {\n"
         << "    \"workload\": {\"level\": "
         << Quote(contract.workload_sha256 == "UNKNOWN" ? "UNKNOWN"
                                                          : "USER_PROVIDED")
         << ", \"digest\": "
         << Quote(contract.workload_sha256) << "},\n"
         << "    \"device_profile\": {\"level\": "
         << Quote(contract.ascend_profiled
                      ? contract.profile_evidence_level
                      : "UNKNOWN")
         << ", \"digest\": " << Quote(contract.device_profile_sha256)
         << ", \"readiness\": " << Quote(contract.profile_field_readiness)
         << "},\n"
         << "    \"raw_observation\": {\"level\": "
         << Quote(contract.raw_observation_evidence_level)
         << ", \"digest\": " << Quote(contract.raw_observation_sha256)
         << ", \"readiness\": "
         << Quote(contract.raw_observation_field_readiness) << "},\n"
         << "    \"routing\": {\"level\": "
         << Quote(contract.routing_evidence_level)
         << ", \"digest\": " << Quote(contract.routing_sha256)
         << ", \"readiness\": "
         << Quote(contract.routing_field_readiness) << "},\n"
         << "    \"cost_model\": {\"level\": "
         << Quote(contract.cost_model_evidence_level)
         << ", \"digest\": " << Quote(contract.cost_model_sha256)
         << ", \"readiness\": "
         << Quote(contract.cost_model_field_readiness) << "},\n"
         << "    \"target_model\": {\"level\": "
         << Quote(contract.target_model_evidence_level)
         << ", \"digest\": " << Quote(contract.target_model_sha256)
         << ", \"readiness\": "
         << Quote(contract.target_model_field_readiness) << "},\n"
         << "    \"target_step\": {\"level\": "
         << Quote(contract.target_step_evidence_level)
         << ", \"digest\": " << Quote(contract.target_step_sha256)
         << ", \"readiness\": "
         << Quote(contract.target_step_field_readiness) << "},\n"
         << "    \"target_routing\": {\"level\": "
         << Quote(contract.target_routing_evidence_level)
         << ", \"digest\": " << Quote(contract.target_routing_sha256)
         << ", \"readiness\": "
         << Quote(contract.target_routing_field_readiness) << "},\n"
         << "    \"target_memory_event_plan\": {\"level\": "
         << Quote(contract.target_memory_evidence_level)
         << ", \"digest\": "
         << Quote(contract.target_memory_event_plan_sha256)
         << ", \"readiness\": "
         << Quote(contract.target_memory_field_readiness) << "},\n"
         << "    \"a2_ground_truth\": {\"level\": "
         << Quote(contract.a2_ground_truth_evidence_level)
         << ", \"digest\": " << Quote(contract.a2_ground_truth_result_sha256)
         << ", \"readiness\": "
         << Quote(contract.a2_ground_truth_field_readiness) << "}\n"
         << "  },\n"
         << "  \"readiness\": {\n"
         << "    \"contract\": " << Quote(valid ? "READY" : "BLOCKED") << ",\n"
         << "    \"workload\": " << Quote(workload_readiness) << ",\n"
         << "    \"analytical_backend\": "
         << Quote(valid ? "READY" : "BLOCKED") << ",\n"
         << "    \"ascend_profile\": "
         << Quote(contract.ascend_profiled && contract.accepted ? "READY"
                                                               : (contract.device_profile_present
                                                                      ? "BLOCKED"
                                                                      : "UNKNOWN"))
         << ",\n"
         << "    \"hccl_cost_model\": "
         << Quote(contract.ascend_profiled && ascend_cost_valid ? "READY"
                                                               : (contract.device_profile_present
                                                                      ? "BLOCKED"
                                                                      : "UNKNOWN"))
         << ",\n"
         << "    \"topology\": "
         << Quote(contract.topology_required ? contract.topology_readiness
                                             : "NOT_REQUIRED")
         << ",\n"
         << "    \"routing\": "
         << Quote(contract.routing_required
                      ? (contract.routing_sha256 == "UNKNOWN"
                             ? "UNKNOWN"
                             : (valid ? "READY" : "BLOCKED"))
                      : "NOT_REQUIRED")
         << ",\n"
         << "    \"target_model\": "
         << Quote(contract.target_workload_present
                      ? (contract.target_model_ready ? "READY" : "BLOCKED")
                      : "NOT_REQUIRED")
         << ",\n"
         << "    \"target_step\": "
         << Quote(contract.target_workload_present
                      ? (contract.target_step_ready ? "READY" : "BLOCKED")
                      : "NOT_REQUIRED")
         << ",\n"
         << "    \"target_routing\": "
         << Quote(contract.target_workload_present
                      ? (contract.target_routing_ready ? "READY" : "BLOCKED")
                      : "NOT_REQUIRED")
         << ",\n"
         << "    \"target_memory_event_plan\": "
         << Quote(contract.target_workload_present
                      ? (contract.target_memory_event_plan_ready
                             ? "READY"
                             : "BLOCKED")
                      : "NOT_REQUIRED")
         << ",\n"
         << "    \"target_workload\": "
         << Quote(contract.target_workload_present
                      ? (contract.target_workload_ready ? "READY" : "BLOCKED")
                      : "NOT_REQUIRED")
         << ",\n"
         << "    \"hbm\": "
         << Quote(contract.target_memory_materialized
                      ? (contract.target_memory_gate_failed ? "BLOCKED"
                                                           : "READY")
                      : "UNKNOWN")
         << ",\n"
         << "    \"traffic\": "
         << Quote(valid && contract.ascend_profiled ? "READY" : "UNKNOWN")
         << ",\n"
         << "    \"a2_ground_truth\": "
         << Quote(contract.a2_ground_truth_present
                      ? (contract.a2_ground_truth_ready ? "READY" : "BLOCKED")
                      : "NOT_REQUIRED")
         << "\n"
         << "  },\n"
         << "  \"results\": {\n"
         << "    \"validity\": " << Quote(valid ? "VALID" : "UNKNOWN") << ",\n"
         << "    \"timing_ns\": ";
  if (valid && contract.ascend_profiled) {
    output << cost_summary.total_duration_ns << ",\n";
  } else {
    output << "\"UNKNOWN\",\n";
  }
  output << "    \"hbm_peak_B\": ";
  if (contract.target_memory_materialized) {
    output << contract.target_memory_peak_B << ",\n";
  } else {
    output << "\"UNKNOWN\",\n";
  }
  output << "    \"memory\": ";
  if (contract.target_memory_symbolic) {
    output
        << "{\"unit\": \"B\", "
        << "\"aggregation\": \"CONSERVATIVE_COMPONENT_PEAK_SUM\", "
        << "\"bindings\": {\"precision\": \"UNBOUND\", "
        << "\"optimizer\": \"UNBOUND\", \"placement\": \"UNBOUND\", "
        << "\"recomputation\": \"UNBOUND\", \"runtime\": \"UNBOUND\"}, "
        << "\"components\": {"
        << "\"parameters\": {\"state\": \"SYMBOLIC\", \"unit\": \"B\", "
        << "\"value\": \"UNKNOWN\", \"expression\": "
        << Quote("logical trainable tensors * training parameter precision / parameter shards")
        << "}, \"gradients\": {\"state\": \"SYMBOLIC\", \"unit\": \"B\", "
        << "\"value\": \"UNKNOWN\", \"expression\": "
        << Quote("logical trainable tensors * gradient precision / gradient shards")
        << "}, \"optimizer_states\": {\"state\": \"SYMBOLIC\", \"unit\": \"B\", "
        << "\"value\": \"UNKNOWN\", \"expression\": "
        << Quote("optimizer state tensors and optional master weights / optimizer shards")
        << "}, \"activations\": {\"state\": \"SYMBOLIC\", \"unit\": \"B\", "
        << "\"value\": \"UNKNOWN\", \"expression\": "
        << Quote("saved activation shape trace after recomputation selection")
        << "}, \"communication_buffers\": {\"state\": \"SYMBOLIC\", "
        << "\"unit\": \"B\", \"value\": \"UNKNOWN\", \"expression\": "
        << Quote("dispatch combine collective and runtime scratch buffers")
        << "}, \"expert_placement\": {\"state\": \"SYMBOLIC\", "
        << "\"unit\": \"B\", \"value\": \"UNKNOWN\", \"expression\": "
        << Quote("local routed expert weights and maximum local expert load")
        << "}, \"recomputation\": {\"state\": \"SYMBOLIC\", "
        << "\"unit\": \"B\", \"value\": \"UNKNOWN\", \"expression\": "
        << Quote("recomputed activation and observed kernel workspace")
        << "}}, \"peak_per_rank_B\": \"UNKNOWN\", "
        << "\"search_95_percent_gate\": \"UNKNOWN\", "
        << "\"a2_a3_execution_85_percent_gate\": \"UNKNOWN\"},\n";
  } else if (contract.target_memory_materialized) {
    output
        << "{\"unit\": \"B\", "
        << "\"aggregation\": \"CONSERVATIVE_COMPONENT_PEAK_SUM\", "
        << "\"bindings\": {"
        << "\"precision\": {\"state\": \"BOUND\", \"sha256\": "
        << Quote(contract.target_precision_policy_sha256)
        << "}, \"optimizer\": {\"state\": \"BOUND\", \"sha256\": "
        << Quote(contract.target_optimizer_policy_sha256)
        << "}, \"placement\": {\"state\": \"BOUND\", \"sha256\": "
        << Quote(contract.target_placement_sha256)
        << "}, \"recomputation\": {\"state\": \"BOUND\", \"sha256\": "
        << Quote(contract.target_recomputation_policy_sha256)
        << "}, \"runtime\": {\"state\": \"BOUND\", \"sha256\": "
        << Quote(contract.target_runtime_profile_sha256)
        << "}}, \"components\": {"
        << "\"parameters\": {\"state\": \"MATERIALIZED\", \"unit\": \"B\", "
        << "\"value\": " << contract.target_memory_parameters_B
        << "}, \"gradients\": {\"state\": \"MATERIALIZED\", \"unit\": \"B\", "
        << "\"value\": " << contract.target_memory_gradients_B
        << "}, \"optimizer_states\": {\"state\": \"MATERIALIZED\", "
        << "\"unit\": \"B\", \"value\": "
        << contract.target_memory_optimizer_states_B
        << "}, \"activations\": {\"state\": \"MATERIALIZED\", "
        << "\"unit\": \"B\", \"value\": "
        << contract.target_memory_activations_B
        << "}, \"communication_buffers\": {\"state\": \"MATERIALIZED\", "
        << "\"unit\": \"B\", \"value\": "
        << contract.target_memory_communication_buffers_B
        << "}, \"expert_placement\": {\"state\": \"MATERIALIZED\", "
        << "\"unit\": \"B\", \"value\": "
        << contract.target_memory_expert_placement_B
        << "}, \"recomputation\": {\"state\": \"MATERIALIZED\", "
        << "\"unit\": \"B\", \"value\": "
        << contract.target_memory_recomputation_B
        << "}}, \"peak_per_rank_B\": " << contract.target_memory_peak_B
        << ", \"capacity\": {\"base_hbm_B\": "
        << contract.target_memory_base_hbm_B
        << ", \"reserve_hbm_B\": " << contract.target_memory_reserve_hbm_B
        << ", \"scenario_usable_hbm_B\": "
        << contract.target_memory_scenario_usable_hbm_B
        << "}, \"search_limit\": {\"percent\": 95, "
        << "\"denominator\": \"SCENARIO_USABLE_HBM_B\", "
        << "\"rounding\": \"FLOOR_INTEGER_BYTES\", \"maximum_allowed_B\": "
        << contract.target_memory_search_limit_B
        << "}, \"search_95_percent_gate\": "
        << Quote(contract.target_memory_search_gate)
        << ", \"execution_limit\": {\"percent\": 85, "
        << "\"denominator\": \"BASE_HBM_B\", "
        << "\"comparison\": \"STRICTLY_LESS_THAN_85_PERCENT\", "
        << "\"rounding\": \"MAXIMUM_ACCEPTED_INTEGER_BYTES\", "
        << "\"boundary_B\": " << contract.target_memory_execution_boundary_B
        << ", \"maximum_accepted_B\": "
        << contract.target_memory_execution_maximum_accepted_B
        << "}, \"observed_execution_peak_B\": ";
    if (contract.target_memory_execution_peak_known) {
      output << contract.target_memory_execution_peak_B;
    } else {
      output << "\"UNKNOWN\"";
    }
    output << ", \"a2_a3_execution_85_percent_gate\": "
           << Quote(contract.target_memory_execution_gate) << "},\n";
  } else {
    output << "\"UNKNOWN\",\n";
  }
  output << "    \"traffic_B\": ";
  if (valid && contract.ascend_profiled) {
    output << cost_summary.total_traffic_bytes << ",\n";
  } else {
    output << "\"UNKNOWN\",\n";
  }
  output << "    \"collective\": ";
  if (valid && contract.ascend_profiled) {
    output << "{\"operation\": "
           << Quote(AstraSim::CostedCollectiveName(cost_summary.collective))
           << ", \"message_bytes_per_rank\": "
           << cost_summary.message_bytes_per_rank
           << ", \"rank_count\": " << cost_summary.rank_count
           << ", \"group_type\": "
           << Quote(AstraSim::CostedGroupTypeName(cost_summary.group_type))
           << ", \"topology_domain\": "
           << Quote(cost_summary.topology_domain) << "},\n";
  } else {
    output << "\"UNKNOWN\",\n";
  }
  output << "    \"collective_payload\": ";
  if (valid && contract.ascend_profiled) {
    output << "{\"semantics\": " << Quote(cost_summary.payload_semantics)
           << ", \"input_B_per_rank\": "
           << cost_summary.input_bytes_per_rank
           << ", \"output_B_per_rank\": "
           << cost_summary.output_bytes_per_rank
           << ", \"routing_sha256\": "
           << Quote(cost_summary.routing_digest) << "},\n";
  } else {
    output << "\"UNKNOWN\",\n";
  }
  output << "    \"target_workload\": ";
  if (contract.target_model_ready) {
    output << "{\"model\": {\"logical_trainable_parameters\": "
           << contract.target_logical_trainable_parameters
           << ", \"parameter_unit\": \"count\""
           << ", \"routed_experts\": " << contract.target_routed_experts
           << ", \"top_k\": " << contract.target_top_k
           << ", \"expert_intermediate_size\": "
           << contract.target_expert_intermediate_size
           << ", \"shared_experts\": " << contract.target_shared_experts
           << ", \"active_logical_parameters\": {"
           << "\"main_blocks_only\": "
           << contract.target_active_main_blocks_parameters
           << ", \"main_forward_including_io\": "
           << contract.target_active_main_forward_parameters
           << ", \"training_graph_including_mtp\": "
           << contract.target_active_training_graph_parameters
           << ", \"unit\": \"count\"}"
           << ", \"checkpoint_storage\": {\"value\": "
           << contract.target_checkpoint_storage_bytes
           << ", \"unit\": \"B\", \"semantics\": "
           << "\"FIXED_QUANTIZED_CHECKPOINT_ONLY_NOT_TRAINING_HBM\""
           << ", \"used_as_training_hbm\": false}"
           << ", \"checkpoint_auxiliary_elements\": {"
           << "\"quant_scale\": "
           << contract.target_checkpoint_quant_scale_elements
           << ", \"routing_table\": "
           << contract.target_checkpoint_routing_table_elements
           << ", \"total\": "
           << contract.target_checkpoint_auxiliary_elements
           << ", \"unit\": \"count\"}"
           << "}, \"step\": ";
    if (contract.target_step_ready) {
      output << "{\"formula\": \"sequence * MBS * DP * GA\""
             << ", \"sequence_tokens\": " << contract.target_sequence_tokens
             << ", \"micro_batch_sequences\": "
             << contract.target_micro_batch_sequences
             << ", \"data_parallel_replicas\": "
             << contract.target_data_parallel_replicas
             << ", \"gradient_accumulation\": "
             << contract.target_gradient_accumulation
             << ", \"configured_gts\": " << contract.target_configured_gts
             << ", \"gts_limit\": 500000000"
             << ", \"configured_routed_assignment_slots_upper_bound\": "
             << contract.target_routed_assignment_slots
             << "}, \"aicb_execution_binding\": ";
      if (contract.target_workload_ready) {
        output << "{\"workload_sha256\": " << Quote(contract.workload_sha256)
               << ", \"model_sha256\": "
               << Quote(contract.target_model_sha256)
               << ", \"step_sha256\": "
               << Quote(contract.target_step_sha256)
               << ", \"routing_sha256\": "
               << Quote(contract.target_routing_sha256)
               << ", \"memory_event_plan_sha256\": "
               << Quote(contract.target_memory_event_plan_sha256)
               << ", \"target_workload_sha256\": "
               << Quote(contract.target_workload_sha256)
               << ", \"runtime_record_format\": "
               << Quote(contract.target_runtime_record_format)
               << ", \"runtime_specific_parallelism\": [";
        for (size_t layer = 0;
             layer < contract.target_runtime_specific_parallelism.size();
             ++layer) {
          if (layer != 0U) {
            output << ", ";
          }
          output << Quote(
              contract.target_runtime_specific_parallelism[layer]);
        }
        output << "]}},\n";
      } else {
        output << "\"UNKNOWN\"},\n";
      }
    } else {
      output << "\"UNKNOWN\", \"aicb_execution_binding\": \"UNKNOWN\"},\n";
    }
  } else {
    output << "\"UNKNOWN\",\n";
  }
  output << "    \"a2_ground_truth\": ";
  if (contract.a2_ground_truth_present) {
    output << "{\"status\": " << Quote(contract.a2_ground_truth_status)
           << ", \"calibration_eligible\": "
           << (contract.a2_calibration_eligible ? "true" : "false")
           << ", \"evidence\": "
           << Quote(contract.a2_ground_truth_evidence_level)
           << ", \"raw_observation_count\": "
           << contract.a2_raw_observation_count
           << ", \"derived_cost_model_sha256\": "
           << Quote(contract.a2_derived_cost_model_sha256)
           << ", \"scenarios\": [";
    for (size_t index = 0U; index < contract.a2_scenarios.size(); ++index) {
      if (index != 0U) {
        output << ", ";
      }
      const A2GroundTruthScenarioSummary& scenario =
          contract.a2_scenarios[index];
      output << "{\"id\": " << Quote(scenario.id)
             << ", \"sample_count\": " << scenario.sample_count
             << ", \"cv\": " << std::setprecision(17) << scenario.cv
             << ", \"median_step_time_ns\": "
             << scenario.median_step_time_ns
             << ", \"minimum_step_time_ns\": "
             << scenario.minimum_step_time_ns
             << ", \"maximum_step_time_ns\": "
             << scenario.maximum_step_time_ns
             << ", \"representative_statistic\": "
             << Quote(scenario.representative_statistic)
             << ", \"representative_step_time_ns\": "
             << scenario.representative_step_time_ns
             << ", \"peak_hbm_B\": " << scenario.peak_hbm_B << "}";
    }
    output << "]},\n";
  } else {
    output << "\"NOT_REQUIRED\",\n";
  }
  output
         << "    \"useful_throughput_tokens_per_s\": \"UNKNOWN\",\n"
         << "    \"top5\": \"UNKNOWN\",\n"
         << "    \"representatives\": \"UNKNOWN\",\n"
         << "    \"fault_goodput_tokens_per_s\": \"UNKNOWN\"\n"
         << "  }\n"
         << "}\n";
  output.flush();
  return output.good();
}

}  // namespace SimAIContract
