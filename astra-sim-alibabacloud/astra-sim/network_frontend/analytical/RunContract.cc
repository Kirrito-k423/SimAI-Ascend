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
    bool execution_succeeded) {
  if (contract.result_manifest_path.empty()) {
    return false;
  }
  std::ofstream output(
      contract.result_manifest_path.c_str(),
      std::ios::out | std::ios::trunc | std::ios::binary);
  if (!output) {
    return false;
  }

  const bool valid = contract.accepted && execution_succeeded;
  const std::string status = valid ? "VALID" : contract.status;
  const std::string reject_code = valid ? "NONE" : contract.reject_code;
  const std::string message = valid ? "The analytical legacy GPU run completed."
                                    : contract.message;
  const std::string remediation = valid ? "NONE" : contract.remediation;
  const std::string accelerator =
      contract.legacy_gpu.gpu_count > 0 ? "LEGACY_GPU" : "UNKNOWN";
  const std::string cost_model =
      valid && !contract.device_profile_present ? "LEGACY_CALBUSBW" : "UNKNOWN";
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
  if (contract.legacy_gpu.gpu_count > 0) {
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
         << "    \"cost_model\": " << Quote(cost_model) << "\n"
         << "  },\n"
         << "  \"evidence\": {\n"
         << "    \"workload\": {\"level\": "
         << Quote(contract.workload_sha256 == "UNKNOWN" ? "UNKNOWN"
                                                          : "USER_PROVIDED")
         << ", \"digest\": "
         << Quote(contract.workload_sha256) << "},\n"
         << "    \"device_profile\": {\"level\": "
         << Quote(contract.device_profile_present &&
                          contract.device_profile_sha256 != "UNKNOWN"
                      ? "USER_PROVIDED"
                      : "UNKNOWN")
         << ", \"digest\": " << Quote(contract.device_profile_sha256) << "}\n"
         << "  },\n"
         << "  \"readiness\": {\n"
         << "    \"contract\": " << Quote(valid ? "READY" : "BLOCKED") << ",\n"
         << "    \"workload\": " << Quote(workload_readiness) << ",\n"
         << "    \"analytical_backend\": "
         << Quote(valid ? "READY" : "BLOCKED") << ",\n"
         << "    \"ascend_profile\": "
         << Quote(contract.device_profile_present ? "BLOCKED" : "UNKNOWN")
         << ",\n"
         << "    \"hbm\": \"UNKNOWN\",\n"
         << "    \"traffic\": \"UNKNOWN\"\n"
         << "  },\n"
         << "  \"results\": {\n"
         << "    \"validity\": " << Quote(valid ? "VALID" : "UNKNOWN") << ",\n"
         << "    \"timing_ns\": \"UNKNOWN\",\n"
         << "    \"hbm_peak_B\": \"UNKNOWN\",\n"
         << "    \"traffic_B\": \"UNKNOWN\",\n"
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
