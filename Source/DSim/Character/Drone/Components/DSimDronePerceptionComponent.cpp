#include "DSim/Character/Drone/Components/DSimDronePerceptionComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/PawnMovementComponent.h"

UDSimDronePerceptionComponent::UDSimDronePerceptionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDSimDronePerceptionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UDSimDronePerceptionComponent::SetTargetBot(AActor* InTargetBot)
{
	TargetBotActor = InTargetBot;
}

void UDSimDronePerceptionComponent::UpdatePerception()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !IsValid(TargetBotActor))
	{
		return;
	}

	const FVector DroneLocation = Owner->GetActorLocation();
	const FVector TargetLocation = TargetBotActor->GetActorLocation();

	Snapshot.DroneLocation = DroneLocation;
	Snapshot.TargetLocation = TargetLocation;
	Snapshot.DistanceToTarget = FVector::Dist(DroneLocation, TargetLocation);

	if (const APawn* TargetPawn = Cast<APawn>(TargetBotActor))
	{
		if (const UPawnMovementComponent* MovementComp = TargetPawn->GetMovementComponent())
		{
			Snapshot.TargetVelocity = MovementComp->Velocity;
		}
		else
		{
			Snapshot.TargetVelocity = TargetPawn->GetVelocity();
		}
	}
	else
	{
		Snapshot.TargetVelocity = TargetBotActor->GetVelocity();
	}

	Snapshot.bHasLineOfSight = CalculateLineOfSight(DroneLocation, TargetLocation);
	if (Snapshot.bHasLineOfSight)
	{
		Snapshot.LastKnownTargetLocation = TargetLocation;
	}

	Snapshot.bObstacleAhead = CalculateObstacleAhead(DroneLocation, Owner->GetActorForwardVector());
	Snapshot.GroundDistance = CalculateGroundDistance(DroneLocation);
	Snapshot.CurrentAltitude = Snapshot.GroundDistance;
}

bool UDSimDronePerceptionComponent::CalculateLineOfSight(const FVector& From, const FVector& To) const
{
	if (!GetWorld() || !IsValid(TargetBotActor))
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		From,
		To,
		VisibilityTraceChannel,
		Params
	);

	if (!bHit)
	{
		return true;
	}

	return Hit.GetActor() == TargetBotActor;
}

bool UDSimDronePerceptionComponent::CalculateObstacleAhead(const FVector& From, const FVector& Forward) const
{
	if (!GetWorld())
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	Params.AddIgnoredActor(TargetBotActor);

	const FVector End = From + Forward.GetSafeNormal() * ObstacleCheckDistance;

	return GetWorld()->SweepSingleByChannel(
		Hit,
		From,
		End,
		FQuat::Identity,
		ObstacleTraceChannel,
		FCollisionShape::MakeSphere(ObstacleCheckRadius),
		Params
	);
}

float UDSimDronePerceptionComponent::CalculateGroundDistance(const FVector& From) const
{
	if (!GetWorld())
	{
		return 0.f;
	}

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	Params.AddIgnoredActor(TargetBotActor);

	const FVector End = From - FVector::UpVector * GroundCheckDistance;

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		From,
		End,
		ECC_WorldStatic,
		Params
	);

	return bHit ? FVector::Dist(From, Hit.ImpactPoint) : GroundCheckDistance;
}