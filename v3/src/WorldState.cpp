#include "WorldState.hpp"

#include <aml-psdk/game_sa/base/Matrix.h>
#include <aml-psdk/game_sa/base/Timer.h>
#include <aml-psdk/game_sa/entity/Physical.h>
#include <aml-psdk/game_sa/entity/PlayerPed.h>
#include <aml-psdk/game_sa/entity/Vehicle.h>
#include <aml-psdk/game_sa/other/Pools.h>
#include <aml-psdk/game_sa/utils/OpcodeCaller.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace noxxa {
namespace {
struct Candidate { EntityKind kind{EntityKind::Ped}; int32_t ref{-1}; float dist2{0.0f}; };

float DistanceSq(const CVector& a, const CVector& b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}
float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
} // namespace

void WorldStateAdapter::Configure(const WorldCaptureSettings& settings) {
    m_settings = settings;
    m_settings.radius = std::clamp(m_settings.radius, 10.0f, 65.0f);
    m_settings.maxEntities = std::max<std::size_t>(4, std::min<std::size_t>(kHardMaxWorldEntities, m_settings.maxEntities));
}

bool WorldStateAdapter::IsFiniteVector(const CVector& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool WorldStateAdapter::IsSanePhysical(const PhysicalState& s) {
    if (!IsFiniteVector(s.transform.position) || !IsFiniteVector(s.moveSpeed) || !IsFiniteVector(s.turnSpeed)) return false;
    if (std::fabs(s.transform.position.x) > 100000.0f || std::fabs(s.transform.position.y) > 100000.0f || std::fabs(s.transform.position.z) > 100000.0f) return false;
    if (s.transform.hasMatrix && (!IsFiniteVector(s.transform.right) || !IsFiniteVector(s.transform.forward) || !IsFiniteVector(s.transform.up))) return false;
    return true;
}

CVector WorldStateAdapter::GetPhysicalPosition(const CPhysical* physical) {
    if (!physical) return CVector{};
    auto* p = const_cast<CPhysical*>(physical);
    if (CMatrix* matrix = p->GetMatrix()) return matrix->pos;
    return p->m_placement.m_vPosn;
}

bool WorldStateAdapter::CapturePhysical(CPhysical* physical, PhysicalState& out) {
    if (!physical) return false;
    out = {};
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
    return IsSanePhysical(out);
}

bool WorldStateAdapter::ApplyPhysical(CPhysical* physical, const PhysicalState& state) {
    if (!physical || !IsSanePhysical(state)) return false;
    CMatrix* matrix = physical->GetMatrix();
    if (state.transform.hasMatrix && matrix) {
        matrix->right = state.transform.right;
        matrix->up = state.transform.forward;
        matrix->at = state.transform.up;
        matrix->pos = state.transform.position;
        matrix->Reorthogonalise();
        matrix->UpdateRW();
    } else if (matrix) {
        matrix->SetRotateZ(state.transform.heading);
        matrix->SetTranslateOnly(state.transform.position.x, state.transform.position.y, state.transform.position.z);
        matrix->UpdateRW();
    } else {
        physical->m_placement.m_vPosn = state.transform.position;
        if (!state.transform.hasMatrix) physical->m_placement.m_fHeading = state.transform.heading;
    }
    physical->m_vecMoveSpeed = state.moveSpeed;
    physical->m_vecTurnSpeed = state.turnSpeed;
    physical->UpdateRwFrame();
    return true;
}

CVector WorldStateAdapter::LerpVector(const CVector& a, const CVector& b, float t) {
    t = Clamp01(t);
    return CVector(a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t, a.z + (b.z-a.z)*t);
}

float WorldStateAdapter::LerpAngle(float a, float b, float t) {
    constexpr float kPi = 3.14159265358979323846f, kTwoPi = 2.0f * kPi;
    float d = std::fmod(b - a, kTwoPi);
    if (d > kPi) d -= kTwoPi;
    if (d < -kPi) d += kTwoPi;
    return a + d * Clamp01(t);
}

PhysicalState WorldStateAdapter::LerpPhysical(const PhysicalState& a, const PhysicalState& b, float t) {
    t = Clamp01(t);
    PhysicalState out{};
    out.transform.position = LerpVector(a.transform.position, b.transform.position, t);
    if (a.transform.hasMatrix && b.transform.hasMatrix) {
        out.transform.hasMatrix = true;
        out.transform.right = LerpVector(a.transform.right, b.transform.right, t);
        out.transform.forward = LerpVector(a.transform.forward, b.transform.forward, t);
        out.transform.up = LerpVector(a.transform.up, b.transform.up, t);
    } else if (!a.transform.hasMatrix && !b.transform.hasMatrix) {
        out.transform.hasMatrix = false;
        out.transform.heading = LerpAngle(a.transform.heading, b.transform.heading, t);
    } else {
        out.transform = (t < 0.5f) ? a.transform : b.transform;
        out.transform.position = LerpVector(a.transform.position, b.transform.position, t);
    }
    out.moveSpeed = LerpVector(a.moveSpeed, b.moveSpeed, t);
    out.turnSpeed = LerpVector(a.turnSpeed, b.turnSpeed, t);
    return out;
}

bool WorldStateAdapter::PlayerCanAnchor() const {
    CPlayerPed* p = FindPlayerPed(-1);
    return p && p->IsAlive() && !p->bInVehicle;
}
bool WorldStateAdapter::IsPlayerInVehicle() const { CPlayerPed* p = FindPlayerPed(-1); return p && p->bInVehicle; }

bool WorldStateAdapter::CaptureAnchor(PlayerAnchor& out) const {
    CPlayerPed* p = FindPlayerPed(-1);
    if (!p || p->bInVehicle) return false;
    out = {};
    out.valid = true;
    out.pedRef = CPools::GetPedRef(p);
    if (out.pedRef < 0 || !CapturePhysical(p, out.physical)) return false;
    out.health = p->m_fHealth;
    out.armour = p->m_fArmour;
    out.currentRotation = p->m_fCurrentRotation;
    out.aimingRotation = p->m_fAimingRotation;
    return true;
}

bool WorldStateAdapter::ApplyAnchor(const PlayerAnchor& a) const {
    if (!a.valid) return false;
    CPed* ped = CPools::GetPed(a.pedRef);
    if (!ped || !ped->IsPlayer()) return false;
    auto* p = static_cast<CPlayerPed*>(ped);
    if (p->bInVehicle || !ApplyPhysical(p, a.physical)) return false;
    p->m_fHealth = a.health;
    p->m_fArmour = a.armour;
    p->m_fCurrentRotation = a.currentRotation;
    p->m_fAimingRotation = a.aimingRotation;
    return true;
}

void WorldStateAdapter::BeginAnchorPose(const char* anim, const char* ifp) const {
    CPlayerPed* p = FindPlayerPed(-1);
    if (!p || p->bInVehicle) return;
    const int ref = CPools::GetPedRef(p);
    if (ref < 0) return;
    // Safe one-shot focus gesture. Never clear/free the task tree here.
    Command<Commands::TASK_PLAY_ANIM>(ref,
        (anim && *anim) ? anim : "IDLE_TAXI",
        (ifp && *ifp) ? ifp : "PED",
        2.0f, 0, 0, 0, 0, 700);
}

void WorldStateAdapter::EndAnchorPose() const {
    CPlayerPed* p = FindPlayerPed(-1);
    if (!p) return;
    p->RestartNonPartialAnims();
    p->RestoreHeadingRate();
}

bool WorldStateAdapter::CaptureWorld(WorldFrame& out, uint64_t sequence) const {
    CPlayerPed* player = FindPlayerPed(-1);
    if (!player) return false;
    out = {};
    out.sequence = sequence;
    out.gameTimeMs = CTimer::GetTimeInMS();
    const CVector anchorPos = GetPhysicalPosition(player);
    if (!IsFiniteVector(anchorPos)) return false;
    const float radius2 = m_settings.radius * m_settings.radius;
    std::array<Candidate, kHardMaxWorldEntities> candidates{};
    std::size_t candidateCount = 0;

    auto consider = [&](EntityKind kind, int32_t ref, float d2) {
        if (ref < 0 || !std::isfinite(d2)) return;
        if (candidateCount < m_settings.maxEntities) { candidates[candidateCount++] = Candidate{kind, ref, d2}; return; }
        std::size_t worst = 0;
        for (std::size_t i=1;i<candidateCount;++i) if (candidates[i].dist2 > candidates[worst].dist2) worst=i;
        if (d2 < candidates[worst].dist2) candidates[worst] = Candidate{kind, ref, d2};
    };

    if (CPedPool* pool = CPools::ms_pPedPool) {
        for (int i=0;i<pool->GetSize();++i) {
            CPed* ped = pool->GetAt(i);
            if (!ped || ped==player || ped->bInVehicle || !ped->IsAlive() || ped->m_nAreaCode != player->m_nAreaCode) continue;
            const CVector pos = GetPhysicalPosition(ped);
            if (!IsFiniteVector(pos)) continue;
            const float d2 = DistanceSq(pos, anchorPos);
            if (d2 <= radius2) consider(EntityKind::Ped, pool->GetRef(ped), d2);
        }
    }
    if (CVehiclePool* pool = CPools::ms_pVehiclePool) {
        for (int i=0;i<pool->GetSize();++i) {
            CVehicle* v = pool->GetAt(i);
            if (!v || v->m_nAreaCode != player->m_nAreaCode) continue;
            const CVector pos = GetPhysicalPosition(v);
            if (!IsFiniteVector(pos)) continue;
            const float d2 = DistanceSq(pos, anchorPos);
            if (d2 <= radius2) consider(EntityKind::Vehicle, pool->GetRef(v), d2);
        }
    }

    std::sort(candidates.begin(), candidates.begin()+candidateCount, [](const Candidate& a,const Candidate& b){return a.dist2<b.dist2;});
    for (std::size_t ci=0; ci<candidateCount && out.count<kHardMaxWorldEntities; ++ci) {
        const Candidate& c = candidates[ci];
        EntitySnapshot snap{};
        snap.kind = c.kind;
        snap.ref = c.ref;
        if (c.kind == EntityKind::Ped) {
            CPed* ped = CPools::GetPed(c.ref);
            if (!ped || ped==player || ped->bInVehicle || !ped->IsAlive()) continue;
            snap.modelIndex = ped->m_nModelIndex;
            if (!CapturePhysical(ped, snap.physical)) continue;
            snap.health=ped->m_fHealth; snap.armour=ped->m_fArmour;
            snap.currentRotation=ped->m_fCurrentRotation; snap.aimingRotation=ped->m_fAimingRotation;
        } else {
            CVehicle* v = CPools::GetVehicle(c.ref);
            if (!v) continue;
            snap.modelIndex = v->m_nModelIndex;
            if (!CapturePhysical(v, snap.physical)) continue;
            snap.health=v->m_fHealth;
        }
        out.entities[out.count++] = snap;
    }
    return true;
}

const EntitySnapshot* WorldStateAdapter::FindSnapshot(const WorldFrame& f, EntityKind kind, int32_t ref) {
    for (uint16_t i=0;i<f.count;++i) if (f.entities[i].kind==kind && f.entities[i].ref==ref) return &f.entities[i];
    return nullptr;
}

bool WorldStateAdapter::ApplyEntity(const EntitySnapshot& target, const EntitySnapshot* from, float alpha) const {
    // No matching newer sample means the entity entered/leaved the capture set;
    // skipping avoids one-frame teleports and ref-reuse pops.
    if (!from || from->modelIndex != target.modelIndex) return false;
    if (DistanceSq(from->physical.transform.position, target.physical.transform.position) > 400.0f) return false;
    const PhysicalState state = LerpPhysical(from->physical, target.physical, alpha);
    CPlayerPed* player = FindPlayerPed(-1);
    if (!player) return false;

    if (target.kind == EntityKind::Ped) {
        CPed* ped = CPools::GetPed(target.ref);
        if (!ped || ped->IsPlayer() || ped->bInVehicle || !ped->IsAlive()) return false;
        if (ped->m_nModelIndex != target.modelIndex || ped->m_nAreaCode != player->m_nAreaCode) return false;
        if (!ApplyPhysical(ped, state)) return false;
        ped->m_fCurrentRotation = LerpAngle(from->currentRotation, target.currentRotation, alpha);
        ped->m_fAimingRotation = LerpAngle(from->aimingRotation, target.aimingRotation, alpha);
        if (m_settings.restoreHealth) { ped->m_fHealth=target.health; ped->m_fArmour=target.armour; }
        return true;
    }

    CVehicle* v = CPools::GetVehicle(target.ref);
    if (!v || v->m_nModelIndex != target.modelIndex || v->m_nAreaCode != player->m_nAreaCode) return false;
    if (!ApplyPhysical(v, state)) return false;
    if (m_settings.restoreHealth && v->m_fHealth > 0.0f) v->m_fHealth=target.health;
    return true;
}

bool WorldStateAdapter::ApplyWorldInterpolated(const WorldFrame& newer, const WorldFrame& older, float alpha) const {
    bool any=false; alpha=Clamp01(alpha);
    for (uint16_t i=0;i<older.count;++i) {
        const EntitySnapshot& target=older.entities[i];
        any |= ApplyEntity(target, FindSnapshot(newer,target.kind,target.ref), alpha);
    }
    return any;
}

void WorldStateAdapter::QuiesceWorld(const WorldFrame* frame) const {
    if (!frame) return;
    for (uint16_t i=0;i<frame->count;++i) {
        const EntitySnapshot& s=frame->entities[i];
        CPhysical* p=nullptr;
        if (s.kind==EntityKind::Ped) {
            CPed* ped=CPools::GetPed(s.ref);
            if (!ped || ped->IsPlayer() || ped->bInVehicle || !ped->IsAlive() || ped->m_nModelIndex != s.modelIndex) continue;
            p=ped;
        } else {
            CVehicle* v=CPools::GetVehicle(s.ref);
            if (!v || v->m_nModelIndex != s.modelIndex) continue;
            p=v;
        }
        p->m_vecMoveSpeed=CVector{}; p->m_vecTurnSpeed=CVector{};
    }
}

void WorldStateAdapter::QuiescePlayer() const {
    CPlayerPed* p=FindPlayerPed(-1);
    if (!p || p->bInVehicle) return;
    p->m_vecMoveSpeed=CVector{}; p->m_vecTurnSpeed=CVector{};
}

} // namespace noxxa
