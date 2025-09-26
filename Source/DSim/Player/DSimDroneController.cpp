// Fill out your copyright notice in the Description page of Project Settings.


#include "DSim/Player/DSimDroneController.h"

#include "Blueprint/UserWidget.h"
#include "DSim/DSimDebugComponent.h"
#include "DSim/AI/ML/DSimReinforcementLearningComp.h"

ADSimDroneController::ADSimDroneController()
{
	DebugComponent = CreateDefaultSubobject<UDSimDebugComponent>("DebugComp");
	// Make sure all pointers start as null
	MenuWidget     = nullptr;
	ShowPathMenu   = nullptr;
	DebugWidget    = nullptr;
	CurrentWidget  = nullptr;
}

void ADSimDroneController::HandleEndPlay()
{
	ShowWidget(EInGameWidgetType::EndGameWindow);
}

bool ADSimDroneController::ShowWidget(EInGameWidgetType WidgetType)
{
    // 1) If there is already a widget in the viewport, remove it:
    if (IsValid(CurrentWidget))
    {
        CurrentWidget->RemoveFromParent();
        CurrentWidget = nullptr;
    }

    // 2) Decide which widget instance to use/create:
    UUserWidget* NewWidget = nullptr;

    switch (WidgetType)
    {
        case EInGameWidgetType::DebugWindow:
        {
            // If we haven’t yet instantiated DebugWidget, do it now
            if (DebugWidget == nullptr)
            {
                if (DebugWidgetClass == nullptr)
                {
                    // No class assigned → cannot create
                    return false;
                }
                DebugWidget = CreateWidget<UUserWidget>(this, DebugWidgetClass);
                if (DebugWidget == nullptr)
                {
                    return false;
                }
            }
            NewWidget = DebugWidget;
            break;
        }

        case EInGameWidgetType::ShowPathWindow:
        {
            if (ShowPathMenu == nullptr)
            {
                if (ShowPathMenuClass == nullptr)
                {
                    return false;
                }
                ShowPathMenu = CreateWidget<UUserWidget>(this, ShowPathMenuClass);
                if (ShowPathMenu == nullptr)
                {
                    return false;
                }
            }
            NewWidget = ShowPathMenu;
            break;
        }

        case EInGameWidgetType::EndGameWindow:
        {
            // I’m assuming “EndGameWindow” corresponds to MenuWidget in your header
            if (MenuWidget == nullptr)
            {
                if (MenuWidgetClass == nullptr)
                {
                    return false;
                }
                MenuWidget = CreateWidget<UUserWidget>(this, MenuWidgetClass);
                if (MenuWidget == nullptr)
                {
                    return false;
                }
            }
            NewWidget = MenuWidget;
            break;
        }

        case EInGameWidgetType::None:
        default:
            // If “None” or invalid enum, do nothing (we already cleared CurrentWidget above)
            return false;
    }

    // 3) Finally, add the new widget to the viewport and mark it as current:
    if (NewWidget)
    {
        NewWidget->AddToViewport();
        CurrentWidget = NewWidget;
        return true;
    }

    return false;
}