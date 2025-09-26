// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DSimGameMode.generated.h"

class ADSimCharacterAIController;
class ADSimCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStopAllCharacters);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNeedToDrawDebugSpheres);

UCLASS()
class ADSimGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADSimGameMode();

	void PlayerEndGame();

	UFUNCTION(BlueprintCallable)
	void RestartGame();

	void CloseGame();

	UFUNCTION(BlueprintCallable)
	void DrawDebugSpheres();

	UPROPERTY(BlueprintAssignable)
	FOnStopAllCharacters OnStopAllCharacters;

	UPROPERTY(BlueprintAssignable)
	FOnNeedToDrawDebugSpheres OnNeedToDrawDebugSpheres;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UWorld> LevelRef;

	UPROPERTY()
	TArray<TObjectPtr<ADSimCharacterAIController>> BotsInTheLevel;
};



