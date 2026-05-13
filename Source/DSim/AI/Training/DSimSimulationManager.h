#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DSim/AI/Training/DSimTrainingTypes.h"
#include "DSim/Character/Drone/DSimDronePawn.h"
#include "DSimSimulationManager.generated.h"

class ADSimCharacter;
class UDSimBotTrainingAlgorithmComponent;
class UDSimBotTrainingAlgorithmConfig;

UENUM(BlueprintType)
enum class EDSimExperimentState : uint8
{
	Idle     UMETA(DisplayName = "Idle"),
	Running  UMETA(DisplayName = "Running"),
	Finished UMETA(DisplayName = "Finished")
};

USTRUCT(BlueprintType)
struct FDSimActorSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY()
	FTransform InitialTransform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct FDSimExperimentPairConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pair")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pair")
	int32 ArenaId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pair")
	TObjectPtr<ADSimDronePawn> Drone = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pair")
	TObjectPtr<ADSimCharacter> Bot = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment", meta = (ClampMin = "1"))
	int32 EpisodesCount = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment", meta = (ClampMin = "1.0"))
	float EpisodeTimeoutSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment", meta = (ClampMin = "0.0"))
	float DelayBeforeNextEpisode = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone")
	EDroneControlMode DroneMode = EDroneControlMode::SimplePursuit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Algorithm")
	TSubclassOf<UDSimBotTrainingAlgorithmComponent> AlgorithmClass;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Algorithm")
	TObjectPtr<UDSimBotTrainingAlgorithmConfig> AlgorithmConfig = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Algorithm")
	EDSimStateRepresentationMode StateRepresentationMode = EDSimStateRepresentationMode::TwoD;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Files")
	FString InitialTrainingFile = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Files")
	FString OutputName = TEXT("experiment_pair");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Files")
	bool bLoadInitialTrainingFileOnExperimentStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Files")
	bool bSaveTrainingDataAfterEachEpisode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random")
	int32 RandomSeed = 25;
};

USTRUCT()
struct FDSimExperimentPairRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEpisodeRunning = false;

	UPROPERTY()
	bool bPairFinished = false;

	UPROPERTY()
	int32 CompletedEpisodes = 0;

	UPROPERTY()
	int32 CurrentEpisodeId = 0;

	UPROPERTY()
	FDSimActorSnapshot DroneSnapshot;

	UPROPERTY()
	FDSimActorSnapshot BotSnapshot;

	UPROPERTY()
	TObjectPtr<UDSimBotTrainingAlgorithmComponent> ActiveAlgorithm = nullptr;

	FTimerHandle TimeoutTimerHandle;
	FTimerHandle NextEpisodeTimerHandle;
};

UCLASS()
class DSIM_API ADSimSimulationManager : public AActor
{
	GENERATED_BODY()

public:
	ADSimSimulationManager();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	UFUNCTION(BlueprintCallable, Category = "DSim|Experiment")
	void StartExperiment();

	UFUNCTION(BlueprintCallable, Category = "DSim|Experiment")
	void StopExperiment();

	UFUNCTION(BlueprintCallable, Category = "DSim|Experiment")
	void StartEpisodeForPair(int32 PairIndex);

	UFUNCTION(BlueprintCallable, Category = "DSim|Experiment")
	void FinishEpisodeForPair(int32 PairIndex, EDSimEpisodeFinishReason FinishReason);

	UFUNCTION(BlueprintCallable, Category = "DSim|Events")
	void NotifyBotReachedGoal(AActor* BotActor);

	UFUNCTION(BlueprintCallable, Category = "DSim|Events")
	void NotifyBotKilled(AActor* BotActor);

	UFUNCTION(BlueprintCallable, Category = "DSim|Events")
	void NotifyDroneCrashed(AActor* DroneActor);

	UFUNCTION(BlueprintPure, Category = "DSim|Experiment")
	bool IsExperimentRunning() const { return ExperimentState == EDSimExperimentState::Running; }

private:
	void BuildRuntimes();
	void SaveInitialSnapshots();
	void SaveSnapshot(FDSimActorSnapshot& Snapshot, AActor* Actor) const;
	void ResetActorFromSnapshot(const FDSimActorSnapshot& Snapshot) const;

	UDSimBotTrainingAlgorithmComponent* EnsureAlgorithmForPair(int32 PairIndex);
	void ConfigureDroneForPair(int32 PairIndex);
	void ConfigureAlgorithmForPair(int32 PairIndex);

	void HandleEpisodeTimeout(int32 PairIndex);
	void ScheduleNextEpisodeForPair(int32 PairIndex);
	void SavePairOutputs(int32 PairIndex, EDSimEpisodeFinishReason FinishReason);

	int32 FindPairIndexByBot(AActor* BotActor) const;
	int32 FindPairIndexByDrone(AActor* DroneActor) const;
	bool AreAllPairsFinished() const;

	FString BuildRunDirectory() const;
	FString BuildPairDirectory(const FDSimExperimentPairConfig& Config) const;
	FString BuildTrainingDataFileName(const FDSimExperimentPairConfig& Config, int32 EpisodeId) const;
	FString BuildTelemetryFileName(const FDSimExperimentPairConfig& Config, int32 EpisodeId) const;

	void DrawDebugInfo() const;

	void StartPairLogic(int32 PairIndex);
	void StopPairLogic(int32 PairIndex);

private:
	void AppendEpisodeSummary(int32 PairIndex, EDSimEpisodeFinishReason FinishReason);
	FString BuildEpisodeSummaryFileName(const FDSimExperimentPairConfig& Config) const;

private:
	UPROPERTY(EditAnywhere, Category = "DSim|Experiment")
	bool bStartOnBeginPlay = false;

	UPROPERTY(EditAnywhere, Category = "DSim|Experiment")
	int32 RunId = 1;

	UPROPERTY(EditAnywhere, Category = "DSim|Experiment")
	TArray<FDSimExperimentPairConfig> PairConfigs;

	UPROPERTY(EditAnywhere, Category = "DSim|Debug")
	bool bDrawDebugInfo = true;

	UPROPERTY(EditAnywhere, Category = "DSim|Debug")
	float DebugTextDuration = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "DSim|Runtime")
	EDSimExperimentState ExperimentState = EDSimExperimentState::Idle;

	UPROPERTY()
	TArray<FDSimExperimentPairRuntime> PairRuntimes;
};