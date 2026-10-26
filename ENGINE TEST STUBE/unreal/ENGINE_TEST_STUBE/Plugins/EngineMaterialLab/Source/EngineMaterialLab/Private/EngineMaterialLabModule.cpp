// Isolated UE 5.8 adapter; no modification of Unreal Engine sources.
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "upc/core.hpp"
#include <memory>

class FEngineMaterialLabModule final : public IModuleInterface
{
    std::unique_ptr<upc::UnifiedPhysicsCore> CorePhysics;
    FTSTicker::FDelegateHandle TickHandle;

    bool Advance(float DeltaSeconds)
    {
        if (CorePhysics && DeltaSeconds > 0.0f)
            CorePhysics->step(FMath::Clamp((double)DeltaSeconds, 0.00001, 0.033));
        return true;
    }

public:
    virtual void StartupModule() override
    {
        CorePhysics = std::make_unique<upc::UnifiedPhysicsCore>(8,8,8,0.1);
        CorePhysics->materials().add({1,upc::Phase::Liquid,1000.0,0.001});
        auto& Water = CorePhysics->add_water(16,12,0.1,1);
        for(std::size_t y=0;y<12;++y)
            for(std::size_t x=0;x<5;++x) Water.set_depth(x,y,0.6);
        TickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this,&FEngineMaterialLabModule::Advance),0.033f);
        UE_LOG(LogTemp,Display,TEXT("EngineMaterialLab UPC started (16x12 water CPU prototype)"));
    }
    virtual void ShutdownModule() override
    {
        if(TickHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        CorePhysics.reset();
        UE_LOG(LogTemp,Display,TEXT("EngineMaterialLab UPC stopped"));
    }
};
IMPLEMENT_MODULE(FEngineMaterialLabModule,EngineMaterialLab)
