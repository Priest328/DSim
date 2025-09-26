// Fill out your copyright notice in the Description page of Project Settings.


#include "DSim/AI/Services/BTService_CheckDanger.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTService_CheckDanger::UBTService_CheckDanger()
{
	NodeName = "Check If In Danger";
	Interval = 0.5f;

	TargetKey.SelectedKeyName = "TargetActor";
	IsInDangerKey.SelectedKeyName = "IsInDanger";
}

void UBTService_CheckDanger::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	APawn* ControlledPawn = OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr;
	if (!IsValid(ControlledPawn))
		return;

	UObject* TargetObj = OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetKey.SelectedKeyName);
	AActor* TargetActor = Cast<AActor>(TargetObj);
	if (!IsValid(TargetActor))
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsBool(IsInDangerKey.SelectedKeyName, false);
		return;
	}

	float Distance = FVector::Dist(ControlledPawn->GetActorLocation(), TargetActor->GetActorLocation());
	if (Distance > DangerDistance)
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsBool(IsInDangerKey.SelectedKeyName, false);
		return;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(ControlledPawn);
	Params.AddIgnoredActor(TargetActor);

	FHitResult Hit;
	bool bHit = ControlledPawn->GetWorld()->LineTraceSingleByChannel(
		Hit,
		ControlledPawn->GetActorLocation(),
		TargetActor->GetActorLocation(),
		ECollisionChannel::ECC_Visibility,
		Params);

	bool bCanSee = !bHit;

	OwnerComp.GetBlackboardComponent()->SetValueAsBool(IsInDangerKey.SelectedKeyName, bCanSee);
}
