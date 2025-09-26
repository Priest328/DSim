// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DSimBlueprintFunctionLibrary.generated.h"

class UDSimDebugComponent;
/**
 * 
 */
UCLASS()
class DSIM_API UDSimBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure)
	static UDSimDebugComponent* GetDebugComponent(const UObject* WorldContextObj);

	UFUNCTION()
	static void DrawMovementPathInTheLevel(const UObject* WorldContextObj,
	                                       const TArray<FVector>& LocationPoints,
	                                       FColor ColorToDrawPath, float SphereRadius,
	                                       int32 SphereSegments);
};
