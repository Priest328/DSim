#include "DSim/AI/Training/DSimSimulationManager.h"

#include "DSim/AI/Training/DSimBotTrainingAlgorithmComponent.h"
#include "DSim/Character/DSimCharacter.h"
#include "DSim/Character/Drone/Components/DSimDroneTelemetryComponent.h"
#include "GameFramework/MovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"

ADSimSimulationManager::ADSimSimulationManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADSimSimulationManager::BeginPlay()
{
	Super::BeginPlay();

	BuildRuntimes();
	SaveInitialSnapshots();

	if (bStartOnBeginPlay)
	{
		StartExperiment();
	}
}

void ADSimSimulationManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDrawDebugInfo)
	{
		DrawDebugInfo();
	}
}

void ADSimSimulationManager::StartExperiment()
{
	if (ExperimentState == EDSimExperimentState::Running)
	{
		return;
	}

	BuildRuntimes();
	SaveInitialSnapshots();

	ExperimentState = EDSimExperimentState::Running;

	for (int32 PairIndex = 0; PairIndex < PairConfigs.Num(); ++PairIndex)
	{
		if (!PairConfigs[PairIndex].bEnabled)
		{
			if (PairRuntimes.IsValidIndex(PairIndex))
			{
				PairRuntimes[PairIndex].bPairFinished = true;
			}
			continue;
		}

		EnsureAlgorithmForPair(PairIndex);
		ConfigureAlgorithmForPair(PairIndex);
		StartEpisodeForPair(PairIndex);
	}
}

void ADSimSimulationManager::StopExperiment()
{
	for (FDSimExperimentPairRuntime& Runtime : PairRuntimes)
	{
		GetWorldTimerManager().ClearTimer(Runtime.TimeoutTimerHandle);
		GetWorldTimerManager().ClearTimer(Runtime.NextEpisodeTimerHandle);
		Runtime.bEpisodeRunning = false;
		Runtime.bPairFinished = true;
	}

	ExperimentState = EDSimExperimentState::Finished;
}

void ADSimSimulationManager::StartEpisodeForPair(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return;
	}

	FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

	if (!Config.bEnabled || Runtime.bPairFinished)
	{
		return;
	}

	if (Runtime.CompletedEpisodes >= Config.EpisodesCount)
	{
		Runtime.bPairFinished = true;

		if (AreAllPairsFinished())
		{
			StopExperiment();
		}

		return;
	}

	Runtime.CurrentEpisodeId = Runtime.CompletedEpisodes + 1;
	Runtime.bEpisodeRunning = true;

	ResetActorFromSnapshot(Runtime.DroneSnapshot);
	ResetActorFromSnapshot(Runtime.BotSnapshot);

	ConfigureDroneForPair(PairIndex);

	if (Runtime.ActiveAlgorithm)
	{
		Runtime.ActiveAlgorithm->StartEpisode(Runtime.CurrentEpisodeId);
	}

	GetWorldTimerManager().ClearTimer(Runtime.TimeoutTimerHandle);

	FTimerDelegate TimeoutDelegate;
	TimeoutDelegate.BindUObject(this, &ADSimSimulationManager::HandleEpisodeTimeout, PairIndex);

	GetWorldTimerManager().SetTimer(
		Runtime.TimeoutTimerHandle,
		TimeoutDelegate,
		Config.EpisodeTimeoutSeconds,
		false
	);
}

void ADSimSimulationManager::FinishEpisodeForPair(int32 PairIndex, EDSimEpisodeFinishReason FinishReason)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return;
	}

	FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

	if (!Runtime.bEpisodeRunning)
	{
		return;
	}

	Runtime.bEpisodeRunning = false;

	GetWorldTimerManager().ClearTimer(Runtime.TimeoutTimerHandle);

	if (IsValid(Config.Drone))
	{
		Config.Drone->StopAutopilotLogic();
	}

	if (Runtime.ActiveAlgorithm)
	{
		Runtime.ActiveAlgorithm->EndEpisode(FinishReason);
	}

	SavePairOutputs(PairIndex, FinishReason);

	Runtime.CompletedEpisodes++;

	if (Runtime.CompletedEpisodes >= Config.EpisodesCount)
	{
		Runtime.bPairFinished = true;

		if (AreAllPairsFinished())
		{
			StopExperiment();
		}

		return;
	}

	ScheduleNextEpisodeForPair(PairIndex);
}

void ADSimSimulationManager::NotifyBotReachedGoal(AActor* BotActor)
{
	const int32 PairIndex = FindPairIndexByBot(BotActor);
	FinishEpisodeForPair(PairIndex, EDSimEpisodeFinishReason::GoalReached);
}

void ADSimSimulationManager::NotifyBotKilled(AActor* BotActor)
{
	const int32 PairIndex = FindPairIndexByBot(BotActor);
	FinishEpisodeForPair(PairIndex, EDSimEpisodeFinishReason::BotKilledByDrone);
}

void ADSimSimulationManager::NotifyDroneCrashed(AActor* DroneActor)
{
	const int32 PairIndex = FindPairIndexByDrone(DroneActor);
	FinishEpisodeForPair(PairIndex, EDSimEpisodeFinishReason::DroneCrashed);
}

