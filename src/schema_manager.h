#pragma once

#include "general_context.h"
#include <unordered_map>
#include <memory>

namespace hyni {

/**
 * @class schema_manager
 * @brief Singleton class for managing schema providers and their associated paths.
 * @warning Not thread-safe. Concurrent calls to register_schema_path(),
 *          set_schema_directory(), or create_context() require external synchronization.
 */
class schema_manager {
public:
    /**
     * @brief Get the singleton instance of the schema manager
     * @return Reference to the schema manager instance
     */
    static schema_manager& get_instance();

    /**
    * @brief Registers a custom schema path for a provider.
    * @param provider_name The name of the provider (case-sensitive).
    * @param schema_path Absolute or relative path to the schema file.
    * @note Registered paths override any matching provider name in the default directory.
    * @throw std::invalid_argument If provider_name is empty or schema_path is malformed.
    */
    void register_schema_path(const std::string& provider_name, const std::string& schema_path);

    /**
     * @brief Set the directory where schema files are located
     * @param directory The directory path where schema files are stored
     * @note If the directory path doesn't end with a slash, one will be added
     */
    void set_schema_directory(const std::string& directory);

    /**
     * @brief Create a context for a provider using default configuration
     * @param provider_name The name of the provider
     * @return A unique pointer to the created general_context
     * @throws schema_exception if the schema file for the provider is not found
     */
    std::unique_ptr<general_context> create_context(const std::string& provider_name);

    /**
    * @brief Creates a context for a provider using default configuration.
    * @param provider_name The name of the provider.
    * @return A unique_ptr to the created general_context.
    * @throw schema_exception If:
    *  - The provider's schema file is missing.
    *  - The schema file is malformed JSON.
    *  - provider_name is empty.
    */
    std::unique_ptr<general_context> create_context(const std::string& provider_name, const context_config& config);

    /**
    * @brief Retrieves all available providers.
    * @return Vector of unique provider names.
    * @note Example:
    * @code
    * auto providers = manager.get_available_providers();
    * for (const auto& name : providers) { ... }
    * @endcode
    */
    std::vector<std::string> get_available_providers() const;

    /**
     * @brief Check if a specific provider is available
     * @param provider_name The name of the provider to check
     * @return true if the provider's schema file exists, false otherwise
     */
    bool is_provider_available(const std::string& provider_name) const;

private:
    /**
     * @brief Default constructor (private for singleton pattern)
     */
    schema_manager() = default;

    /**
     * @brief Destructor (private for singleton pattern)
     */
    ~schema_manager() = default;

    /**
     * @brief Copy constructor (deleted for singleton pattern)
     */
    schema_manager(const schema_manager&) = delete;

    /**
     * @brief Assignment operator (deleted for singleton pattern)
     */
    schema_manager& operator=(const schema_manager&) = delete;

    /**
     * @brief Map of provider names to their custom schema paths
     */
    std::unordered_map<std::string, std::string> m_provider_paths;

    /**
     * @brief Directory where schema files are stored
     * @note Default is "./schemas/" with a trailing slash
     */
    std::string m_schema_directory = "./schemas/";

    /**
    * @brief Resolves the schema path for a provider, checking registered paths first, then the default directory.
    * @param provider_name The name of the provider.
    * @return The resolved absolute path to the schema file.
    * @note Relative paths (e.g., "../schemas/foo.json") are converted to absolute paths.
    *       Registered paths take precedence over the default directory.
    * @warning Does not verify file existence; use is_provider_available() for validation.
    */
    std::string resolve_schema_path(const std::string& provider_name) const;
};

} // hyni
