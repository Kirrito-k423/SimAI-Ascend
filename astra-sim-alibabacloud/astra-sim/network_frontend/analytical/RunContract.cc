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

bool BuildEvidenceClassIndex(
    const JsonValue& spec,
    std::map<std::string, std::string>* evidence_classes) {
  const JsonValue* records = Member(spec, "evidence");
  if (records == nullptr || records->type != JsonValue::Type::Array ||
      records->array.empty()) {
    return false;
  }
  for (const JsonValue& record : records->array) {
    std::string id;
    std::string evidence_class;
    if (!EvidenceRecordIsComplete(record) ||
        !StringMember(record, "id", &id) ||
        !StringMember(record, "class", &evidence_class) ||
        evidence_classes->count(id) != 0U) {
      return false;
    }
    (*evidence_classes)[id] = evidence_class;
  }
  return true;
}

bool ConsumedFieldEvidenceIsValid(
    const JsonValue& field,
    const std::map<std::string, std::string>& evidence_classes,
    bool* has_unverified_field) {
  std::string status;
  std::string evidence_ref;
  std::string readiness;
  if (!StringMember(field, "status", &status) || status != "KNOWN" ||
      !StringMember(field, "evidenceRef", &evidence_ref) ||
      !StringMember(field, "readiness", &readiness) ||
      (readiness != "FIELD_UNVERIFIED" && readiness != "FIELD_VERIFIED")) {
    return false;
  }
  const auto evidence = evidence_classes.find(evidence_ref);
  if (evidence == evidence_classes.end() ||
      (readiness == "FIELD_UNVERIFIED" && evidence->second == "MEASURED")) {
    return false;
  }
  *has_unverified_field =
      *has_unverified_field || readiness == "FIELD_UNVERIFIED";
  return true;
}

