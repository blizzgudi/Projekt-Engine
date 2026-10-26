// Native Unreal 5.8 3D MAC/VOF visualization adapter for standalone UPC.
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SceneCapture2D.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/DirectionalLight.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "upc/fluid3d_module.hpp"
#include <memory>

DEFINE_LOG_CATEGORY_STATIC(LogEngineMaterialLab,Log,All);
class FEngineMaterialLabModule final:public IModuleInterface {
 std::unique_ptr<upc::UnifiedPhysicsRuntime> Physics;
 upc::CoupledMacWater3D* Water=nullptr;
 FTSTicker::FDelegateHandle TickHandle;
 TWeakObjectPtr<UWorld> ActiveWorld;
 TWeakObjectPtr<AActor> Root;
 TWeakObjectPtr<UInstancedStaticMeshComponent> VoxelMesh;
 TWeakObjectPtr<ASceneCapture2D> Camera;
 UTextureRenderTarget2D* RT=nullptr;
 FString Kind,OutputDir;
 int32 Frames=0;
 int64 ViscousSubsteps=0;
 bool Done=false,AutoQuit=false;
 double InitialMass=0,MassError=0,Outside=0;
 void Reset(){
   if(RT){RT->RemoveFromRoot();RT=nullptr;}
   Physics.reset();Water=nullptr;Root.Reset();VoxelMesh.Reset();
   Camera.Reset();ActiveWorld.Reset();Frames=0;ViscousSubsteps=0;Done=false;
 }
 void Initialize(UWorld* W){
   Reset();ActiveWorld=W;Kind=W->WorldType==EWorldType::PIE?TEXT("PIE"):TEXT("GAME");
   Physics=std::make_unique<upc::UnifiedPhysicsRuntime>(12,8,6,0.1);
   Physics->core().materials().add({1,upc::Phase::Liquid,1000,0.001});
   Water=&Physics->add_water3d(12,8,6,0.1,1);
   Water->set_kinematic_viscosity(0.005); // P2 live masked MAC/VOF integration proof
   for(int z=0;z<4;z++)for(int y=0;y<8;y++)for(int x=0;x<4;x++)Water->set_water(x,y,z,1);
   InitialMass=Water->mass_kg();MassError=0;Outside=0;
   Root=W->SpawnActor<AActor>(AActor::StaticClass(),FVector(0),FRotator(0));
   UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
   if(!Root.IsValid()||!Cube){UE_LOG(LogEngineMaterialLab,Error,TEXT("P8 Mesh/Root load FAIL"));Reset();return;}
   auto* Mesh=NewObject<UInstancedStaticMeshComponent>(Root.Get(),TEXT("UPC3DWaterRenderer"));
   Root->AddInstanceComponent(Mesh);Mesh->SetStaticMesh(Cube);
   Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   Mesh->RegisterComponent();VoxelMesh=Mesh;
   W->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(),
                                   FVector(0,0,700),FRotator(-55,-25,0));
   Camera=W->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(),
                                       FVector(-450,-500,330),FRotator(-20,47,0));
   if(Camera.IsValid()){
      RT=NewObject<UTextureRenderTarget2D>();RT->AddToRoot();
      RT->RenderTargetFormat=RTF_RGBA8;RT->InitAutoFormat(960,540);
      RT->UpdateResourceImmediate(true);
      auto* Capture=Camera->GetCaptureComponent2D();
      Capture->TextureTarget=RT;Capture->CaptureSource=SCS_FinalColorLDR;
      Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->FOVAngle=63;
   }
   OutputDir=FPaths::ProjectSavedDir()/TEXT("EngineMaterialLab/P8Native");
   IFileManager::Get().MakeDirectory(*OutputDir,true);
   UE_LOG(LogEngineMaterialLab,Display,TEXT("P8 WORLD START kind=%s native3D=1 initial_mass=%.10f"),
          *Kind,InitialMass);
 }
 void Draw(){
   if(!Water||!VoxelMesh.IsValid())return;
   auto* Mesh=VoxelMesh.Get();Mesh->ClearInstances();
   for(int z=0;z<6;z++)for(int y=0;y<8;y++)for(int x=0;x<12;x++){
     const double f=Water->water_fraction(x,y,z);
     if(f<0.035)continue;
     float h=36.0f*FMath::Clamp((float)f,0.035f,1.0f);
     Mesh->AddInstance(FTransform(FRotator::ZeroRotator,
             FVector((float)x*36,(float)y*36,(float)z*36+h/2),
             FVector(0.34f,0.34f,h/100)));
     if(x>=4)Outside=FMath::Max(Outside,f);
   }
 }
 void Prove(){
   if(Done||!Water||!ActiveWorld.IsValid())return;
   Done=true;
   const FString Base=FString::Printf(TEXT("P8_%s_UE58_Native_3DWater"),*Kind);
   const FString Image=OutputDir/(Base+TEXT(".png"));
   if(Camera.IsValid()&&RT){
     Camera->GetCaptureComponent2D()->CaptureScene();
     UKismetRenderingLibrary::ExportRenderTarget(ActiveWorld.Get(),RT,OutputDir,Base+TEXT(".png"));
   }
   const int64 Bytes=IFileManager::Get().FileSize(*Image);
   const int32 MeshCount=VoxelMesh.IsValid()?VoxelMesh->GetInstanceCount():0;
   const bool MassOK=MassError<1e-6,FlowOK=Outside>0.04,RenderOK=Bytes>4096,MeshOK=MeshCount>0;
   const FString Json=FString::Printf(
     TEXT("{\"kind\":\"%s\",\"ue_version\":\"%s\",\"native_mac_3d\":true,"
          "\"frames\":%d,\"initial_mass_kg\":%.12f,\"final_mass_kg\":%.12f,"
          "\"max_mass_residual_kg\":%.12f,\"max_outside_vof\":%.9f,"
          "\"mesh_instances\":%d,\"png_bytes\":%lld,"
          "\"viscous_substeps\":%lld,\"kinematic_viscosity_m2_s\":%.6f,"
          "\"mass_ok\":%s,\"flow_ok\":%s,\"mesh_ok\":%s,\"render_ok\":%s}"),
     *Kind,*FEngineVersion::Current().ToString(),Frames,InitialMass,Water->mass_kg(),
     MassError,Outside,MeshCount,Bytes,ViscousSubsteps,
     Water->kinematic_viscosity_m2_s(),MassOK?TEXT("true"):TEXT("false"),
     FlowOK?TEXT("true"):TEXT("false"),MeshOK?TEXT("true"):TEXT("false"),
     RenderOK?TEXT("true"):TEXT("false"));
   const FString Report=OutputDir/(Base+TEXT(".json"));
   FFileHelper::SaveStringToFile(Json,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
   UE_LOG(LogEngineMaterialLab,Display,
      TEXT("P8 NATIVE E2E kind=%s mass=%d flow=%d mesh=%d render=%d png=%lld frames=%d report=%s"),
      *Kind,MassOK,FlowOK,MeshOK,RenderOK,Bytes,Frames,*Report);
 }
 bool Tick(float Delta){
   if(!GEngine)return true;
   UWorld* W=nullptr;
   for(const FWorldContext& C:GEngine->GetWorldContexts()){
      UWorld* Test=C.World();
      if(Test&&(Test->WorldType==EWorldType::PIE||Test->WorldType==EWorldType::Game)
             && Test->HasBegunPlay()){W=Test;break;}
   }
   if(!W){if(ActiveWorld.IsValid())Reset();return true;}
   if(ActiveWorld.Get()!=W)Initialize(W);
   if(!Water||!FMath::IsFinite(Delta)||Delta<=0)return true;
   try{Physics->step(0.012,100);}
   catch(const std::exception& E){
      UE_LOG(LogEngineMaterialLab,Error,TEXT("P8 native physics failed: %hs"),E.what());
      return true;
   }
   Frames++;ViscousSubsteps+=static_cast<int64>(Water->last_viscosity_substeps());
   MassError=FMath::Max(MassError,FMath::Abs(Water->mass_kg()-InitialMass));
   Draw();
   if(Frames==85)Prove();
   if(AutoQuit&&Frames>=95){UE_LOG(LogEngineMaterialLab,Display,TEXT("P8 AUTOQUIT"));
       FPlatformMisc::RequestExit(false);}
   return true;
 }
public:
 void StartupModule()override{
   AutoQuit=FParse::Param(FCommandLine::Get(),TEXT("EngineMaterialLabAutoQuit"));
   TickHandle=FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateRaw(this,&FEngineMaterialLabModule::Tick),0.018f);
   UE_LOG(LogEngineMaterialLab,Display,TEXT("EngineMaterialLab UPC started (native 3D UE5.8 adapter)"));
 }
 void ShutdownModule()override{
   if(TickHandle.IsValid())FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
   // Modules are unloaded after UObjectArray teardown during a clean UE exit.
   // Never RemoveFromRoot or touch UObjects in ShutdownModule().
   Physics.reset();Water=nullptr;RT=nullptr;
   UE_LOG(LogEngineMaterialLab,Display,TEXT("EngineMaterialLab UPC stopped"));
 }
};
IMPLEMENT_MODULE(FEngineMaterialLabModule,EngineMaterialLab)
