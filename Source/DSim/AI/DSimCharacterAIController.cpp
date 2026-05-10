// Fill out your copyright notice in the Description page of Project Settings.

#include "DSim/AI/DSimCharacterAIController.h"

#include "DSim/Character/DSimCharacter.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DSim/Actors/DSimGoalActor.h"
#include "DSim/Actors/SphereActor.h"
#include "DSim/Character/Drone/DSimDronePawn.h"
#include "DSim/Game/DSimGameMode.h"
#include "DSim/Libraries/DSimBlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"


ADSimCharacterAIController::ADSimCharacterAIController()
{
	RLComp = CreateDefaultSubobject<UDSimReinforcementLearningComp>("PPO");
}

void ADSimCharacterAIController::SetupDroneActor()
{
	TArray<AActor*> ResultActor;

	UGameplayStatics::GetAllActorsOfClass(this, ADSimDronePawn::StaticClass(), ResultActor);
	if (ResultActor.IsEmpty())
	{
		return;
	}

	GetBlackboardComponent()->SetValueAsObject(AIBlackboardKeys::DroneActor, ResultActor[0]);
}

void ADSimCharacterAIController::OnDeath()
{
	GetWorldTimerManager().ClearTimer(LocationUpdateTimer);

	UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent());
	if (BTComp)
	{
		BTComp->StopTree(EBTStopMode::Safe); // or EBTStopMode::Immediate
	}
}

void ADSimCharacterAIController::BeginPlay()
{
	Super::BeginPlay();

	ADSimGameMode* GameMode = Cast<ADSimGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (IsValid(GameMode))
	{
		GameMode->BotsInTheLevel.Add(this);
		GameMode->OnStopAllCharacters.AddDynamic(this, &ADSimCharacterAIController::OnDeath);
		GameMode->OnNeedToDrawDebugSpheres.AddDynamic(
			this, &ADSimCharacterAIController::ADSimCharacterAIController::DrawLocations);
	}
	SetupDroneActor();

	GetWorldTimerManager().SetTimer(LocationUpdateTimer, [this]()
	{
		if (!IsValid(CharacterRef))
		{
			return;
		}
		Locations.Add(CharacterRef->GetActorLocation());
		FVector AdjustedPoint = FVector(
		CharacterRef->GetActorLocation().X,
		CharacterRef->GetActorLocation().Y,
		CharacterRef->GetActorLocation().Z
	);

		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// GetWorld()->SpawnActor<ASphereActor>(
		//   SphereMarkerClass,
		//   AdjustedPoint,
		//   FRotator::ZeroRotator,
		//   Params
		// );
	}, 0.5f, true);
}

void ADSimCharacterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	CharacterRef = Cast<ADSimCharacter>(InPawn);

	if (IsValid(BehaviorTreeComponent) && IsValid(BlackboardComponent))
	{
		RunBehaviorTree(BehaviorTreeComponent);
	}

	TArray<AActor*> Goals;
	UGameplayStatics::GetAllActorsOfClass(this, ADSimGoalActor::StaticClass(), Goals);
	if (!Goals.IsEmpty())
	{
		const FVector GoalPosition = Goals[0]->GetActorLocation();
		GetBlackboardComponent()->SetValueAsVector(AIBlackboardKeys::GoalLocation, GoalPosition);

		// Важливо для 2D-дискретизації, щоб RL-компонент знав актуальну ціль ще до першого RequestAction()
		if (IsValid(RLComp))
		{
			RLComp->SetGoalPosition(GoalPosition);
		}
	}

	if (IsValid(RLComp))
	{
		RLComp->InitComponentData();
	}
}

void ADSimCharacterAIController::DrawLocations()
{
	UDSimBlueprintFunctionLibrary::DrawMovementPathInTheLevel(GetWorld(), Locations, FColor::Yellow, 100.0f, 20);
}
