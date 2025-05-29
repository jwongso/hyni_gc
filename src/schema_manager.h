#ifndef SCHEMA_MANAGER_H
#define SCHEMA_MANAGER_H

#include "general_context.h"
#include <unordered_map>
#include <memory>

namespace hyni {

// Schema Manager for provider management
class schema_manager {
public:
    static schema_manager& get_instance();

    void register_schema_path(const std::string& provider_name, const std::string& schema_path);
    void set_schema_directory(const std::string& directory);

    std::unique_ptr<general_context> create_context(const std::string& provider_name);
    std::unique_ptr<general_context> create_context(const std::string& provider_name, const context_config& config);

    std::vector<std::string> get_available_providers() const;
    bool is_provider_available(const std::string& provider_name) const;

private:
    schema_manager() = default;
    ~schema_manager() = default;
    schema_manager(const schema_manager&) = delete;
    schema_manager& operator=(const schema_manager&) = delete;

    std::unordered_map<std::string, std::string> m_provider_paths;
    std::string m_schema_directory = "./schemas/";

    std::string resolve_schema_path(const std::string& provider_name) const;
};

} // hyni

#endif // SCHEMA_MANAGER_H
