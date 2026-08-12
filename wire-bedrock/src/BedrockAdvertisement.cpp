#include <basalt/wire/bedrock/BedrockAdvertisement.hpp>
#include <basalt/wire/bedrock/BedrockPacketIds.hpp>

namespace basalt::wire::bedrock {
namespace {

std::string oneLine(const std::string_view text) {
    std::string clean;
    clean.reserve(text.size());
    for (const char character : text) {
        clean += (character == ';' || character == '\n' || character == '\r') ? ' ' : character;
    }
    while (!clean.empty() && clean.front() == ' ') {
        clean.erase(clean.begin());
    }
    while (!clean.empty() && clean.back() == ' ') {
        clean.pop_back();
    }
    return clean;
}

} // namespace

std::pair<std::string, std::string> splitMotd(const std::string_view text) {
    const std::size_t newline = text.find('\n');
    if (newline == std::string_view::npos) {
        return {oneLine(text), std::string{}};
    }
    return {oneLine(text.substr(0, newline)), oneLine(text.substr(newline + 1))};
}

std::string advertisementLine(const Advertisement& status) {
    std::string line = "MCPE;";
    line += oneLine(status.motd) + ";";
    line += std::to_string(kNativeBedrockProtocol) + ";";
    line += status.gameVersion + ";";
    line += std::to_string(status.online) + ";";
    line += std::to_string(status.max) + ";";
    line += std::to_string(status.guid) + ";";
    line += oneLine(status.subMotd) + ";";
    line += "Survival;1;";
    line += std::to_string(status.port) + ";";
    line += std::to_string(status.port) + ";";
    return line;
}

} // namespace basalt::wire::bedrock
