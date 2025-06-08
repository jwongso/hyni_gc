#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

namespace hyni {
// Custom exceptions for better error handling
class schema_exception : public std::runtime_error {
public:
    explicit schema_exception(const std::string& msg) : std::runtime_error(msg) {}
};

class validation_exception : public std::runtime_error {
public:
    explicit validation_exception(const std::string& msg) : std::runtime_error(msg) {}
};

// Configuration structure for additional options
struct context_config {
    bool enable_streaming_support = false;
    bool enable_validation = true;
    bool enable_caching = true;
    std::optional<int> default_max_tokens;
    std::optional<double> default_temperature;
    std::unordered_map<std::string, nlohmann::json> custom_parameters;
};

// Forward declaration
class schema_manager;

// Main general_context class
class general_context {
public:
    explicit general_context(const std::string& schema_path, const context_config& config = {});

    // Model and system configuration
    general_context& set_model(const std::string& model);
    general_context& set_system_message(const std::string& system_text);
    general_context& set_parameter(const std::string& key, const nlohmann::json& value);
    general_context& set_parameters(const std::unordered_map<std::string, nlohmann::json>& params);
    general_context& set_api_key(const std::string& api_key);

    // Message management
    general_context& add_user_message(const std::string& content, const std::optional<std::string>& media_type = {},
                         const std::optional<std::string>& media_data = {});
    general_context& add_assistant_message(const std::string& content);
    general_context& add_message(const std::string& role, const std::string& content,
                    const std::optional<std::string>& media_type = {},
                    const std::optional<std::string>& media_data = {});

    // Request building and response handling
    nlohmann::json build_request(bool streaming = false);
    std::string extract_text_response(const nlohmann::json& response);
    nlohmann::json extract_full_response(const nlohmann::json& response);
    std::string extract_error(const nlohmann::json& response);

    // Utility methods
    void reset();
    void clear_messages();
    void clear_parameters();
    bool has_api_key() const { return !m_api_key.empty(); }

    // Getters for introspection
    const nlohmann::json& get_schema() const { return m_schema; }
    const std::string& get_provider_name() const { return m_provider_name; }
    const std::string& get_endpoint() const { return m_endpoint; }
    const std::unordered_map<std::string, std::string>& get_headers() const { return m_headers; }

    std::vector<std::string> get_supported_models() const;
    bool supports_multimodal() const;
    bool supports_streaming() const;
    bool supports_system_messages() const;

    // Validation
    bool is_valid_request() const;
    std::vector<std::string> get_validation_errors() const;

    // Parameters
    const std::unordered_map<std::string, nlohmann::json>& get_parameters() const {
        return m_parameters;
    }
    nlohmann::json get_parameter(const std::string& key) const;
    template<typename T>
    T get_parameter_as(const std::string& key) const {
        auto param = get_parameter(key);
        try {
            return param.get<T>();
        } catch (const nlohmann::json::exception& e) {
            throw validation_exception("Parameter '" + key +
                                       "' cannot be converted to requested type: " + e.what());
        }
    }
    template<typename T>
    T get_parameter_as(const std::string& key, const T& default_value) const {
        if (!has_parameter(key)) {
            return default_value;
        }
        return get_parameter_as<T>(key);
    }
    bool has_parameter(const std::string& key) const;

    const std::vector<nlohmann::json>& get_messages() const { return m_messages; }

private:
    // Core data members
    nlohmann::json m_schema;
    nlohmann::json m_request_template;
    context_config m_config;

    std::string m_provider_name;
    std::string m_endpoint;
    std::unordered_map<std::string, std::string> m_headers;
    std::string m_model_name;
    std::optional<std::string> m_system_message;
    std::vector<nlohmann::json> m_messages;
    std::unordered_map<std::string, nlohmann::json> m_parameters;
    std::string m_api_key;
    std::unordered_set<std::string> m_valid_roles;

    // Cached paths and formats
    std::vector<std::string> m_text_path;
    std::vector<std::string> m_error_path;
    nlohmann::json m_message_structure;
    nlohmann::json m_text_content_format;
    nlohmann::json m_image_content_format;

    // Internal methods
    void load_schema(const std::string& schema_path);
    void validate_schema();
    void apply_defaults();
    void cache_schema_elements();
    void build_headers();

    nlohmann::json create_message(const std::string& role, const std::string& content,
                                 const std::optional<std::string>& media_type = {},
                                 const std::optional<std::string>& media_data = {});
    nlohmann::json create_text_content(const std::string& text);
    nlohmann::json create_image_content(const std::string& media_type, const std::string& data);

    nlohmann::json resolve_path(const nlohmann::json& json, const std::vector<std::string>& path) const;
    std::vector<std::string> parse_json_path(const nlohmann::json& path_array) const;

    void validate_message(const nlohmann::json& message) const;
    void validate_parameter(const std::string& key, const nlohmann::json& value) const;

    std::string encode_image_to_base64(const std::string& image_path) const;
    bool is_base64_encoded(const std::string& data) const;
};

} // hyni
