// Fill out your copyright notice in the Description page of Project Settings.


#include "DSimGoalActor.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/BoxComponent.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/AI/Training/DSimSimulationManager.h"
#include "DSim/Character/DSimCharacter.h"
#include "DSim/Game/DSimGameMode.h"
#include "DSim/Libraries/DSimBlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ADSimGoalActor::ADSimGoalActor()
{
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>("StaticMesh");
	BoxCollisionComp = CreateDefaultSubobject<UBoxComponent>("BoxComponent");
	
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ADSimGoalActor::BeginPlay()
{
	Super::BeginPlay();

	BoxCollisionComp->OnComponentBeginOverlap.AddDynamic(this,&ADSimGoalActor::OnBoxBeginOverlap);
}

void ADSimGoalActor::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ADSimCharacter* Character = Cast<ADSimCharacter>(OtherActor);
	if (IsValid(Character))
	{
		ADSimCharacterAIController* AIController = Cast<ADSimCharacterAIController>(Character->GetController());
		if (IsValid(AIController))
		{
			ADSimSimulationManager* SimulationManager = UDSimBlueprintFunctionLibrary::GetSimulationManager(this);
			if (!IsValid(SimulationManager))
			{
				return;
			}

			SimulationManager->NotifyBotReachedGoal(Character);
		}
	}
}

// Called every frame
void ADSimGoalActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

