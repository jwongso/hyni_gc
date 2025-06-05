#ifndef HYNI_CONFIG_H
#define HYNI_CONFIG_H

#include <string>

namespace hyni
{
const std::string GENERAL_SYSPROMPT =
    "You are a helpful assistant";

const std::string BEHAVIORAL_SYSPROMPT = "";

const std::string SYSTEM_DESIGN_SYSPROMPT = "";

/// This could be for other system configurations (e.g., model type)
constexpr const char GPT_MODEL_TYPE[] = "gpt-4o";
constexpr const char DS_GENERAL_MODEL_TYPE[] = "deepseek-chat";
constexpr const char DS_CODING_MODEL_TYPE[] = "deepseek-coder";
constexpr const char CL_MODEL_TYPE[] = "claude-3-5-sonnet-20240620";
constexpr const char GPT_API_URL[] = "https://api.openai.com/v1/chat/completions";
constexpr const char DS_API_URL[] = "https://api.deepseek.com/v1/chat/completions";
constexpr const char CL_API_URL[] = "https://api.anthropic.com/v1/messages";

const std::string& get_commit_hash();

} // namespace hyni

#endif // HYNI_CONFIG_H
