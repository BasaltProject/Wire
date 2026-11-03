#pragma once

#include <basalt/wire/io/DnsResolver.hpp>
#include <basalt/wire/io/TcpSocket.hpp>

namespace basalt::wire::io::detail {

[[nodiscard]] std::expected<std::vector<TcpAddress>, DnsError>
resolveAndroidDns(std::string_view host, std::uint16_t port, const DnsOptions& options);

} // namespace basalt::wire::io::detail
