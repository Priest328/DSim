#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DSimReinforcementLearningComp.generated.h"

class ADSimCharacterAIController;
class ADSimCharacter;

// Дія
UENUM(BlueprintType)
enum class EBotAction : uint8
{
	None UMETA(DisplayName = "None"),
	TowardGoal UMETA(DisplayName = "Toward Goal"),
	TowardCover UMETA(DisplayName = "Toward Cover"),
	RandomMove UMETA(DisplayName = "Random Move"),
};

USTRUCT(BlueprintType)
struct FDSimBotActionData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBotAction BotAction = EBotAction::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float QValue = 0.0f; // Q-value для цієї дії
};

USTRUCT(BlueprintType)
struct FDSimRLSectionData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SectionKey = 0.f; // Ключ секції (наприклад, частка пройденого шляху)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimBotActionData> Actions;

	// Пошук дії за типом
	FDSimBotActionData* FindAction(EBotAction Action)
	{
		for (auto& A : Actions)
			if (A.BotAction == Action)
				return &A;
		return nullptr;
	}
};

USTRUCT(BlueprintType)
struct FEpisodeStep
{
	GENERATED_BODY()
	UPROPERTY()
	float SectionKey = 0.f;
	UPROPERTY()
	EBotAction Action = EBotAction::None;
	UPROPERTY()
	float Reward = 0.f;
};

USTRUCT(BlueprintType)
struct FDSimRLData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDSimRLSectionData> AllSections;

	FDSimRLSectionData* FindSection(float SectionKey)
	{
		for (auto& S : AllSections)
			if (FMath::IsNearlyEqual(S.SectionKey, SectionKey, KINDA_SMALL_NUMBER))
				return &S;
		return nullptr;
	}
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DSIM_API UDSimReinforcementLearningComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UDSimReinforcementLearningComp();

	// RL API
	void InitComponentData();
	EBotAction RequestAction();
	void ApplyReward(float Reward, bool bEpisodeEnd);

	// Save/load
	UFUNCTION(BlueprintCallable)
	bool SaveRLDataToFile();
	UFUNCTION(BlueprintCallable)
	bool LoadRLDataFromFile();

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// RL core
	void OnEpisodeEnd();
	void DecayEpsilon();

	// Helpers
	void StartNewEpisode();
	void LogRLDebug(const FString& Msg);

private:
	UPROPERTY()
	TObjectPtr<ADSimCharacter> OwnerActor;
	UPROPERTY()
	TObjectPtr<ADSimCharacterAIController> AIController;

	UPROPERTY()
	FDSimRLData RLData;

	UPROPERTY()
	TArray<FEpisodeStep> CurrentEpisode;

	TArray<TArray<FEpisodeStep>> ReplayBuffer;

	UPROPERTY(EditAnywhere, Category = "RL")
	int32 ReplayBufferSize = 100;
	UPROPERTY(EditAnywhere, Category = "RL")
	float Gamma = 0.95f;
	UPROPERTY(EditAnywhere, Category = "RL")
	float Alpha = 0.3f;
	UPROPERTY(EditAnywhere, Category = "RL")
	float Epsilon = 0.2f;
	UPROPERTY(EditAnywhere, Category = "RL")
	float EpsilonMin = 0.05f;
	UPROPERTY(EditAnywhere, Category = "RL")
	float EpsilonDecay = 0.99f;

	// Section parameters
	UPROPERTY(EditAnywhere, Category = "RL")
	int32 NumSections = 100;
	UPROPERTY(EditAnywhere, Category = "RL")
	float CriticalDroneDistance = 900.f;

	FVector StartPosition;
	FVector GoalPosition;
	float PathLength = 1.f;

	float LastSectionKey = 0.f;
	EBotAction LastAction = EBotAction::None;

	// RL file
	FString GetRLDataSavePath() const;
};