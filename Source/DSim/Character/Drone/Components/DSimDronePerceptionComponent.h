#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DSimDronePerceptionComponent.generated.h"

USTRUCT(BlueprintType)
struct FDronePerceptionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector DroneLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector TargetVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	float DistanceToTarget = 0.f;

	UPROPERTY(BlueprintReadOnly)
	bool bHasLineOfSight = false;

	UPROPERTY(BlueprintReadOnly)
	bool bObstacleAhead = false;

	UPROPERTY(BlueprintReadOnly)
	float CurrentAltitude = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float GroundDistance = 0.f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DSIM_API UDSimDronePerceptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDSimDronePerceptionComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Drone|Perception")
	void SetTargetBot(AActor* InTargetBot);

	UFUNCTION(BlueprintCallable, Category = "Drone|Perception")
	void UpdatePerception();

	UFUNCTION(BlueprintPure, Category = "Drone|Perception")
	const FDronePerceptionSnapshot& GetSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category = "Drone|Perception")
	bool HasLineOfSightToTarget() const { return Snapshot.bHasLineOfSight; }

	UFUNCTION(BlueprintPure, Category = "Drone|Perception")
	bool HasObstacleAhead() const { return Snapshot.bObstacleAhead; }

	UFUNCTION(BlueprintPure, Category = "Drone|Perception")
	float GetDistanceToTarget() const { return Snapshot.DistanceToTarget; }

	UFUNCTION(BlueprintPure, Category = "Drone|Perception")
	FVector GetTargetLocation() const { return Snapshot.TargetLocation; }

	UFUNCTION(BlueprintPure, Category = "Drone|Perception")
	FVector GetTargetVelocity() const { return Snapshot.TargetVelocity; }

	UFUNCTION(BlueprintPure, Category = "Drone|Perception")
	FVector GetLastKnownTargetLocation() const { return Snapshot.LastKnownTargetLocation; }

private:
	bool CalculateLineOfSight(const FVector& From, const FVector& To) const;
	bool CalculateObstacleAhead(const FVector& From, const FVector& Forward) const;
	float CalculateGroundDistance(const FVector& From) const;

private:
	UPROPERTY(EditAnywhere, Category = "Drone|Perception")
	TEnumAsByte<ECollisionChannel> VisibilityTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category = "Drone|Perception")
	TEnumAsByte<ECollisionChannel> ObstacleTraceChannel = ECC_WorldStatic;

	UPROPERTY(EditAnywhere, Category = "Drone|Perception")
	float ObstacleCheckDistance = 700.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Perception")
	float ObstacleCheckRadius = 120.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Perception")
	float GroundCheckDistance = 5000.f;

	UPROPERTY()
	TObjectPtr<AActor> TargetBotActor;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Perception")
	FDronePerceptionSnapshot Snapshot;
};