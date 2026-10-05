#include "../Source/HorrorSystems/Public/HSPolicy.h"
#include <cmath>
#include <cstdio>
int Failures = 0;
void Check(bool Condition, const char* Message)
{
    if (!Condition) { std::printf("FAIL: %s\n", Message); ++Failures; }
}
int main()
{
    Check(HSPolicy::ValidSlot(0) && HSPolicy::ValidSlot(9), "all ten slots are selectable");
    Check(!HSPolicy::ValidSlot(-1) && !HSPolicy::ValidSlot(10), "indices outside ten slots are rejected");
    Check(HSPolicy::IsCrouchTap(.1, .22), "short Ctrl tap toggles crouch");
    Check(!HSPolicy::IsCrouchTap(.22, .22) && !HSPolicy::IsCrouchTap(.5, .22), "held Ctrl must not toggle crouch on release");
    Check(HSPolicy::MoveSpeed(false, false, false,420,650,150,230)==420, "normal fast walk");
    Check(HSPolicy::MoveSpeed(false, true, false,420,650,150,230)==650, "Shift sprint");
    Check(HSPolicy::MoveSpeed(false, true, true,420,650,150,230)==150, "slow walk wins conflicting modifiers");
    Check(HSPolicy::MoveSpeed(true, true, true,420,650,150,230)==230, "crouch always fixed speed");
    Check(HSPolicy::ChaseSpeed(100,800,1800,160,540)==160, "near monster speed");
    Check(HSPolicy::ChaseSpeed(2500,800,1800,160,540)==540, "far monster speed");
    Check(std::fabs(HSPolicy::ChaseSpeed(1300,800,1800,160,540)-350)<.01, "smooth transition midpoint");
    float Previous = 160;
    for(int D=800;D<=1800;D+=10)
    {
        float S=HSPolicy::ChaseSpeed(float(D),800,1800,160,540);
        Check(S>=Previous && S-Previous<7, "no sudden acceleration across transition");
        Previous=S;
    }
    Check(std::isfinite(HSPolicy::ChaseSpeed(1000,800,800,160,540)), "degenerate radii stay finite");
    std::printf("Policy checks: %s (%d failures)\n", Failures ? "FAILED" : "PASSED", Failures);
    return Failures ? 1 : 0;
}
