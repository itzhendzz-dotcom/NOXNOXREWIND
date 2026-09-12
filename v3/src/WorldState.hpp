#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <math.h>
#include <float.h>

#include <aml-psdk/game_sa/plugin.h>
#include <aml-psdk/gta_base/Vector.h>

class CPhysical;
class CPlayerPed;
class CPed;
class CVehicle;

namespace noxxa {

constexpr std::size_t kHardMaxWorldEntities = 64;

enum class EntityKind : uint8_t { Ped = 1, Vehicle = 2 };

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

struct PedBehaviorState {
    uint32_t pedState{0};
    int32_t moveState{0};
    uint32_t moveStateAnim{0};
    uint32_t storedMoveState{0};
    uint32_t animGroup{0};
    uint8_t createdBy{0};
    uint8_t selectedWeaponSlot{0};
    uint16_t flags{0};

    int32_t activeTaskType{-1};
    int32_t tempEventTaskType{-1};
    int32_t nonTempEventTaskType{-1};

    int32_t weaponType{-1};
    int32_t weaponState{-1};
    uint32_t ammoInClip{0};
    uint32_t ammoTotal{0};

    // Stored relative to the historical frame time, never as raw old absolute
    // CTimer values. These are rebased when the past becomes the new present.
    uint32_t attackDelayMs{0};
    uint32_t weaponNextShotDelayMs{0};
    uint32_t lastDamageAgeMs{0};
};

struct VehicleBehaviorState {
    float steerAngle{0.0f};
    float secondSteerAngle{0.0f};
    float gasPedal{0.0f};
    float brakePedal{0.0f};
    uint8_t currentGear{0};
    bool engineOn{false};
    bool handbrakeOn{false};
    bool lightsOn{false};
};

struct EntitySnapshot {
    EntityKind kind{EntityKind::Ped};
    int32_t ref{-1};
    uint16_t modelIndex{0xFFFF};
    PhysicalState physical{};
    float health{0.0f};
    float armour{0.0f};
    float currentRotation{0.0f};
    float aimingRotation{0.0f};
    PedBehaviorState pedBehavior{};
    VehicleBehaviorState vehicleBehavior{};
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
};

struct WorldCaptureSettings {
    float radius{35.0f};
    std::size_t maxEntities{28};
    bool restoreHealth{true};
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

    // Called only after rewind release/commit. This does not deserialize task
    // pointers. It removes event-response tasks that provably belong to the
    // discarded future, restores historical scalar behaviour, and rebases
    // timers onto the new present.
    int CommitCausalState(const WorldFrame& selectedPast) const;

    bool PlayerCanAnchor() const;
    bool IsPlayerInVehicle() const;
    static CVector GetPhysicalPosition(const CPhysical* physical);

private:
    static bool CapturePhysical(CPhysical* physical, PhysicalState& out);
    static bool ApplyPhysical(CPhysical* physical, const PhysicalState& state);
    static bool IsFiniteVector(const CVector& v);
    static bool IsSanePhysical(const PhysicalState& state);
    static PhysicalState LerpPhysical(const PhysicalState& a, const PhysicalState& b, float t);
    static CVector LerpVector(const CVector& a, const CVector& b, float t);
    static float LerpAngle(float a, float b, float t);
    static const EntitySnapshot* FindSnapshot(const WorldFrame& frame, EntityKind kind, int32_t ref);

    static void CapturePedBehavior(CPed* ped, uint32_t frameTimeMs, PedBehaviorState& out);
    static void ApplyPedBehaviorPlayback(CPed* ped, const PedBehaviorState& state, uint32_t nowMs);
    static void CaptureVehicleBehavior(CVehicle* vehicle, VehicleBehaviorState& out);
    static void ApplyVehicleBehavior(CVehicle* vehicle, const VehicleBehaviorState& state);

    bool ApplyEntity(const EntitySnapshot& target, const EntitySnapshot* from, float alpha) const;
    WorldCaptureSettings m_settings{};
};

} // namespace noxxa
