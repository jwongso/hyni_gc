#include "schema_manager.h"
#include <filesystem>

namespace hyni {

schema_manager& schema_manager::get_instance() {
    static schema_manager instance;
    return instance;
}

void schema_manager::register_schema_path(const std::string& provider_name, const std::string& schema_path) {
    m_provider_paths[provider_name] = schema_path;
}

void schema_manager::set_schema_directory(const std::string& directory) {
    m_schema_directory = directory;
    if (!m_schema_directory.empty() && m_schema_directory.back() != '/') {
        m_schema_directory += '/';
    }
}

std::unique_ptr<general_context> schema_manager::create_context(const std::string& provider_name) {
    context_config default_config;
    return create_context(provider_name, default_config);
}

std::unique_ptr<general_context> schema_manager::create_context(const std::string& provider_name,
                                                               const context_config& config) {
    std::string schema_path = resolve_schema_path(provider_name);

    if (!std::filesystem::exists(schema_path)) {
        throw schema_exception("Schema file not found for provider: " + provider_name + " at " + schema_path);
    }

    return std::make_unique<general_context>(schema_path, config);
}

std::vector<std::string> schema_manager::get_available_providers() const {
    std::vector<std::string> providers;

    // From registered paths
    for (const auto& [name, path] : m_provider_paths) {
        if (std::filesystem::exists(path)) {
            providers.push_back(name);
        }
    }

    // From schema directory
    if (std::filesystem::exists(m_schema_directory)) {
        for (const auto& entry : std::filesystem::directory_iterator(m_schema_directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string provider_name = entry.path().stem().string();
                if (m_provider_paths.find(provider_name) == m_provider_paths.end() &&
                    std::find(providers.begin(), providers.end(), provider_name) == providers.end()) {
                    providers.push_back(provider_name);
                }
            }
        }
    }

    return providers;
}

bool schema_manager::is_provider_available(const std::string& provider_name) const {
    std::string schema_path = resolve_schema_path(provider_name);
    return std::filesystem::exists(schema_path);
}

std::string schema_manager::resolve_schema_path(const std::string& provider_name) const {
    auto it = m_provider_paths.find(provider_name);
    if (it != m_provider_paths.end()) {
        return it->second;
    }
    return m_schema_directory + provider_name + ".json";
}

} // hyni
