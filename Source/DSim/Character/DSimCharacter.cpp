// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSimCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/SphereComponent.h"
#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/AI/Training/DSimSimulationManager.h"
#include "DSim/Game/DSimGameMode.h"
#include "DSim/Libraries/DSimBlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ADSimCharacter

ADSimCharacter::ADSimCharacter()
{
	CoverDetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CoverSphere"));
	CoverDetectionSphere->InitSphereRadius(3000.f);
	CoverDetectionSphere->SetupAttachment(GetRootComponent());
	CoverDetectionSphere->SetCollisionProfileName(TEXT("OverlapAll"));

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 800.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 500.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
}

inline void ADSimCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	DSimAIController = Cast<ADSimCharacterAIController>(NewController);
}

void ADSimCharacter::GetOverlappedDamage(float DamageAmount)
{
	ADSimSimulationManager* SimulationManager = UDSimBlueprintFunctionLibrary::GetSimulationManager(this);

	if (IsValid(SimulationManager))
	{
		SimulationManager->NotifyBotKilled(this);
		return;
	}

	// Legacy fallback, якщо менеджера на рівні немає.
	if (!IsValid(DSimAIController))
	{
		return;
	}

	if (IsValid(DSimAIController->RLComp))
	{
		DSimAIController->RLComp->EndEpisode(EDSimEpisodeFinishReason::BotKilledByDrone);
	}
}

void ADSimCharacter::BeginPlay()
{
	Super::BeginPlay();
}
