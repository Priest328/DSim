// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSim/Interfaces/DroneTargetInterface.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "DSimCharacter.generated.h"

class ADSimCharacterAIController;
class USphereComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class ADSimCharacter : public ACharacter, public IDroneTargetInterface
{
	GENERATED_BODY()

public:
	ADSimCharacter();

	virtual void GetOverlappedDamage(float DamageAmount) override;

protected:
	virtual void BeginPlay();

	virtual void PossessedBy(AController* NewController) override;

public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TObjectPtr<USphereComponent> CoverDetectionSphere;

	UPROPERTY()
	TObjectPtr<ADSimCharacterAIController> DSimAIController;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "DSim|Training")
	FName BotEnvironmentTag;
};


