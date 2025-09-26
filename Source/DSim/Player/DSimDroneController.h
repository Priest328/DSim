// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DSimDroneController.generated.h"

class UDSimDebugComponent;
class UDSimReinforcementLearningComp;

UENUM(BlueprintType)
enum class EInGameWidgetType : uint8
{
	None,
	DebugWindow,
	EndGameWindow,
	ShowPathWindow
};

/**
 * 
 */
UCLASS()
class DSIM_API ADSimDroneController : public APlayerController
{
	GENERATED_BODY()

public:
	ADSimDroneController();

	void HandleEndPlay();

	UFUNCTION(BlueprintCallable)
	bool ShowWidget(EInGameWidgetType WidgetType);

	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TObjectPtr<UDSimDebugComponent> DebugComponent;

	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TSubclassOf<UUserWidget> MenuWidgetClass;

	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TSubclassOf<UUserWidget> ShowPathMenuClass;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TSubclassOf<UUserWidget> DebugWidgetClass;

	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TObjectPtr<UUserWidget> MenuWidget;

	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TObjectPtr<UUserWidget> ShowPathMenu;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TObjectPtr<UUserWidget> DebugWidget;

private:
	/** Tracks which widget is currently in the viewport (if any). */
	UPROPERTY()
	TObjectPtr<UUserWidget> CurrentWidget;
};