void ADSimSimulationManager::BuildRuntimes()
{
	PairRuntimes.SetNum(PairConfigs.Num());

	for (FDSimExperimentPairRuntime& Runtime : PairRuntimes)
	{
		Runtime.bEpisodeRunning = false;
		Runtime.bPairFinished = false;
		Runtime.CompletedEpisodes = 0;
		Runtime.CurrentEpisodeId = 0;
		Runtime.ActiveAlgorithm = nullptr;
	}
}

void ADSimSimulationManager::SaveInitialSnapshots()
{
	for (int32 PairIndex = 0; PairIndex < PairConfigs.Num(); ++PairIndex)
	{
		if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
		{
			continue;
		}

		const FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
		FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

		if (!Config.bEnabled)
		{
			continue;
		}

		SaveSnapshot(Runtime.DroneSnapshot, Config.Drone);
		SaveSnapshot(Runtime.BotSnapshot, Config.Bot);
	}
}

void ADSimSimulationManager::SaveSnapshot(FDSimActorSnapshot& Snapshot, AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return;
	}

	Snapshot.Actor = Actor;
	Snapshot.InitialTransform = Actor->GetActorTransform();
}

void ADSimSimulationManager::ResetActorFromSnapshot(const FDSimActorSnapshot& Snapshot) const
{
	if (!IsValid(Snapshot.Actor))
	{
		return;
	}

	Snapshot.Actor->SetActorTransform(
		Snapshot.InitialTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Snapshot.Actor->GetRootComponent()))
	{
		RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
		RootPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	if (UMovementComponent* MovementComponent = Snapshot.Actor->FindComponentByClass<UMovementComponent>())
	{
		MovementComponent->StopMovementImmediately();
	}
}

UDSimBotTrainingAlgorithmComponent* ADSimSimulationManager::EnsureAlgorithmForPair(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return nullptr;
	}

	FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

	if (!IsValid(Config.Bot))
	{
		return nullptr;
	}

	UDSimBotTrainingAlgorithmComponent* Existing =
		Config.Bot->FindComponentByClass<UDSimBotTrainingAlgorithmComponent>();

	if (IsValid(Existing))
	{
		if (!Config.AlgorithmClass || Existing->GetClass() == Config.AlgorithmClass)
		{
			Runtime.ActiveAlgorithm = Existing;
			return Existing;
		}

		Existing->DestroyComponent();
	}

	if (!Config.AlgorithmClass)
	{
		return nullptr;
	}

	UDSimBotTrainingAlgorithmComponent* NewAlgorithm =
		NewObject<UDSimBotTrainingAlgorithmComponent>(Config.Bot, Config.AlgorithmClass);

	if (!IsValid(NewAlgorithm))
	{
		return nullptr;
	}

	NewAlgorithm->RegisterComponent();
	Runtime.ActiveAlgorithm = NewAlgorithm;

	return NewAlgorithm;
}

void ADSimSimulationManager::ConfigureDroneForPair(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex))
	{
		return;
	}

	const FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];

	if (!IsValid(Config.Drone) || !IsValid(Config.Bot))
	{
		return;
	}

	Config.Drone->StopAutopilotLogic();
	Config.Drone->ResetAutopilotRuntime();

	Config.Drone->SetArenaId(Config.ArenaId);
	Config.Drone->SetTargetBot(Config.Bot);
	Config.Drone->SetDroneControlMode(Config.DroneMode);

	if (UDSimDroneTelemetryComponent* Telemetry =
		Config.Drone->FindComponentByClass<UDSimDroneTelemetryComponent>())
	{
		const int32 EpisodeId = PairRuntimes.IsValidIndex(PairIndex)
			? PairRuntimes[PairIndex].CurrentEpisodeId
			: 0;

		Telemetry->ClearFrames();
		Telemetry->SetEpisodeId(EpisodeId);
	}

	Config.Drone->StartAutopilotLogic();
}

void ADSimSimulationManager::ConfigureAlgorithmForPair(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return;
	}

	FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

	if (!Runtime.ActiveAlgorithm)
	{
		return;
	}

	FDSimAlgorithmRuntimeContext Context;
	Context.ArenaId = Config.ArenaId;
	Context.RunId = RunId;
	Context.StateRepresentationMode = Config.StateRepresentationMode;

	Context.InitialTrainingFile = Config.bLoadInitialTrainingFileOnExperimentStart
		? Config.InitialTrainingFile
		: TEXT("");

	Context.OutputDirectory = BuildPairDirectory(Config);
	Context.OutputTrainingFile = BuildTrainingDataFileName(Config, 0);

	Runtime.ActiveAlgorithm->InitializeAlgorithm(Context, Config.AlgorithmConfig);
}

void ADSimSimulationManager::HandleEpisodeTimeout(int32 PairIndex)
{
	FinishEpisodeForPair(PairIndex, EDSimEpisodeFinishReason::Timeout);
}

