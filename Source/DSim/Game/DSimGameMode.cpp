// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSimGameMode.h"

#include "DSim/AI/DSimCharacterAIController.h"
#include "DSim/Character/DSimCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ADSimGameMode::ADSimGameMode()
{
}

void ADSimGameMode::PlayerEndGame()
{
	OnStopAllCharacters.Broadcast();
}

void ADSimGameMode::RestartGame()
{
	if (LevelRef)
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(GetWorld(),LevelRef);
	}
}

void ADSimGameMode::CloseGame()
{
	if (BotsInTheLevel.IsEmpty())
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), nullptr, EQuitPreference::Quit, false);
		return;
	}
}

void ADSimGameMode::DrawDebugSpheres()
{
	OnNeedToDrawDebugSpheres.Broadcast();
}