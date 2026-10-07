#include "Misc/AutomationTest.h"
#include "HSWorldActors.h"
#include "Components/PostProcessComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSBrightnessTest,"HorrorSystems.Vision.BrightnessScale",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHSBrightnessTest::RunTest(const FString&)
{
    float Multiplier=1.f;
    TestTrue(TEXT("Brightness multiplier is configurable"),GConfig->GetFloat(TEXT("/Script/HorrorSystems.HSSettings"),TEXT("SceneBrightnessMultiplier"),Multiplier,GGameIni));
    bool Found=false;
    for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game || C.WorldType==EWorldType::PIE)
        for(TActorIterator<AHSVisionRig> It(C.World());It;++It) if(It->bUseProjectSettings)
        {
            Found=true;
            const float Expected=-1.f+FMath::Log2(FMath::Max(.1f,Multiplier));
            TestTrue(TEXT("Global scene exposure applies the configured brightness multiplier"),FMath::IsNearlyEqual(It->PostProcess->Settings.AutoExposureBias,Expected,1.e-5f));
        }
    TestTrue(TEXT("Game map has a configured vision rig"),Found);
    return true;
}
#endif
