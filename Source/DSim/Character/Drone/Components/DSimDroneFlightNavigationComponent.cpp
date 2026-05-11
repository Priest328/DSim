#include "DSim/Character/Drone/Components/DSimDroneFlightNavigationComponent.h"

UDSimDroneFlightNavigationComponent::UDSimDroneFlightNavigationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UDSimDroneFlightNavigationComponent::FindSafeFlightTarget(const FVector& DesiredTarget, FVector& OutSafeTarget)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return false;
	}

	const FVector From = Owner->GetActorLocation();
	const FVector ClampedDesiredTarget = ClampAltitude(DesiredTarget);

	if (HasClearPath(From, ClampedDesiredTarget))
	{
		OutSafeTarget = ClampedDesiredTarget;
		LastSafeTarget = OutSafeTarget;
		return true;
	}

	const TArray<FDroneFlightCandidate> Candidates = GenerateAvoidanceCandidates(From, ClampedDesiredTarget);

	bool bFound = false;
	float BestScore = -FLT_MAX;
	FVector BestLocation = ClampedDesiredTarget;

	for (const FDroneFlightCandidate& Candidate : Candidates)
	{
		if (!Candidate.bHasClearPath)
		{
			continue;
		}

		if (Candidate.Score > BestScore)
		{
			BestScore = Candidate.Score;
			BestLocation = Candidate.Location;
			bFound = true;
		}
	}

	OutSafeTarget = BestLocation;
	LastSafeTarget = OutSafeTarget;
	return bFound;
}

bool UDSimDroneFlightNavigationComponent::HasClearPath(const FVector& From, const FVector& To) const
{
	if (!GetWorld())
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());

	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		From,
		To,
		FQuat::Identity,
		ObstacleTraceChannel,
		FCollisionShape::MakeSphere(DroneRadius),
		Params
	);

	return !bHit;
}

TArray<FDroneFlightCandidate> UDSimDroneFlightNavigationComponent::GenerateAvoidanceCandidates(
	const FVector& From,
	const FVector& DesiredTarget
) const
{
	TArray<FDroneFlightCandidate> Candidates;

	const FVector Direction = (DesiredTarget - From).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();

	TArray<FVector> RawLocations;
	RawLocations.Add(DesiredTarget + Right * AvoidanceSideOffset);
	RawLocations.Add(DesiredTarget - Right * AvoidanceSideOffset);
	RawLocations.Add(DesiredTarget + FVector::UpVector * AvoidanceVerticalOffset);
	RawLocations.Add(DesiredTarget + Right * AvoidanceSideOffset + FVector::UpVector * AvoidanceVerticalOffset);
	RawLocations.Add(DesiredTarget - Right * AvoidanceSideOffset + FVector::UpVector * AvoidanceVerticalOffset);

	for (const FVector& RawLocation : RawLocations)
	{
		FDroneFlightCandidate Candidate;
		Candidate.Location = ClampAltitude(RawLocation);
		Candidate.bHasClearPath = HasClearPath(From, Candidate.Location);
		Candidate.Score = ScoreCandidate(From, Candidate.Location, DesiredTarget);
		Candidates.Add(Candidate);
	}

	return Candidates;
}

float UDSimDroneFlightNavigationComponent::ScoreCandidate(
	const FVector& From,
	const FVector& CandidateLocation,
	const FVector& DesiredTarget
) const
{
	const float DistanceToDesired = FVector::Dist(CandidateLocation, DesiredTarget);
	const float DistanceFromCurrent = FVector::Dist(From, CandidateLocation);
	const float AltitudePenalty = FMath::Abs(CandidateLocation.Z - DesiredTarget.Z);

	float Score = 0.f;
	Score -= DistanceToDesired * 1.0f;
	Score -= DistanceFromCurrent * 0.25f;
	Score -= AltitudePenalty * 0.15f;

	if (HasClearPath(From, CandidateLocation))
	{
		Score += 10000.f;
	}

	return Score;
}

FVector UDSimDroneFlightNavigationComponent::ClampAltitude(const FVector& Location) const
{
	FVector Result = Location;
	Result.Z = FMath::Clamp(Result.Z, MinFlightZ, MaxFlightZ);
	return Result;
}

bool UDSimDroneFlightNavigationComponent::FindAttackDiveTarget(
	AActor* TargetActor,
	FVector& OutImpactTarget,
	FHitResult& OutGroundHit
) const
{
	AActor* Owner = GetOwner();

	if (!IsValid(Owner) || !IsValid(TargetActor) || !GetWorld())
	{
		return false;
	}

	const FVector TargetLocation = TargetActor->GetActorLocation();

	const FVector GroundTraceStart = TargetLocation + FVector::UpVector * AttackGroundTraceHeight;
	const FVector GroundTraceEnd = TargetLocation - FVector::UpVector * AttackGroundTraceDepth;

	FCollisionQueryParams GroundParams;
	GroundParams.AddIgnoredActor(Owner);
	GroundParams.AddIgnoredActor(TargetActor);

	const bool bGroundHit = GetWorld()->LineTraceSingleByChannel(
		OutGroundHit,
		GroundTraceStart,
		GroundTraceEnd,
		AttackGroundTraceChannel,
		GroundParams
	);

	if (bGroundHit)
	{
		OutImpactTarget = OutGroundHit.ImpactPoint + FVector::UpVector * AttackImpactHeightOffset;
		return true;
	}

	// Fallback: якщо землю не знайшли, летимо в позицію бота.
	OutImpactTarget = TargetLocation;
	return true;
}

bool UDSimDroneFlightNavigationComponent::IsAttackDivePathClear(
	const FVector& ImpactTarget,
	AActor* AllowedTargetActor,
	FHitResult& OutBlockingHit
) const
{
	AActor* Owner = GetOwner();

	if (!IsValid(Owner) || !GetWorld())
	{
		return false;
	}

	const FVector From = Owner->GetActorLocation();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	const bool bHit = GetWorld()->SweepSingleByChannel(
		OutBlockingHit,
		From,
		ImpactTarget,
		FQuat::Identity,
		AttackDiveTraceChannel,
		FCollisionShape::MakeSphere(AttackDivePathRadius),
		Params
	);

	if (!bHit)
	{
		return true;
	}

	AActor* HitActor = OutBlockingHit.GetActor();

	// Якщо першим на шляху є сам бот — це валідний шлях атаки.
	if (IsValid(HitActor) && HitActor == AllowedTargetActor)
	{
		return true;
	}

	// Якщо перешкода дуже близько до кінцевої точки — це, швидше за все, земля в точці удару.
	const float DistanceFromHitToImpact = FVector::Dist(OutBlockingHit.ImpactPoint, ImpactTarget);

	if (DistanceFromHitToImpact <= AttackTerminalHitTolerance)
	{
		return true;
	}

	// Інакше це дерево, стіна, камінь або інша перешкода на траєкторії піке.
	return false;
}
