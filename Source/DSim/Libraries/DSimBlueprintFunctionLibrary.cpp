// Fill out your copyright notice in the Description page of Project Settings.


#include "DSim/Libraries/DSimBlueprintFunctionLibrary.h"

#include "DSim/AI/Training/DSimSimulationManager.h"
#include "DSim/Player/DSimDroneController.h"
#include "Kismet/GameplayStatics.h"

UDSimDebugComponent* UDSimBlueprintFunctionLibrary::GetDebugComponent(const UObject* WorldContextObj)
{
	ADSimDroneController* Controller = Cast<ADSimDroneController>(
		UGameplayStatics::GetPlayerController(WorldContextObj, 0));
	if (!IsValid(Controller))
	{
		return nullptr;
	}

	return Controller->DebugComponent;
}

void UDSimBlueprintFunctionLibrary::DrawMovementPathInTheLevel(const UObject* WorldContextObj,
                                                               const TArray<FVector>& LocationPoints,
                                                               FColor ColorToDrawPath, float SphereRadius,
                                                               int32 SphereSegments)
{
	if (!IsValid(WorldContextObj))
	{
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObj);
	if (!World)
	{
		return;
	}

	for (const FVector& Point : LocationPoints)
	{
		FVector AdjustedPoint = FVector(Point.X, Point.Y - 700, Point.Z);
		DrawDebugSphere(
			World,
			AdjustedPoint,
			SphereRadius,
			SphereSegments,
			ColorToDrawPath,
			true,
			90.0f,
			0,
			1.0f
		);
	}
}

ADSimSimulationManager* UDSimBlueprintFunctionLibrary::GetSimulationManager(UObject* WorldContextObj)
{
	if (!IsValid(WorldContextObj))
	{
		return nullptr;
	}
	
	return Cast<ADSimSimulationManager>(
	UGameplayStatics::GetActorOfClass(WorldContextObj->GetWorld(), ADSimSimulationManager::StaticClass()));
}