bool ConsumedTopologyEvidenceIsValid(
    const JsonValue& topology_level,
    const std::map<std::string, std::string>& evidence_classes,
    bool* has_unverified_field) {
  std::string evidence_ref;
  std::string readiness;
  if (!StringMember(topology_level, "evidenceRef", &evidence_ref) ||
      !StringMember(topology_level, "readiness", &readiness) ||
      (readiness != "FIELD_UNVERIFIED" && readiness != "FIELD_VERIFIED")) {
    return false;
  }
  const auto evidence = evidence_classes.find(evidence_ref);
  if (evidence == evidence_classes.end() ||
      (readiness == "FIELD_UNVERIFIED" && evidence->second == "MEASURED")) {
    return false;
  }
  *has_unverified_field =
      *has_unverified_field || readiness == "FIELD_UNVERIFIED";
  return true;
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

struct ValidatedAscendProfile {
  std::string id;
  int rank_count = 0;
  std::string topology_domain;
  std::string topology_digest;
  std::string evidence_level;
  std::string field_readiness;
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
  const JsonValue* first_evidence =
      spec == nullptr ? nullptr : FirstArrayObject(*spec, "evidence");

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
      schema_semver != "0.1.0" || metadata == nullptr ||
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
      !IsSha256Identifier(validated->topology_digest) ||
      first_evidence == nullptr ||
      !StringMember(
          *first_evidence, "class", &validated->evidence_level) ||
      !EvidenceRecordIsComplete(*first_evidence)) {
    Reject(
        contract,
        "DEVICE_PROFILE_SCHEMA_INVALID",
        "The Ascend Profile schema or canonical units are invalid.",
        "Use simai.ascend.profile/v1alpha1 with complete typed fields.");
    return false;
  }

  std::map<std::string, std::string> evidence_classes;
  bool has_unverified_field = false;
  if (!BuildEvidenceClassIndex(*spec, &evidence_classes) ||
      !ConsumedFieldEvidenceIsValid(
          *physical_chip_count, evidence_classes, &has_unverified_field) ||
      !ConsumedFieldEvidenceIsValid(
          *management_device_count, evidence_classes, &has_unverified_field) ||
      !ConsumedFieldEvidenceIsValid(
          *ranks_per_unit, evidence_classes, &has_unverified_field) ||
      !ConsumedFieldEvidenceIsValid(
          *peak_flops, evidence_classes, &has_unverified_field) ||
      !ConsumedFieldEvidenceIsValid(
          *hbm_capacity, evidence_classes, &has_unverified_field) ||
      !ConsumedFieldEvidenceIsValid(
          *hbm_bandwidth, evidence_classes, &has_unverified_field) ||
      !ConsumedTopologyEvidenceIsValid(
          *topology_level, evidence_classes, &has_unverified_field)) {
    Reject(
        contract,
        "DEVICE_PROFILE_FIELD_EVIDENCE_INVALID",
        "A consumed Profile field lacks resolved evidence or readiness.",
        "Use KNOWN values with valid readiness and a resolved evidenceRef.");
    return false;
  }
  validated->field_readiness =
      has_unverified_field ? "FIELD_UNVERIFIED" : "FIELD_VERIFIED";
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
  if (!StringMember(model, "apiVersion", &api_version) ||
      api_version != "simai.ascend.costmodel/v1alpha1" ||
      !StringMember(model, "kind", &kind) || kind != "HcclCostModel" ||
      !StringMember(model, "schemaSemver", &schema_semver) ||
      schema_semver != "0.1.0" || metadata == nullptr ||
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
      rank_count != profile.rank_count ||
      model_topology_domain != profile.topology_domain ||
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
      validated->field_readiness != "FIELD_UNVERIFIED" ||
      extrapolation == nullptr ||
      !BooleanMember(
          *extrapolation, "allowed", &extrapolation_allowed) ||
      extrapolation_allowed || !UnitsAreCanonical(model) ||
      ContainsString(model, "MEASURED")) {
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
      !StringMember(raw, "schemaSemver", &schema_semver) ||
      schema_semver != "0.1.0" || metadata == nullptr ||
      !StringMember(*metadata, "id", &raw_id) ||
      raw_id != model.input_sample_id || spec == nullptr ||
      !StringMember(*spec, "profileRef", &profile_ref) ||
      profile_ref != profile.id ||
      !StringMember(*spec, "profileDigest", &raw_profile_digest) ||
      raw_profile_digest != profile_digest ||
      !StringMember(*spec, "collective", &collective) ||
      collective != model.collective || group == nullptr ||
      !PositiveIntMember(*group, "rankCount", &rank_count) ||
      rank_count != model.config.rank_count ||
      !StringMember(*group, "scope", &scope) ||
      scope != profile.topology_domain ||
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
      validated->field_readiness != "FIELD_UNVERIFIED" ||
      !EvidenceRecordIsComplete(*first_evidence) || correctness == nullptr ||
      !StringMember(*correctness, "status", &correctness_status) ||
      correctness_status != "PASS" || eligibility == nullptr ||
      !BooleanMember(*eligibility, "fit", &eligible_for_fit) ||
      !eligible_for_fit ||
      (!has_canonical_metadata && !uses_issue17_metadata_compatibility) ||
      !UnitsAreCanonical(raw) ||
      ContainsString(raw, "MEASURED")) {
    Reject(
        contract,
        "RAW_OBSERVATION_SCHEMA_INVALID",
        "The immutable HCCL RawObservation is invalid or out of domain.",
        "Use a canonical-unit HCCL observation matching Profile and model.");
    return false;
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
  contract->raw_observation_evidence_level = raw.evidence_level;
  contract->raw_observation_field_readiness = raw.field_readiness;
  contract->cost_model_evidence_level = model.evidence_level;
  contract->cost_model_field_readiness = model.field_readiness;
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
    const std::string& path) {
  std::ifstream input(path.c_str());
  std::string header;
  std::string layer_count_line;
  if (!input || !std::getline(input, header) ||
      !std::getline(input, layer_count_line)) {
    return LegacyWorkloadCollectiveCheck::Malformed;
  }
  const AstraSim::WorkloadLayerRecordFormat record_format =
      AstraSim::DecodeWorkloadLayerRecordFormat(header);
  std::istringstream count_input(layer_count_line);
  int layer_count = 0;
  std::string count_suffix;
  if (!(count_input >> layer_count) || layer_count < 0 ||
      (count_input >> count_suffix)) {
    return LegacyWorkloadCollectiveCheck::Malformed;
  }

  bool has_alltoallv = false;
  for (int layer = 0; layer < layer_count; ++layer) {
    AstraSim::DecodedWorkloadLayerRecord record;
    if (!AstraSim::DecodeWorkloadLayerRecord(
            input, record_format, &record)) {
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
  contract.workload_sha256 = FileSha256(contract.workload_path);
  if (contract.workload_sha256 == "UNKNOWN") {
    Reject(
        &contract,
        "WORKLOAD_NOT_FOUND",
        "The referenced workload could not be read.",
        "Provide a readable public workload artifact.");
    return contract;
  }
  if (declared_workload_sha256 != contract.workload_sha256) {
    Reject(
        &contract,
        "WORKLOAD_DIGEST_MISMATCH",
        "The workload content does not match its declared SHA-256 digest.",
        "Use the intended immutable workload or update its declared digest.");
    return contract;
  }
  contract.workload_digest_verified = true;

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
      CheckLegacyWorkloadCollectives(contract.workload_path);
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
    output << "    \"gpu_count\": " << contract.ascend_rank_count << "\n";
  } else if (contract.legacy_gpu.gpu_count > 0) {
    output << "    \"gpu_count\": " << contract.legacy_gpu.gpu_count << "\n";
  } else {
    output << "    \"gpu_count\": \"UNKNOWN\"\n";
  }
  output << "  },\n"
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
         << Quote(contract.routing_sha256) << "\n"
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
         << Quote(contract.cost_model_field_readiness) << "}\n"
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
         << "    \"hbm\": \"UNKNOWN\",\n"
         << "    \"traffic\": "
         << Quote(valid && contract.ascend_profiled ? "READY" : "UNKNOWN")
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
  output << "    \"hbm_peak_B\": \"UNKNOWN\",\n"
         << "    \"traffic_B\": ";
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
