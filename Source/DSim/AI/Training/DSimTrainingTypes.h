#pragma once

#include "CoreMinimal.h"
#include "DSimTrainingTypes.generated.h"

UENUM(BlueprintType)
enum class EBotAction : uint8
{
	None        = 0 UMETA(DisplayName = "None"),
	TowardGoal  = 1 UMETA(DisplayName = "Toward Goal"),
	TowardCover = 2 UMETA(DisplayName = "Toward Cover"),
	RandomMove  = 3 UMETA(DisplayName = "Random Move")
};

UENUM(BlueprintType)
enum class EDSimStateRepresentationMode : uint8
{
	OneD            UMETA(DisplayName = "1D"),
	TwoD            UMETA(DisplayName = "2D"),
	TwoDThreatAware UMETA(DisplayName = "2D + Threat Aware")
};

UENUM(BlueprintType)
enum class EDSimEpisodeFinishReason : uint8
{
	Unknown          UMETA(DisplayName = "Unknown"),
	GoalReached      UMETA(DisplayName = "Goal Reached"),
	BotKilledByDrone UMETA(DisplayName = "Bot Killed By Drone"),
	DroneCrashed     UMETA(DisplayName = "Drone Crashed"),
	Timeout          UMETA(DisplayName = "Timeout"),
	StoppedManually  UMETA(DisplayName = "Stopped Manually")
};

USTRUCT(BlueprintType)
struct FDSimAlgorithmRuntimeContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 ArenaId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 RunId = 0;

	UPROPERTY(BlueprintReadOnly)
	FString InitialTrainingFile;

	UPROPERTY(BlueprintReadOnly)
	FString OutputTrainingFile;

	UPROPERTY(BlueprintReadOnly)
	FString OutputDirectory;

	UPROPERTY(BlueprintReadOnly)
	EDSimStateRepresentationMode StateRepresentationMode = EDSimStateRepresentationMode::TwoD;
};