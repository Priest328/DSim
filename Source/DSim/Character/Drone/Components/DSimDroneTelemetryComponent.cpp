#include "DSim/Character/Drone/Components/DSimDroneTelemetryComponent.h"

#include "DSim/Character/Drone/DSimDronePawn.h"
#include "DSim/Character/Drone/Components/DSimDronePerceptionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UDSimDroneTelemetryComponent::UDSimDroneTelemetryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDSimDroneTelemetryComponent::SetEpisodeId(int32 NewEpisodeId)
{
	EpisodeId = NewEpisodeId;
}

void UDSimDroneTelemetryComponent::CaptureFrame()
{
	if (!bCaptureTelemetry || !GetWorld())
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastCaptureTime < CaptureInterval)
	{
		return;
	}

	LastCaptureTime = CurrentTime;

	ADSimDronePawn* DronePawn = Cast<ADSimDronePawn>(GetOwner());
	if (!IsValid(DronePawn))
	{
		return;
	}

	const UDSimDronePerceptionComponent* Perception =
		DronePawn->FindComponentByClass<UDSimDronePerceptionComponent>();

	FDroneTelemetryFrame Frame;
	Frame.EpisodeId = EpisodeId;
	Frame.ArenaId = DronePawn->GetArenaId();
	Frame.TimeSeconds = CurrentTime;
	Frame.DroneLocation = DronePawn->GetActorLocation();
	Frame.DesiredTarget = DronePawn->GetDesiredFlightTarget();
	Frame.SafeTarget = DronePawn->GetSafeFlightTarget();
	Frame.DroneMode = static_cast<uint8>(DronePawn->GetDroneControlMode());
	Frame.DroneState = static_cast<uint8>(DronePawn->GetAutopilotState());

	if (Perception)
	{
		const FDronePerceptionSnapshot& Snapshot = Perception->GetSnapshot();
		Frame.TargetLocation = Snapshot.TargetLocation;
		Frame.DistanceToTarget = Snapshot.DistanceToTarget;
		Frame.bHasLineOfSight = Snapshot.bHasLineOfSight;
		Frame.bObstacleAhead = Snapshot.bObstacleAhead;
	}

	Frames.Add(Frame);
}

void UDSimDroneTelemetryComponent::ClearFrames()
{
	Frames.Reset();
	LastCaptureTime = -1000.f;
}

bool UDSimDroneTelemetryComponent::SaveTelemetryToCsv(const FString& FileName)
{
	FString CsvContent = MakeCsvHeader();

	for (const FDroneTelemetryFrame& Frame : Frames)
	{
		CsvContent += MakeCsvLine(Frame);
	}

	const FString Directory = FPaths::ProjectSavedDir() / TEXT("Experiments");
	IFileManager::Get().MakeDirectory(*Directory, true);

	const FString FullPath = Directory / FileName;

	return FFileHelper::SaveStringToFile(CsvContent, *FullPath);
}

FString UDSimDroneTelemetryComponent::MakeCsvHeader() const
{
	return TEXT("episode_id,arena_id,time,drone_x,drone_y,drone_z,target_x,target_y,target_z,desired_x,desired_y,desired_z,safe_x,safe_y,safe_z,distance_to_target,has_los,obstacle_ahead,drone_mode,drone_state\n");
}

FString UDSimDroneTelemetryComponent::MakeCsvLine(const FDroneTelemetryFrame& Frame) const
{
	return FString::Printf(
		TEXT("%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%d\n"),
		Frame.EpisodeId,
		Frame.ArenaId,
		Frame.TimeSeconds,
		Frame.DroneLocation.X,
		Frame.DroneLocation.Y,
		Frame.DroneLocation.Z,
		Frame.TargetLocation.X,
		Frame.TargetLocation.Y,
		Frame.TargetLocation.Z,
		Frame.DesiredTarget.X,
		Frame.DesiredTarget.Y,
		Frame.DesiredTarget.Z,
		Frame.SafeTarget.X,
		Frame.SafeTarget.Y,
		Frame.SafeTarget.Z,
		Frame.DistanceToTarget,
		Frame.bHasLineOfSight ? 1 : 0,
		Frame.bObstacleAhead ? 1 : 0,
		Frame.DroneMode,
		Frame.DroneState
	);
}