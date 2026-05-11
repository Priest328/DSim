#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DSimDroneTelemetryComponent.generated.h"

USTRUCT(BlueprintType)
struct FDroneTelemetryFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 EpisodeId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 ArenaId = 0;

	UPROPERTY(BlueprintReadOnly)
	float TimeSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly)
	FVector DroneLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector DesiredTarget = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector SafeTarget = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	float DistanceToTarget = 0.f;

	UPROPERTY(BlueprintReadOnly)
	bool bHasLineOfSight = false;

	UPROPERTY(BlueprintReadOnly)
	bool bObstacleAhead = false;

	UPROPERTY(BlueprintReadOnly)
	uint8 DroneMode = 0;

	UPROPERTY(BlueprintReadOnly)
	uint8 DroneState = 0;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DSIM_API UDSimDroneTelemetryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDSimDroneTelemetryComponent();

	UFUNCTION(BlueprintCallable, Category = "Drone|Telemetry")
	void CaptureFrame();

	UFUNCTION(BlueprintCallable, Category = "Drone|Telemetry")
	void ClearFrames();

	UFUNCTION(BlueprintCallable, Category = "Drone|Telemetry")
	void SetEpisodeId(int32 NewEpisodeId);

	UFUNCTION(BlueprintCallable, Category = "Drone|Telemetry")
	bool SaveTelemetryToCsv(const FString& FileName);

	UFUNCTION(BlueprintPure, Category = "Drone|Telemetry")
	const TArray<FDroneTelemetryFrame>& GetFrames() const { return Frames; }

private:
	FString MakeCsvHeader() const;
	FString MakeCsvLine(const FDroneTelemetryFrame& Frame) const;

private:
	UPROPERTY(EditAnywhere, Category = "Drone|Telemetry")
	bool bCaptureTelemetry = true;

	UPROPERTY(EditAnywhere, Category = "Drone|Telemetry")
	float CaptureInterval = 0.2f;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Telemetry")
	int32 EpisodeId = 0;

	UPROPERTY()
	float LastCaptureTime = -1000.f;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Telemetry")
	TArray<FDroneTelemetryFrame> Frames;
};