#include <basalt/wire/multiversion/java/NativeParticles.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace basalt::wire::multiversion::java::nativeParticles {
namespace {

using Shape = protocol::play::ParticleOptionShape;

/// The particle types that carry options, by this server's numbering.
constexpr std::array<std::pair<std::int32_t, Shape>, 22> kParticleShapes = {{
    {1, Shape::BlockState},      {2, Shape::BlockState},   {7, Shape::Geyser},  {8, Shape::GeyserBase},
    {9, Shape::GeyserBase},      {10, Shape::Geyser},      {15, Shape::Number}, {21, Shape::Dust},
    {22, Shape::DustTransition}, {23, Shape::Spell},       {28, Shape::Color},  {36, Shape::BlockState},
    {46, Shape::Color},          {48, Shape::Number},      {52, Shape::Color},  {56, Shape::Spell},
    {57, Shape::Item},           {58, Shape::Vibration},   {59, Shape::Trail},  {115, Shape::Delay},
    {121, Shape::BlockState},    {125, Shape::BlockState},
}};

} // namespace

Shape shapeOf(std::int32_t nativeType) {
    const auto found = std::ranges::find(kParticleShapes, nativeType, &std::pair<std::int32_t, Shape>::first);
    return found == kParticleShapes.end() ? Shape::None : found->second;
}

} // namespace basalt::wire::multiversion::java::nativeParticles
