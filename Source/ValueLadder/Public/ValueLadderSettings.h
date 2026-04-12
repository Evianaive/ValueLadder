#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"

#include "ValueLadderTypes.h"
#include "ValueLadderSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings, DefaultConfig)
class VALUELADDER_API UValueLadderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("Value Ladder"); }

	UPROPERTY(EditAnywhere, Config, Category = "Input")
	bool bRequireAltModifier = false;

	UPROPERTY(EditAnywhere, Config, Category = "Input")
	FKey TriggerMouseButton = EKeys::MiddleMouseButton;

	UPROPERTY(EditAnywhere, Config, Category = "Input", meta = (ClampMin = "0.0001", UIMin = "0.0001"))
	float DragSensitivity = 0.05f;

	UPROPERTY(EditAnywhere, Config, Category = "Input", meta = (ClampMin = "0.5", UIMin = "0.5", ClampMax = "32.0", UIMax = "32.0"))
	float DragActivationThresholdPx = 6.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Input", meta = (ClampMin = "0.0001", UIMin = "0.0001"))
	float ShiftStepMultiplier = 10.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Input", meta = (ClampMin = "0.0001", UIMin = "0.0001"))
	float CtrlStepMultiplier = 0.1f;

	UPROPERTY(EditAnywhere, Config, Category = "Types")
	bool bEnableFloat = true;

	UPROPERTY(EditAnywhere, Config, Category = "Types")
	bool bEnableDouble = true;

	UPROPERTY(EditAnywhere, Config, Category = "Types")
	bool bEnableInt32 = true;

	UPROPERTY(EditAnywhere, Config, Category = "Types")
	bool bEnableVector = true;

	UPROPERTY(EditAnywhere, Config, Category = "UI")
	bool bShowOverlay = true;

	double ResolveStepMultiplier(bool bShiftDown, bool bCtrlDown) const;
	double ComputeDeltaFromPixelOffset(double PixelOffset, bool bShiftDown, bool bCtrlDown) const;
	bool SupportsType(EValueLadderNumericType NumericType) const;
};
