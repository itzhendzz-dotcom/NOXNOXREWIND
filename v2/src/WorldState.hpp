#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <math.h>
#include <float.h>
#include <aml-psdk/gta_base/Vector.h>

class CPhysical;
class CPlayerPed;
class CPed;
class CVehicle;

namespace noxxa {

constexpr std::size_t kHardMaxWorldEntities = 64;

enum class EntityKind : uint8_t {
    Ped = 1,
    Vehicle = 2,
};

struct TransformState {
    bool hasMatrix{false};
    CVector right{};
    CVector forward{};
    CVector up{};
    CVector position{};
    float heading{0.0f};
};

struct PhysicalState {
    TransformState transform{};
    CVector moveSpeed{};
    CVector turnSpeed{};
};

struct EntitySnapshot {
    EntityKind kind{EntityKind::Ped};
    int32_t ref{-1};
    PhysicalState physical{};
    float health{0.0f};
    float armour{0.0f};
    float currentRotation{0.0f};
    float aimingRotation{0.0f};
};

struct WorldFrame {
    uint64_t sequence{0};
    uint32_t gameTimeMs{0};
    uint16_t count{0};
    std::array<EntitySnapshot, kHardMaxWorldEntities> entities{};
};

struct PlayerAnchor {
    bool valid{false};
    int32_t pedRef{-1};
    PhysicalState physical{};
    float health{0.0f};
    float armour{0.0f};
    float currentRotation{0.0f};
    float aimingRotation{0.0f};
    bool collisionEnabled{true};
};

struct WorldCaptureSettings {
    float radius{45.0f};
    std::size_t maxEntities{48};
    bool restoreHealth{false};
};

class WorldStateAdapter {
public:
    void Configure(const WorldCaptureSettings& settings);

    bool CaptureWorld(WorldFrame& out, uint64_t sequence) const;
    bool CaptureAnchor(PlayerAnchor& out) const;
    bool ApplyAnchor(const PlayerAnchor& anchor) const;

    void BeginAnchorPose(const char* anim, const char* ifp) const;
    void EndAnchorPose() const;

    void QuiesceWorld(const WorldFrame* frame) const;
    void QuiescePlayer() const;

    bool ApplyWorldInterpolated(const WorldFrame& newer, const WorldFrame& older, float alpha) const;

    bool PlayerCanAnchor() const;
    bool IsPlayerInVehicle() const;

    static CVector GetPhysicalPosition(const CPhysical* physical);

private:
    static void CapturePhysical(CPhysical* physical, PhysicalState& out);
    static void ApplyPhysical(CPhysical* physical, const PhysicalState& state);
    static PhysicalState LerpPhysical(const PhysicalState& a, const PhysicalState& b, float t);
    static CVector LerpVector(const CVector& a, const CVector& b, float t);
    static float LerpAngle(float a, float b, float t);
    static const EntitySnapshot* FindSnapshot(const WorldFrame& frame, EntityKind kind, int32_t ref);

    bool ApplyEntity(const EntitySnapshot& target, const EntitySnapshot* from, float alpha) const;

    WorldCaptureSettings m_settings{};
};

} // namespace noxxa
