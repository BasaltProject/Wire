#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/multiversion/SubstitutionLabels.hpp>

#include <cstdint>
#include <memory>
#include <set>

namespace basalt::wire::multiversion::java {

/// One connection's chain: the links, the tables they share and the rewriter, all owned together.
class JavaChain {
public:
    JavaChain(const JavaChain&) = delete;
    JavaChain& operator=(const JavaChain&) = delete;
    JavaChain(JavaChain&&) = delete;
    JavaChain& operator=(JavaChain&&) = delete;
    virtual ~JavaChain() = default;

    [[nodiscard]] virtual const ProtocolChain& chain() const = 0;

protected:
    JavaChain() = default;
};

/// The links this build has, and the chain one client needs.
namespace javaTranslators {

/// Every protocol a client may connect with, the native one included.
[[nodiscard]] const std::set<std::int32_t>& supported();

[[nodiscard]] std::unique_ptr<JavaChain> chainFor(std::int32_t protocol, const SubstitutionLabels* labels = nullptr);

/// How `protocol` numbers a block state this server calls `nativeState`.
[[nodiscard]] std::int32_t blockStateAs(std::int32_t protocol, std::int32_t nativeState);

} // namespace javaTranslators

} // namespace basalt::wire::multiversion::java