void ADSimSimulationManager::ScheduleNextEpisodeForPair(int32 PairIndex)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return;
	}

	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];
	const FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];

	FTimerDelegate NextEpisodeDelegate;
	NextEpisodeDelegate.BindUObject(this, &ADSimSimulationManager::StartEpisodeForPair, PairIndex);

	GetWorldTimerManager().SetTimer(
		Runtime.NextEpisodeTimerHandle,
		NextEpisodeDelegate,
		Config.DelayBeforeNextEpisode,
		false
	);
}

void ADSimSimulationManager::SavePairOutputs(int32 PairIndex, EDSimEpisodeFinishReason FinishReason)
{
	if (!PairConfigs.IsValidIndex(PairIndex) || !PairRuntimes.IsValidIndex(PairIndex))
	{
		return;
	}

	const FDSimExperimentPairConfig& Config = PairConfigs[PairIndex];
	FDSimExperimentPairRuntime& Runtime = PairRuntimes[PairIndex];

	if (IsValid(Config.Drone))
	{
		if (UDSimDroneTelemetryComponent* Telemetry =
			Config.Drone->FindComponentByClass<UDSimDroneTelemetryComponent>())
		{
			Telemetry->SaveTelemetryToCsv(
				BuildTelemetryFileName(Config, Runtime.CurrentEpisodeId)
			);
		}
	}

	if (Config.bSaveTrainingDataAfterEachEpisode && Runtime.ActiveAlgorithm)
	{
		Runtime.ActiveAlgorithm->SaveTrainingData(
			BuildTrainingDataFileName(Config, Runtime.CurrentEpisodeId)
		);
	}
}

int32 ADSimSimulationManager::FindPairIndexByBot(AActor* BotActor) const
{
	for (int32 i = 0; i < PairConfigs.Num(); ++i)
	{
		if (PairConfigs[i].Bot == BotActor)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

int32 ADSimSimulationManager::FindPairIndexByDrone(AActor* DroneActor) const
{
	for (int32 i = 0; i < PairConfigs.Num(); ++i)
	{
		if (PairConfigs[i].Drone == DroneActor)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

bool ADSimSimulationManager::AreAllPairsFinished() const
{
	for (int32 i = 0; i < PairConfigs.Num(); ++i)
	{
		if (!PairConfigs[i].bEnabled)
		{
			continue;
		}

		if (!PairRuntimes.IsValidIndex(i) || !PairRuntimes[i].bPairFinished)
		{
			return false;
		}
	}

	return true;
}

FString ADSimSimulationManager::BuildRunDirectory() const
{
	return FPaths::ProjectSavedDir() / TEXT("Experiments") /
		FString::Printf(TEXT("Run_%03d"), RunId);
}

FString ADSimSimulationManager::BuildPairDirectory(const FDSimExperimentPairConfig& Config) const
{
	return BuildRunDirectory() /
		FString::Printf(TEXT("Arena_%02d_%s"), Config.ArenaId, *Config.OutputName);
}

FString ADSimSimulationManager::BuildTrainingDataFileName(
	const FDSimExperimentPairConfig& Config,
	int32 EpisodeId
) const
{
	const FString Directory = BuildPairDirectory(Config);
	IFileManager::Get().MakeDirectory(*Directory, true);

	if (EpisodeId <= 0)
	{
		return Directory / TEXT("training_final.json");
	}

	return Directory / FString::Printf(TEXT("training_ep_%03d.json"), EpisodeId);
}

FString ADSimSimulationManager::BuildTelemetryFileName(
	const FDSimExperimentPairConfig& Config,
	int32 EpisodeId
) const
{
	const FString Directory = BuildPairDirectory(Config);
	IFileManager::Get().MakeDirectory(*Directory, true);

	return FString::Printf(
		TEXT("telemetry_ep_%03d.csv"),
		EpisodeId
	);
}

void ADSimSimulationManager::DrawDebugInfo() const
{
	if (!GEngine)
	{
		return;
	}

	int32 RunningPairs = 0;
	int32 FinishedPairs = 0;
	int32 CompletedEpisodes = 0;
	int32 TargetEpisodes = 0;

	for (int32 i = 0; i < PairConfigs.Num(); ++i)
	{
		if (!PairConfigs[i].bEnabled)
		{
			continue;
		}

		TargetEpisodes += PairConfigs[i].EpisodesCount;

		if (PairRuntimes.IsValidIndex(i))
		{
			CompletedEpisodes += PairRuntimes[i].CompletedEpisodes;

			if (PairRuntimes[i].bEpisodeRunning)
			{
				RunningPairs++;
			}

			if (PairRuntimes[i].bPairFinished)
			{
				FinishedPairs++;
			}
		}
	}

	const FString Text = FString::Printf(
		TEXT("Experiment: %s | Run: %03d | Episodes: %d / %d | Running pairs: %d | Finished pairs: %d"),
		*UEnum::GetValueAsString(ExperimentState),
		RunId,
		CompletedEpisodes,
		TargetEpisodes,
		RunningPairs,
		FinishedPairs
	);

	GEngine->AddOnScreenDebugMessage(
		987654,
		DebugTextDuration,
		FColor::Green,
		Text
	);
}