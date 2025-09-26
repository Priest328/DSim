// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DroneTargetInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UDroneTargetInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class DSIM_API IDroneTargetInterface
{
	GENERATED_BODY()

public:
	virtual void GetOverlappedDamage(float DamageAmount);
};
