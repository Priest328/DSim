#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DSimDroneFlightNavigationComponent.generated.h"

USTRUCT(BlueprintType)
struct FDroneFlightCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	float Score = 0.f;

	UPROPERTY(BlueprintReadOnly)
	bool bHasClearPath = false;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DSIM_API UDSimDroneFlightNavigationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDSimDroneFlightNavigationComponent();

	UFUNCTION(BlueprintCallable, Category = "Drone|Flight Navigation")
	bool FindSafeFlightTarget(const FVector& DesiredTarget, FVector& OutSafeTarget);

	UFUNCTION(BlueprintPure, Category = "Drone|Flight Navigation")
	FVector GetLastSafeTarget() const { return LastSafeTarget; }

	UFUNCTION(BlueprintCallable, Category = "Drone|Attack Dive")
	bool FindAttackDiveTarget(AActor* TargetActor, FVector& OutImpactTarget, FHitResult& OutGroundHit) const;

	UFUNCTION(BlueprintCallable, Category = "Drone|Attack Dive")
	bool IsAttackDivePathClear(const FVector& ImpactTarget, AActor* AllowedTargetActor, FHitResult& OutBlockingHit) const;

private:
	bool HasClearPath(const FVector& From, const FVector& To) const;
	TArray<FDroneFlightCandidate> GenerateAvoidanceCandidates(const FVector& From, const FVector& DesiredTarget) const;
	float ScoreCandidate(const FVector& From, const FVector& CandidateLocation, const FVector& DesiredTarget) const;
	FVector ClampAltitude(const FVector& Location) const;

private:
	UPROPERTY(EditAnywhere, Category = "Drone|Flight Navigation")
	TEnumAsByte<ECollisionChannel> ObstacleTraceChannel = ECC_WorldStatic;

	UPROPERTY(EditAnywhere, Category = "Drone|Flight Navigation")
	float DroneRadius = 120.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Flight Navigation")
	float AvoidanceSideOffset = 600.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Flight Navigation")
	float AvoidanceVerticalOffset = 450.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Flight Navigation")
	float MinFlightZ = 250.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Flight Navigation")
	float MaxFlightZ = 2000.f;

	UPROPERTY(VisibleAnywhere, Category = "Drone|Flight Navigation")
	FVector LastSafeTarget = FVector::ZeroVector;
	
private:
	UPROPERTY(EditAnywhere, Category = "Drone|Attack Dive")
	TEnumAsByte<ECollisionChannel> AttackDiveTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category = "Drone|Attack Dive")
	TEnumAsByte<ECollisionChannel> AttackGroundTraceChannel = ECC_WorldStatic;

	UPROPERTY(EditAnywhere, Category = "Drone|Attack Dive")
	float AttackDivePathRadius = 90.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Attack Dive")
	float AttackGroundTraceHeight = 800.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Attack Dive")
	float AttackGroundTraceDepth = 3000.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Attack Dive")
	float AttackImpactHeightOffset = 60.f;

	UPROPERTY(EditAnywhere, Category = "Drone|Attack Dive")
	float AttackTerminalHitTolerance = 220.f;
};