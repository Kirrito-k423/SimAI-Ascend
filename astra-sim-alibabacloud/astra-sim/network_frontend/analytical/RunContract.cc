/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#include "RunContract.h"

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

bool LoadAscendResources(
    const JsonValue& root,
    const JsonValue& profile_reference,
    AnalyticalRunContract* contract) {
  std::string profile_path;
  std::string declared_profile_digest;
  if (!ParseArtifactReference(
          profile_reference, &profile_path, &declared_profile_digest)) {
    Reject(
        contract,
        "DEVICE_PROFILE_REFERENCE_INVALID",
        "The Ascend Profile reference is invalid.",
        "Provide device_profile.path and its SHA-256 digest.");
    return false;
  }
  std::string profile_content;
  if (!ReadFile(profile_path, &profile_content)) {
    Reject(
        contract,
        "DEVICE_PROFILE_NOT_FOUND",
        "The Ascend Profile could not be read.",
        "Provide a readable public Ascend Profile artifact.");
    return false;
  }
  contract->device_profile_sha256 = "sha256:" + Sha256Hex(profile_content);
  if (contract->device_profile_sha256 != declared_profile_digest) {
    Reject(
        contract,
        "DEVICE_PROFILE_DIGEST_MISMATCH",
        "The Ascend Profile does not match its declared digest.",
        "Use the intended immutable Profile or update its digest.");
    return false;
  }
  JsonValue profile;
  if (!ParseJsonDocument(profile_content, &profile)) {
    Reject(
        contract,
        "DEVICE_PROFILE_INVALID_JSON",
        "The Ascend Profile is not valid JSON.",
        "Correct the Profile JSON and retry.");
    return false;
  }

  std::string api_version;
  std::string kind;
  std::string schema_semver;
  const JsonValue* profile_metadata = Member(profile, "metadata");
  const JsonValue* profile_spec = Member(profile, "spec");
  const JsonValue* identity =
      profile_spec == nullptr ? nullptr : Member(*profile_spec, "identity");
  const JsonValue* physical_chip_count =
      identity == nullptr ? nullptr : Member(*identity, "physicalChipCount");
  const JsonValue* management_device_count =
      identity == nullptr ? nullptr : Member(*identity, "managementDeviceCount");
  const JsonValue* rank_granularity = profile_spec == nullptr
      ? nullptr
      : Member(*profile_spec, "rankGranularity");
  const JsonValue* ranks_per_unit = rank_granularity == nullptr
      ? nullptr
      : Member(*rank_granularity, "trainingRanksPerUnit");
  const JsonValue* topology =
      profile_spec == nullptr ? nullptr : Member(*profile_spec, "topology");
  const JsonValue* topology_level =
      topology == nullptr ? nullptr : FirstArrayObject(*topology, "levels");
  const JsonValue* profile_evidence =
      profile_spec == nullptr ? nullptr : FirstArrayObject(*profile_spec, "evidence");
  std::string profile_id;
  std::string profile_status;
  std::string vendor;
  std::string generation;
  std::string rank_unit;
  std::string physical_count_unit;
  std::string management_count_unit;
  std::string ranks_per_unit_unit;
  std::string topology_domain;
  std::string topology_digest;
  std::string profile_evidence_level;
  uint64_t physical_count = 0;
  uint64_t management_count = 0;
  uint64_t ranks_per_unit_count = 0;
  int profile_rank_count = 0;
  if (!StringMember(profile, "apiVersion", &api_version) ||
      api_version != "simai.ascend.profile/v1alpha1" ||
      !StringMember(profile, "kind", &kind) ||
      kind != "AscendHardwareProfile" ||
      !StringMember(profile, "schemaSemver", &schema_semver) ||
      schema_semver != "0.1.0" || profile_metadata == nullptr ||
      !StringMember(*profile_metadata, "id", &profile_id) ||
      profile_id.empty() || profile_spec == nullptr ||
      !StringMember(*profile_spec, "status", &profile_status) ||
      profile_status != "READY_FOR_ANALYTICAL" || identity == nullptr ||
      !StringMember(*identity, "vendor", &vendor) ||
      vendor != "HUAWEI_ASCEND" ||
      !StringMember(*identity, "generation", &generation) ||
      (generation != "A2" && generation != "A3" && generation != "A5") ||
      physical_chip_count == nullptr ||
      !PositiveUint64Member(*physical_chip_count, "value", &physical_count) ||
      !StringMember(*physical_chip_count, "unit", &physical_count_unit) ||
      physical_count_unit != "count" || management_device_count == nullptr ||
      !PositiveUint64Member(
          *management_device_count, "value", &management_count) ||
      !StringMember(
          *management_device_count, "unit", &management_count_unit) ||
      management_count_unit != "count" || rank_granularity == nullptr ||
      !StringMember(
          *rank_granularity, "trainingRankUnit", &rank_unit) ||
      (rank_unit != "CHIP" && rank_unit != "MANAGEMENT_DEVICE" &&
       rank_unit != "PACKAGE") || ranks_per_unit == nullptr ||
      !PositiveUint64Member(
          *ranks_per_unit, "value", &ranks_per_unit_count) ||
      !StringMember(*ranks_per_unit, "unit", &ranks_per_unit_unit) ||
      ranks_per_unit_unit != "count" || topology_level == nullptr ||
      !StringMember(*topology_level, "scope", &topology_domain) ||
      !PositiveIntMember(*topology_level, "rankCount", &profile_rank_count) ||
      physical_count != static_cast<uint64_t>(profile_rank_count) ||
      management_count < 1U || ranks_per_unit_count < 1U ||
      !StringMember(*topology_level, "topologyDigest", &topology_digest) ||
      !IsSha256Identifier(topology_digest) || profile_evidence == nullptr ||
      !StringMember(*profile_evidence, "class", &profile_evidence_level) ||
      !EvidenceRecordIsComplete(*profile_evidence) ||
      !UnitsAreCanonical(profile)) {
    Reject(
        contract,
        "DEVICE_PROFILE_SCHEMA_INVALID",
        "The Ascend Profile schema or canonical units are invalid.",
        "Use simai.ascend.profile/v1alpha1 with B, B/s, FLOP/s, ns, or count.");
    return false;
  }
  const std::string profile_field_readiness =
      ContainsString(profile, "FIELD_UNVERIFIED")
          ? "FIELD_UNVERIFIED"
          : (ContainsString(profile, "FIELD_VERIFIED") ? "FIELD_VERIFIED"
                                                        : "UNKNOWN");
  if (profile_field_readiness == "FIELD_UNVERIFIED" &&
      ContainsString(profile, "MEASURED")) {
    Reject(
        contract,
        "EVIDENCE_READINESS_CONFLICT",
        "FIELD_UNVERIFIED Profile values cannot be reported as MEASURED.",
        "Use USER_INPUT, VENDOR_SPEC, DERIVED, or verify the target field.");
    return false;
  }

  const JsonValue* model_reference = Member(root, "collective_cost_model");
  if (model_reference == nullptr) {
    RejectUnsupported(
        contract,
        "HCCL_COST_MODEL_REQUIRED",
        "Ascend Analytical requires an explicit HCCL cost model.",
        "Provide collective_cost_model.path and its SHA-256 digest.");
    return false;
  }
  std::string model_path;
  std::string declared_model_digest;
  if (!ParseArtifactReference(
          *model_reference, &model_path, &declared_model_digest)) {
    Reject(
        contract,
        "HCCL_COST_MODEL_REFERENCE_INVALID",
        "The HCCL cost model reference is invalid.",
        "Provide collective_cost_model.path and its SHA-256 digest.");
    return false;
  }
  std::string model_content;
  if (!ReadFile(model_path, &model_content)) {
    Reject(
        contract,
        "HCCL_COST_MODEL_NOT_FOUND",
        "The HCCL cost model could not be read.",
        "Provide a readable public HCCL cost model artifact.");
    return false;
  }
  contract->cost_model_sha256 = "sha256:" + Sha256Hex(model_content);
  if (contract->cost_model_sha256 != declared_model_digest) {
    Reject(
        contract,
        "HCCL_COST_MODEL_DIGEST_MISMATCH",
        "The HCCL cost model does not match its declared digest.",
        "Use the intended DerivedCostModel or update its digest.");
    return false;
  }
  JsonValue model;
  if (!ParseJsonDocument(model_content, &model)) {
    Reject(
        contract,
        "HCCL_COST_MODEL_INVALID_JSON",
        "The HCCL cost model is not valid JSON.",
        "Correct the cost model JSON and retry.");
    return false;
  }

  const JsonValue* model_metadata = Member(model, "metadata");
  const JsonValue* model_spec = Member(model, "spec");
  const JsonValue* group_domain =
      model_spec == nullptr ? nullptr : Member(*model_spec, "groupDomain");
  const JsonValue* message_domain =
      model_spec == nullptr ? nullptr : Member(*model_spec, "messageDomainBytes");
  const JsonValue* fit =
      model_spec == nullptr ? nullptr : Member(*model_spec, "fit");
  const JsonValue* startup = fit == nullptr ? nullptr : Member(*fit, "startup");
  const JsonValue* bandwidth = fit == nullptr ? nullptr : Member(*fit, "bandwidth");
  const JsonValue* input_sample =
      model_spec == nullptr ? nullptr : FirstArrayObject(*model_spec, "inputSamples");
  const JsonValue* input_samples =
      model_spec == nullptr ? nullptr : Member(*model_spec, "inputSamples");
  const JsonValue* extrapolation =
      model_spec == nullptr ? nullptr : Member(*model_spec, "extrapolation");
  const JsonValue* traffic =
      model_spec == nullptr ? nullptr : Member(*model_spec, "traffic");
  std::string model_id;
  std::string input_sample_id;
  std::string model_profile_digest;
  std::string collective;
  std::string dtype;
  std::string reduction;
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
  std::string model_evidence_level;
  std::string model_field_readiness;
  std::string traffic_algorithm;
  std::string traffic_semantics;
  int model_rank_count = 0;
  uint64_t minimum_message_bytes = 0;
  uint64_t maximum_message_bytes = 0;
  uint64_t startup_ns = 0;
  double bandwidth_Bps = 0.0;
  bool extrapolation_allowed = true;
  if (!StringMember(model, "apiVersion", &api_version) ||
      api_version != "simai.ascend.costmodel/v1alpha1" ||
      !StringMember(model, "kind", &kind) || kind != "HcclCostModel" ||
      !StringMember(model, "schemaSemver", &schema_semver) ||
      schema_semver != "0.1.0" || model_metadata == nullptr ||
      !StringMember(*model_metadata, "id", &model_id) || model_id.empty() ||
      model_spec == nullptr ||
      !StringMember(*model_spec, "profileDigest", &model_profile_digest) ||
      model_profile_digest != contract->device_profile_sha256 ||
      !StringMember(*model_spec, "collective", &collective) ||
      collective != "ALL_REDUCE" || group_domain == nullptr ||
      !StringMember(*model_spec, "dtype", &dtype) || dtype != "BF16" ||
      !StringMember(*model_spec, "reduction", &reduction) ||
      reduction != "SUM" ||
      !StringMember(*model_spec, "timingScope", &timing_scope) ||
      timing_scope != "DEVICE_ONLY" ||
      !FirstArrayPositiveInt(*group_domain, "rankCounts", &model_rank_count) ||
      !FirstArrayString(*group_domain, "groupTypes", &group_type) ||
      !FirstArrayString(*group_domain, "scopes", &model_topology_domain) ||
      !FirstArrayString(
          *group_domain, "topologyDigests", &model_topology_digest) ||
      model_rank_count != profile_rank_count ||
      model_topology_domain != topology_domain ||
      model_topology_digest != topology_digest || message_domain == nullptr ||
      !PositiveUint64Member(
          *message_domain, "min", &minimum_message_bytes) ||
      !PositiveUint64Member(
          *message_domain, "max", &maximum_message_bytes) ||
      minimum_message_bytes > maximum_message_bytes ||
      !StringMember(*message_domain, "unit", &message_unit) ||
      message_unit != "B" || fit == nullptr ||
      !StringMember(*fit, "family", &family) || family != "ALPHA_BETA" ||
      !StringMember(*fit, "formula", &formula) ||
      formula !=
          "round(startup_ns + message_B / bandwidth_Bps * 1000000000)" ||
      !StringMember(*fit, "interpolation", &interpolation) ||
      interpolation != "NONE" ||
      startup == nullptr ||
      !PositiveUint64Member(*startup, "value", &startup_ns) ||
      !StringMember(*startup, "unit", &startup_unit) || startup_unit != "ns" ||
      bandwidth == nullptr ||
      !NumberMember(*bandwidth, "value", &bandwidth_Bps) ||
      bandwidth_Bps <= 0.0 ||
      !StringMember(*bandwidth, "unit", &bandwidth_unit) ||
      bandwidth_unit != "B/s" || input_sample == nullptr ||
      input_samples == nullptr ||
      input_samples->type != JsonValue::Type::Array ||
      input_samples->array.size() != 1U ||
      !StringMember(*input_sample, "id", &input_sample_id) ||
      input_sample_id.empty() || traffic == nullptr ||
      !StringMember(*traffic, "algorithm", &traffic_algorithm) ||
      traffic_algorithm != "RING" ||
      !StringMember(*traffic, "semantics", &traffic_semantics) ||
      traffic_semantics != "ALGORITHM_TOTAL_GROUP_BYTES" ||
      !StringMember(*model_spec, "evidenceClass", &model_evidence_level) ||
      model_evidence_level != "DERIVED" ||
      !StringMember(*model_spec, "readiness", &model_field_readiness) ||
      model_field_readiness != "FIELD_UNVERIFIED" || extrapolation == nullptr ||
      !BooleanMember(
          *extrapolation, "allowed", &extrapolation_allowed) ||
      extrapolation_allowed || !UnitsAreCanonical(model) ||
      ContainsString(model, "MEASURED")) {
    Reject(
        contract,
        "HCCL_COST_MODEL_SCHEMA_INVALID",
        "The HCCL DerivedCostModel or its domain is invalid.",
        "Provide a non-extrapolating AllReduce ALPHA_BETA model matching the Profile.");
    return false;
  }

  std::string raw_path;
  std::string declared_raw_digest;
  if (!ParseArtifactReference(
          *input_sample, &raw_path, &declared_raw_digest)) {
    Reject(
        contract,
        "RAW_OBSERVATION_REFERENCE_INVALID",
        "The DerivedCostModel RawObservation reference is invalid.",
        "Reference one immutable RawObservation by path and SHA-256.");
    return false;
  }
  std::string raw_content;
  if (!ReadFile(raw_path, &raw_content)) {
    Reject(
        contract,
        "RAW_OBSERVATION_NOT_FOUND",
        "The referenced RawObservation could not be read.",
        "Provide the immutable public RawObservation artifact.");
    return false;
  }
  contract->raw_observation_sha256 = "sha256:" + Sha256Hex(raw_content);
  if (contract->raw_observation_sha256 != declared_raw_digest) {
    Reject(
        contract,
        "RAW_OBSERVATION_DIGEST_MISMATCH",
        "The RawObservation does not match its declared digest.",
        "Restore the immutable RawObservation or derive a new model.");
    return false;
  }
  JsonValue raw;
  if (!ParseJsonDocument(raw_content, &raw)) {
    Reject(
        contract,
        "RAW_OBSERVATION_INVALID_JSON",
        "The RawObservation is not valid JSON.",
        "Correct the RawObservation JSON and derive a new model.");
    return false;
  }

  const JsonValue* raw_spec = Member(raw, "spec");
  const JsonValue* raw_metadata = Member(raw, "metadata");
  const JsonValue* raw_group =
      raw_spec == nullptr ? nullptr : Member(*raw_spec, "group");
  const JsonValue* payload =
      raw_spec == nullptr ? nullptr : Member(*raw_spec, "payload");
  const JsonValue* bytes_per_rank =
      payload == nullptr ? nullptr : Member(*payload, "bytesPerRank");
  const JsonValue* normalized =
      raw_spec == nullptr ? nullptr : Member(*raw_spec, "normalized");
  const JsonValue* average_time =
      normalized == nullptr ? nullptr : Member(*normalized, "averageTime");
  const JsonValue* alg_bandwidth =
      normalized == nullptr ? nullptr : Member(*normalized, "algBandwidth");
  const JsonValue* raw_evidence =
      raw_spec == nullptr ? nullptr : FirstArrayObject(*raw_spec, "evidence");
  const JsonValue* correctness =
      raw_spec == nullptr ? nullptr : Member(*raw_spec, "correctness");
  const JsonValue* eligibility =
      raw_spec == nullptr ? nullptr : Member(*raw_spec, "eligibility");
  std::string raw_profile_ref;
  std::string raw_id;
  std::string raw_profile_digest;
  std::string raw_collective;
  std::string raw_scope;
  std::string raw_group_type;
  std::string raw_topology_digest;
  std::string raw_byte_semantics;
  std::string raw_byte_unit;
  std::string raw_time_unit;
  std::string raw_bandwidth_unit;
  std::string raw_evidence_level;
  std::string raw_field_readiness;
  std::string raw_dtype;
  std::string raw_reduction;
  std::string correctness_status;
  int raw_rank_count = 0;
  uint64_t raw_message_bytes = 0;
  uint64_t raw_time_ns = 0;
  double raw_bandwidth_Bps = 0.0;
  bool eligible_for_fit = false;
  if (!StringMember(raw, "apiVersion", &api_version) ||
      api_version != "simai.ascend.observation/v1alpha1" ||
      !StringMember(raw, "kind", &kind) || kind != "HcclRawSample" ||
      !StringMember(raw, "schemaSemver", &schema_semver) ||
      schema_semver != "0.1.0" || raw_metadata == nullptr ||
      !StringMember(*raw_metadata, "id", &raw_id) ||
      raw_id != input_sample_id || raw_spec == nullptr ||
      !StringMember(*raw_spec, "profileRef", &raw_profile_ref) ||
      raw_profile_ref != profile_id ||
      !StringMember(*raw_spec, "profileDigest", &raw_profile_digest) ||
      raw_profile_digest != contract->device_profile_sha256 ||
      !StringMember(*raw_spec, "collective", &raw_collective) ||
      raw_collective != collective || raw_group == nullptr ||
      !PositiveIntMember(*raw_group, "rankCount", &raw_rank_count) ||
      raw_rank_count != model_rank_count ||
      !StringMember(*raw_group, "scope", &raw_scope) ||
      raw_scope != model_topology_domain ||
      !StringMember(*raw_group, "groupType", &raw_group_type) ||
      raw_group_type != group_type ||
      !StringMember(*raw_group, "topologyDigest", &raw_topology_digest) ||
      raw_topology_digest != model_topology_digest || bytes_per_rank == nullptr ||
      !StringMember(
          *bytes_per_rank, "semantics", &raw_byte_semantics) ||
      raw_byte_semantics != "API_INPUT_BYTES" ||
      !PositiveUint64Member(
          *bytes_per_rank, "uniformValue", &raw_message_bytes) ||
      raw_message_bytes < minimum_message_bytes ||
      raw_message_bytes > maximum_message_bytes ||
      !StringMember(*bytes_per_rank, "unit", &raw_byte_unit) ||
      raw_byte_unit != "B" ||
      !StringMember(*payload, "dtype", &raw_dtype) || raw_dtype != dtype ||
      !StringMember(*payload, "reduction", &raw_reduction) ||
      raw_reduction != reduction || average_time == nullptr ||
      !PositiveUint64Member(*average_time, "value", &raw_time_ns) ||
      !StringMember(*average_time, "unit", &raw_time_unit) ||
      raw_time_unit != "ns" || alg_bandwidth == nullptr ||
      !NumberMember(*alg_bandwidth, "value", &raw_bandwidth_Bps) ||
      raw_bandwidth_Bps <= 0.0 ||
      !StringMember(*alg_bandwidth, "unit", &raw_bandwidth_unit) ||
      raw_bandwidth_unit != "B/s" || raw_evidence == nullptr ||
      !StringMember(*raw_evidence, "class", &raw_evidence_level) ||
      !StringMember(
          *raw_evidence, "readiness", &raw_field_readiness) ||
      raw_field_readiness != "FIELD_UNVERIFIED" ||
      !EvidenceRecordIsComplete(*raw_evidence) ||
      correctness == nullptr ||
      !StringMember(*correctness, "status", &correctness_status) ||
      correctness_status != "PASS" || eligibility == nullptr ||
      !BooleanMember(*eligibility, "fit", &eligible_for_fit) ||
      !eligible_for_fit ||
      !UnitsAreCanonical(raw) || ContainsString(raw, "MEASURED")) {
    Reject(
        contract,
        "RAW_OBSERVATION_SCHEMA_INVALID",
        "The immutable HCCL RawObservation is invalid or out of domain.",
        "Use a canonical-unit AllReduce observation matching Profile and model.");
    return false;
  }

  if (group_type == "TP") {
    contract->hccl_cost_model.group_type = AstraSim::CostedGroupType::TP;
  } else if (group_type == "DP") {
    contract->hccl_cost_model.group_type = AstraSim::CostedGroupType::DP;
  } else if (group_type == "EP") {
    contract->hccl_cost_model.group_type = AstraSim::CostedGroupType::EP;
  } else if (group_type == "DP_EP") {
    contract->hccl_cost_model.group_type = AstraSim::CostedGroupType::DP_EP;
  } else {
    Reject(
        contract,
        "HCCL_COST_MODEL_SCHEMA_INVALID",
        "The HCCL group type is unsupported.",
        "Use TP, DP, EP, or DP_EP.");
    return false;
  }
  contract->hccl_cost_model.model_id = model_id;
  contract->hccl_cost_model.collective = AstraSim::CostedCollective::AllReduce;
  contract->hccl_cost_model.rank_count = model_rank_count;
  contract->hccl_cost_model.minimum_message_bytes = minimum_message_bytes;
  contract->hccl_cost_model.maximum_message_bytes = maximum_message_bytes;
  contract->hccl_cost_model.startup_ns = startup_ns;
  contract->hccl_cost_model.bandwidth_Bps = bandwidth_Bps;
  contract->hccl_cost_model.topology_domain = topology_domain;
  contract->hccl_cost_model.topology_digest = topology_digest;
  contract->ascend_rank_count = profile_rank_count;
  contract->topology_domain = topology_domain;
  contract->topology_digest = topology_digest;
  contract->profile_evidence_level = profile_evidence_level;
  contract->profile_field_readiness = profile_field_readiness;
  contract->raw_observation_evidence_level = raw_evidence_level;
  contract->raw_observation_field_readiness = raw_field_readiness;
  contract->cost_model_evidence_level = model_evidence_level;
  contract->cost_model_field_readiness = model_field_readiness;
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
  const std::string status = runtime_domain_miss
      ? "UNSUPPORTED"
      : (valid ? "VALID" : contract.status);
  const std::string reject_code = runtime_domain_miss
      ? "HCCL_MODEL_DOMAIN_MISS"
      : (valid ? "NONE" : contract.reject_code);
  const std::string message = runtime_domain_miss
      ? "The workload collective is outside the HCCL model domain."
      : (valid
             ? (contract.ascend_profiled
                    ? "The Ascend HCCL Analytical run completed."
                    : "The analytical legacy GPU run completed.")
             : contract.message);
  const std::string remediation = runtime_domain_miss
      ? "Use a workload collective covered by the exact model domain."
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
         << "    \"cost_model_sha256\": "
         << Quote(contract.cost_model_sha256) << ",\n"
         << "    \"raw_observation_sha256\": "
         << Quote(contract.raw_observation_sha256) << "\n"
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
