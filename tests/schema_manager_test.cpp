#include "../src/schema_manager.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace hyni {
namespace testing {

class SchemaManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directory structure
        std::filesystem::create_directory("test_schemas");

        // Create dummy schema files
        createDummySchemaFile("test_schemas/provider1.json");
        createDummySchemaFile("test_schemas/provider2.json");

        // Create a custom schema directory
        std::filesystem::create_directory("custom_schemas");
        createDummySchemaFile("custom_schemas/provider3.json");

        // Reset schema manager to default state
        auto& manager = schema_manager::get_instance();
        manager.set_schema_directory("./schemas/"); // Reset to default
    }

    void TearDown() override {
        // Clean up test directories and files
        std::filesystem::remove_all("test_schemas");
        std::filesystem::remove_all("custom_schemas");
    }

    void createDummySchemaFile(const std::string& path) {
        std::ofstream file(path);
        file << "{ \"provider\": { \"name\": \"test\", \"display_name\": \"Test AI\" } }";
        file.close();
    }
};

// Test singleton behavior
TEST_F(SchemaManagerTest, SingletonInstance) {
    auto& instance1 = schema_manager::get_instance();
    auto& instance2 = schema_manager::get_instance();

    EXPECT_EQ(&instance1, &instance2) << "schema_manager should return the same instance";
}

// Test setting schema directory
TEST_F(SchemaManagerTest, SetSchemaDirectory) {
    auto& manager = schema_manager::get_instance();

    // Test with trailing slash
    manager.set_schema_directory("test_schemas/");
    EXPECT_TRUE(manager.is_provider_available("provider1"));
    EXPECT_TRUE(manager.is_provider_available("provider2"));

    // Test without trailing slash
    manager.set_schema_directory("test_schemas");
    EXPECT_TRUE(manager.is_provider_available("provider1"));
    EXPECT_TRUE(manager.is_provider_available("provider2"));
}

// Test registering custom schema paths
TEST_F(SchemaManagerTest, RegisterSchemaPath) {
    auto& manager = schema_manager::get_instance();
    manager.set_schema_directory("test_schemas/");

    // Register a custom path
    manager.register_schema_path("custom_provider", "custom_schemas/provider3.json");

    EXPECT_TRUE(manager.is_provider_available("provider1")); // From directory
    EXPECT_TRUE(manager.is_provider_available("provider2")); // From directory
    EXPECT_TRUE(manager.is_provider_available("custom_provider")); // From custom path
}

// Test getting available providers
TEST_F(SchemaManagerTest, GetAvailableProviders) {
    auto& manager = schema_manager::get_instance();
    manager.set_schema_directory("test_schemas/");
    manager.register_schema_path("custom_provider", "custom_schemas/provider3.json");

    auto providers = manager.get_available_providers();

    EXPECT_EQ(3, providers.size());
    EXPECT_TRUE(std::find(providers.begin(), providers.end(), "provider1") != providers.end());
    EXPECT_TRUE(std::find(providers.begin(), providers.end(), "provider2") != providers.end());
    EXPECT_TRUE(std::find(providers.begin(), providers.end(), "custom_provider") != providers.end());
}

// Test provider availability check
TEST_F(SchemaManagerTest, IsProviderAvailable) {
    auto& manager = schema_manager::get_instance();
    manager.set_schema_directory("test_schemas/");

    EXPECT_TRUE(manager.is_provider_available("provider1"));
    EXPECT_TRUE(manager.is_provider_available("provider2"));
    EXPECT_FALSE(manager.is_provider_available("nonexistent_provider"));
}

// Test context creation
TEST_F(SchemaManagerTest, CreateContext) {
    auto& manager = schema_manager::get_instance();
    manager.set_schema_directory("test_schemas/");

    // This test requires a mock or real implementation of general_context
    // For now, we'll just test that it throws when the provider doesn't exist
    EXPECT_THROW(manager.create_context("nonexistent_provider"), schema_exception);

    // For existing providers, we would need to mock the general_context constructor
    // or use a real implementation that can handle our test schema files
}

// Test context creation with custom config
TEST_F(SchemaManagerTest, CreateContextWithConfig) {
    auto& manager = schema_manager::get_instance();
    manager.set_schema_directory("test_schemas/");

    context_config config;
    // Set config properties as needed

    // Similar to the previous test, we need proper mocking or implementation
    EXPECT_THROW(manager.create_context("nonexistent_provider", config), schema_exception);
}

// Test behavior with empty or invalid schema directory
TEST_F(SchemaManagerTest, EmptySchemaDirectory) {
    auto& manager = schema_manager::get_instance();
    manager.set_schema_directory("nonexistent_directory/");

    auto providers = manager.get_available_providers();
    EXPECT_FALSE(providers.empty());

    EXPECT_FALSE(manager.is_provider_available("any_provider"));
}

// Test behavior with invalid custom schema path
TEST_F(SchemaManagerTest, InvalidCustomSchemaPath) {
    auto& manager = schema_manager::get_instance();
    manager.register_schema_path("invalid_provider", "nonexistent_path.json");

    EXPECT_FALSE(manager.is_provider_available("invalid_provider"));
    EXPECT_THROW(manager.create_context("invalid_provider"), schema_exception);
}

} // namespace testing
} // namespace hyni
