#pragma once
// Engine-independent policy; consumed by both runtime systems and native tests.
namespace HSPolicy
{
    constexpr int Capacity = 10;
    inline bool ValidSlot(int Index) { return Index >= 0 && Index < Capacity; }
    inline bool IsCrouchTap(double HeldSeconds, double Threshold) { return HeldSeconds >= 0 && HeldSeconds < Threshold; }
    inline float MoveSpeed(bool Crouched, bool Sprint, bool Slow, float Walk, float Run, float SlowWalk, float Crouch)
    { return Crouched ? Crouch : Slow ? SlowWalk : Sprint ? Run : Walk; }
    inline float ChaseSpeed(float Distance, float NearRadius, float FarRadius, float NearSpeed, float FarSpeed)
    {
        const float Span = FarRadius > NearRadius ? FarRadius - NearRadius : 1.f;
        float T = (Distance - NearRadius) / Span;
        T = T < 0 ? 0 : T > 1 ? 1 : T;
        const float Smooth = T * T * (3.f - 2.f * T);
        return NearSpeed + (FarSpeed - NearSpeed) * Smooth;
    }
}
