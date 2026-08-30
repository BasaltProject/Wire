#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/multiversion/SubstitutionLabels.hpp>

#include <cstdint>
#include <memory>
#include <set>

namespace basalt::wire::multiversion::java {

/// One connection's chain: the links, the tables they share and the rewriter, all owned together.
///
/// They have to be owned as one because the chain holds references into the other two, and a link
/// halfway down remembers what it has seen on this connection (the codec, the last rotation, a
/// chunk waiting for its light), which is why a chain is built per player rather than shared.
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
///
/// A version is only reachable when every link between it and the native one exists: the chain is
/// built by walking down from 26.2 and stopping at the first gap, so half a road is no road and the
/// gate refuses it rather than letting somebody in to a broken game.
namespace javaTranslators {

/// Every protocol a client may connect with, the native one included.
[[nodiscard]] const std::set<std::int32_t>& supported();

/// A chain of fresh links for one connection, or null when nothing has to be translated.
///
/// `labels` may be null, which leaves substituted content unnamed.
[[nodiscard]] std::unique_ptr<JavaChain> chainFor(std::int32_t protocol, const SubstitutionLabels* labels = nullptr);

/// How `protocol` numbers a block state this server calls `nativeState`.
///
/// The tables are otherwise private to the links. This is here because "what does the other side
/// call this" is a question worth answering from outside one: a test checks the wire against it, and
/// a proxy that rewrites a chunk of its own needs the same answer.
[[nodiscard]] std::int32_t blockStateAs(std::int32_t protocol, std::int32_t nativeState);

} // namespace javaTranslators

} // namespace basalt::wire::multiversion::java
