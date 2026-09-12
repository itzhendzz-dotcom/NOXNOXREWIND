#include "WorldState.hpp"

#include <aml-psdk/game_sa/base/Matrix.h>
#include <aml-psdk/game_sa/base/Timer.h>
#include <aml-psdk/game_sa/entity/Physical.h>
#include <aml-psdk/game_sa/entity/PlayerPed.h>
#include <aml-psdk/game_sa/entity/Vehicle.h>
#include <aml-psdk/game_sa/other/Pools.h>
#include <aml-psdk/game_sa/utils/OpcodeCaller.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace noxxa {
namespace {

struct Candidate {
    EntityKind kind;
    int32_t ref;
    float dist2;
};

float DistanceSq(const CVector& a, const CVector& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

float Clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

} // namespace

void WorldStateAdapter::Configure(const WorldCaptureSettings& settings) {
    m_settings = settings;
    m_settings.radius = std::max(10.0f, std::min(90.0f, m_settings.radius));
    m_settings.maxEntities = std::max<std::size_t>(4, std::min<std::size_t>(kHardMaxWorldEntities, m_settings.maxEntities));
}

CVector WorldStateAdapter::GetPhysicalPosition(const CPhysical* physical) {
    if (!physical) return CVector{};
    auto* mutablePhysical = const_cast<CPhysical*>(physical);
    if (CMatrix* matrix = mutablePhysical->GetMatrix()) return matrix->pos;
    return mutablePhysical->m_placement.m_vPosn;
}

void WorldStateAdapter::CapturePhysical(CPhysical* physical, PhysicalState& out) {
    if (!physical) return;

    if (CMatrix* matrix = physical->GetMatrix()) {
        out.transform.hasMatrix = true;
        out.transform.right = matrix->right;
        out.transform.forward = matrix->up;
        out.transform.up = matrix->at;
        out.transform.position = matrix->pos;
    } else {
        out.transform.hasMatrix = false;
        out.transform.position = physical->m_placement.m_vPosn;
        out.transform.heading = physical->m_placement.m_fHeading;
    }

    out.moveSpeed = physical->m_vecMoveSpeed;
    out.turnSpeed = physical->m_vecTurnSpeed;
}

void WorldStateAdapter::ApplyPhysical(CPhysical* physical, const PhysicalState& state) {
    if (!physical) return;

    if (state.transform.hasMatrix) {
        if (!physical->GetMatrix()) physical->AllocateMatrix();
        if (CMatrix* matrix = physical->GetMatrix()) {
            matrix->right = state.transform.right;
            matrix->up = state.transform.forward;
            matrix->at = state.transform.up;
            matrix->pos = state.transform.position;
            matrix->UpdateRW();
        }
    } else {
        physical->m_placement.m_vPosn = state.transform.position;
        physical->m_placement.m_fHeading = state.transform.heading;
        if (CMatrix* matrix = physical->GetMatrix()) {
            matrix->SetRotateZ(state.transform.heading);
            matrix->SetTranslateOnly(state.transform.position.x,
                                     state.transform.position.y,
                                     state.transform.position.z);
            matrix->UpdateRW();
        }
    }

    physical->m_vecMoveSpeed = state.moveSpeed;
    physical->m_vecTurnSpeed = state.turnSpeed;
    physical->UpdateRwFrame();
}

CVector WorldStateAdapter::LerpVector(const CVector& a, const CVector& b, float t) {
    t = Clamp01(t);
    return CVector(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    );
}

float WorldStateAdapter::LerpAngle(float a, float b, float t) {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = 2.0f * kPi;
    float d = std::fmod(b - a, kTwoPi);
    if (d > kPi) d -= kTwoPi;
    if (d < -kPi) d += kTwoPi;
    return a + d * Clamp01(t);
}

PhysicalState WorldStateAdapter::LerpPhysical(const PhysicalState& a, const PhysicalState& b, float t) {
    t = Clamp01(t);
    PhysicalState out{};
    out.transform.hasMatrix = b.transform.hasMatrix;
    if (a.transform.hasMatrix && b.transform.hasMatrix) {
        out.transform.right = LerpVector(a.transform.right, b.transform.right, t);
        out.transform.forward = LerpVector(a.transform.forward, b.transform.forward, t);
        out.transform.up = LerpVector(a.transform.up, b.transform.up, t);
        out.transform.right.Normalise();
        out.transform.forward.Normalise();
        out.transform.up.Normalise();
        out.transform.position = LerpVector(a.transform.position, b.transform.position, t);
    } else if (!a.transform.hasMatrix && !b.transform.hasMatrix) {
        out.transform.position = LerpVector(a.transform.position, b.transform.position, t);
        out.transform.heading = LerpAngle(a.transform.heading, b.transform.heading, t);
    } else {
        out.transform = (t < 0.5f) ? a.transform : b.transform;
    }

    out.moveSpeed = LerpVector(a.moveSpeed, b.moveSpeed, t);
    out.turnSpeed = LerpVector(a.turnSpeed, b.turnSpeed, t);
    return out;
}

bool WorldStateAdapter::PlayerCanAnchor() const {
    CPlayerPed* player = FindPlayerPed(-1);
    return player && player->IsAlive() && !player->bInVehicle;
}

bool WorldStateAdapter::IsPlayerInVehicle() const {
    CPlayerPed* player = FindPlayerPed(-1);
    return player && player->bInVehicle;
}

bool WorldStateAdapter::CaptureAnchor(PlayerAnchor& out) const {
    CPlayerPed* player = FindPlayerPed(-1);
    if (!player || player->bInVehicle) return false;

    out = {};
    out.valid = true;
    out.pedRef = CPools::GetPedRef(player);
    CapturePhysical(player, out.physical);
    out.health = player->m_fHealth;
    out.armour = player->m_fArmour;
    out.currentRotation = player->m_fCurrentRotation;
    out.aimingRotation = player->m_fAimingRotation;
    out.collisionEnabled = player->bUsesCollision;
    return true;
}

bool WorldStateAdapter::ApplyAnchor(const PlayerAnchor& anchor) const {
    if (!anchor.valid) return false;
    CPed* ped = CPools::GetPed(anchor.pedRef);
    if (!ped || !ped->IsPlayer()) return false;

    auto* player = static_cast<CPlayerPed*>(ped);
    if (player->bInVehicle) return false;

    ApplyPhysical(player, anchor.physical);
    player->m_fHealth = anchor.health;
    player->m_fArmour = anchor.armour;
    player->m_fCurrentRotation = anchor.currentRotation;
    player->m_fAimingRotation = anchor.aimingRotation;
    player->bUsesCollision = false;
    return true;
}

void WorldStateAdapter::BeginAnchorPose(const char* anim, const char* ifp) const {
    CPlayerPed* player = FindPlayerPed(-1);
    if (!player || player->bInVehicle) return;
    const int pedRef = CPools::GetPedRef(player);
    if (pedRef < 0) return;

    Command<Commands::TASK_PLAY_ANIM>(
        pedRef,
        (anim && *anim) ? anim : "IDLE_TAXI",
        (ifp && *ifp) ? ifp : "PED",
        4.0f,
        0,
        0,
        0,
        1,
        -1
    );
}

void WorldStateAdapter::EndAnchorPose() const {
    CPlayerPed* player = FindPlayerPed(-1);
    if (!player) return;
    const int pedRef = CPools::GetPedRef(player);
    if (pedRef >= 0) Command<Commands::CLEAR_CHAR_TASKS>(pedRef);
    player->bUsesCollision = true;
    player->SetIdle();
}

bool WorldStateAdapter::CaptureWorld(WorldFrame& out, uint64_t sequence) const {
    CPlayerPed* player = FindPlayerPed(-1);
    if (!player) return false;

    out = {};
    out.sequence = sequence;
    out.gameTimeMs = CTimer::GetTimeInMS();

    const CVector anchorPos = GetPhysicalPosition(player);
    const float radius2 = m_settings.radius * m_settings.radius;
    std::vector<Candidate> candidates;
    candidates.reserve(96);

    if (CPedPool* pool = CPools::ms_pPedPool) {
        const int size = pool->GetSize();
        for (int i = 0; i < size; ++i) {
            CPed* ped = pool->GetAt(i);
            if (!ped || ped == player) continue;
            if (ped->bInVehicle) continue;
            if (ped->m_nAreaCode != player->m_nAreaCode) continue;
            const float d2 = DistanceSq(GetPhysicalPosition(ped), anchorPos);
            if (d2 > radius2) continue;
            candidates.push_back({EntityKind::Ped, pool->GetRef(ped), d2});
        }
    }

    if (CVehiclePool* pool = CPools::ms_pVehiclePool) {
        const int size = pool->GetSize();
        for (int i = 0; i < size; ++i) {
            CVehicle* vehicle = pool->GetAt(i);
            if (!vehicle) continue;
            if (vehicle->m_nAreaCode != player->m_nAreaCode) continue;
            const float d2 = DistanceSq(GetPhysicalPosition(vehicle), anchorPos);
            if (d2 > radius2) continue;
            candidates.push_back({EntityKind::Vehicle, pool->GetRef(vehicle), d2});
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.dist2 < b.dist2;
    });
    if (candidates.size() > m_settings.maxEntities) candidates.resize(m_settings.maxEntities);

    for (const Candidate& c : candidates) {
        if (out.count >= kHardMaxWorldEntities) break;
        EntitySnapshot& snap = out.entities[out.count];
        snap = {};
        snap.kind = c.kind;
        snap.ref = c.ref;

        if (c.kind == EntityKind::Ped) {
            CPed* ped = CPools::GetPed(c.ref);
            if (!ped || ped == player || ped->bInVehicle) continue;
            CapturePhysical(ped, snap.physical);
            snap.health = ped->m_fHealth;
            snap.armour = ped->m_fArmour;
            snap.currentRotation = ped->m_fCurrentRotation;
            snap.aimingRotation = ped->m_fAimingRotation;
        } else {
            CVehicle* vehicle = CPools::GetVehicle(c.ref);
            if (!vehicle) continue;
            CapturePhysical(vehicle, snap.physical);
            snap.health = vehicle->m_fHealth;
        }

        ++out.count;
    }

    return true;
}

const EntitySnapshot* WorldStateAdapter::FindSnapshot(const WorldFrame& frame, EntityKind kind, int32_t ref) {
    for (uint16_t i = 0; i < frame.count; ++i) {
        const EntitySnapshot& s = frame.entities[i];
        if (s.kind == kind && s.ref == ref) return &s;
    }
    return nullptr;
}

bool WorldStateAdapter::ApplyEntity(const EntitySnapshot& target, const EntitySnapshot* from, float alpha) const {
    const PhysicalState physical = from ? LerpPhysical(from->physical, target.physical, alpha) : target.physical;

    if (target.kind == EntityKind::Ped) {
        CPed* ped = CPools::GetPed(target.ref);
        if (!ped || ped->IsPlayer() || ped->bInVehicle) return false;
        ApplyPhysical(ped, physical);
        if (from) {
            ped->m_fCurrentRotation = LerpAngle(from->currentRotation, target.currentRotation, alpha);
            ped->m_fAimingRotation = LerpAngle(from->aimingRotation, target.aimingRotation, alpha);
        } else {
            ped->m_fCurrentRotation = target.currentRotation;
            ped->m_fAimingRotation = target.aimingRotation;
        }
        if (m_settings.restoreHealth && ped->IsAlive()) {
            ped->m_fHealth = target.health;
            ped->m_fArmour = target.armour;
        }
        return true;
    }

    CVehicle* vehicle = CPools::GetVehicle(target.ref);
    if (!vehicle) return false;
    ApplyPhysical(vehicle, physical);
    if (m_settings.restoreHealth && vehicle->m_fHealth > 0.0f) vehicle->m_fHealth = target.health;
    return true;
}

bool WorldStateAdapter::ApplyWorldInterpolated(const WorldFrame& newer, const WorldFrame& older, float alpha) const {
    alpha = Clamp01(alpha);
    bool appliedAny = false;
    for (uint16_t i = 0; i < older.count; ++i) {
        const EntitySnapshot& target = older.entities[i];
        const EntitySnapshot* from = FindSnapshot(newer, target.kind, target.ref);
        appliedAny |= ApplyEntity(target, from, alpha);
    }
    return appliedAny;
}

void WorldStateAdapter::QuiesceWorld(const WorldFrame* frame) const {
    if (!frame) return;
    for (uint16_t i = 0; i < frame->count; ++i) {
        const EntitySnapshot& s = frame->entities[i];
        CPhysical* physical = nullptr;
        if (s.kind == EntityKind::Ped) {
            CPed* ped = CPools::GetPed(s.ref);
            if (!ped || ped->IsPlayer() || ped->bInVehicle) continue;
            physical = ped;
        } else {
            physical = CPools::GetVehicle(s.ref);
        }
        if (!physical) continue;
        physical->m_vecMoveSpeed = CVector{};
        physical->m_vecTurnSpeed = CVector{};
    }
}

void WorldStateAdapter::QuiescePlayer() const {
    CPlayerPed* player = FindPlayerPed(-1);
    if (!player || player->bInVehicle) return;
    player->m_vecMoveSpeed = CVector{};
    player->m_vecTurnSpeed = CVector{};
}

} // namespace noxxa
