#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"

#include "ValueLadderTypes.h"
#include "ValueLadderSettings.generated.h"

USTRUCT()
struct FValueLadderUnitOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Ladder")
	FName Unit = NAME_None;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder")
	TArray<float> FloatLadders;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder", meta = (ClampMin = "0"))
	int32 DefaultFloatLadderIndex = 0;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder")
	TArray<int32> IntLadders;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder", meta = (ClampMin = "0"))
	int32 DefaultIntLadderIndex = 0;
};

USTRUCT()
struct FValueLadderSemanticOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Ladder|Semantics")
	EValueLadderSemanticRole SemanticRole = EValueLadderSemanticRole::GenericScalar;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder|Semantics")
	FName Unit = NAME_None;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder|Semantics")
	TArray<float> FloatLadders;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder|Semantics", meta = (ClampMin = "0"))
	int32 DefaultFloatLadderIndex = 0;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder|Semantics")
	TArray<int32> IntLadders;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder|Semantics", meta = (ClampMin = "0"))
	int32 DefaultIntLadderIndex = 0;
};

UCLASS(Config = EditorPerProjectUserSettings, DefaultConfig)
class VALUELADDER_API UValueLadderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UValueLadderSettings();

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
	float DragActivationThresholdPx = 12.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder")
	TArray<float> FloatLadders = {0.1f, 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f};

	UPROPERTY(EditAnywhere, Config, Category = "Ladder", meta = (ClampMin = "0"))
	int32 DefaultFloatLadderIndex = 3;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder")
	TArray<int32> IntLadders = {1, 10, 100, 1000};

	UPROPERTY(EditAnywhere, Config, Category = "Ladder", meta = (ClampMin = "0"))
	int32 DefaultIntLadderIndex = 0;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder|Units")
	TArray<FValueLadderUnitOverride> UnitOverrides;

	UPROPERTY(EditAnywhere, Config, Category = "Ladder|Semantics")
	TArray<FValueLadderSemanticOverride> SemanticOverrides;

	UPROPERTY(EditAnywhere, Config, Category = "Input", meta = (ClampMin = "0.0001", UIMin = "0.0001"))
	float ShiftStepMultiplier = 10.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Input", meta = (ClampMin = "0.0001", UIMin = "0.0001"))
	float CtrlStepMultiplier = 0.1f;

	UPROPERTY(EditAnywhere, Config, Category = "Types")
	bool bEnableFloat = true;

	UPROPERTY(EditAnywhere, Config, Category = "Types")
	bool bEnableDouble = true;

	UPROPERTY(EditAnywhere, Config, Category = "Types", meta = (DisplayName = "Integers"))
	bool bEnableInt32 = true;

	UPROPERTY(EditAnywhere, Config, Category = "Types")
	bool bEnableVector = true;

	UPROPERTY(EditAnywhere, Config, Category = "UI")
	bool bShowOverlay = true;

	double ResolveStepMultiplier(bool bShiftDown, bool bCtrlDown) const;
	double ComputeDeltaFromPixelOffset(double PixelOffset, double LadderStep, bool bShiftDown, bool bCtrlDown) const;
	bool SupportsType(EValueLadderNumericType NumericType) const;
	int32 GetDefaultLadderIndex(EValueLadderNumericType NumericType, FName UnitKey = NAME_None, EValueLadderSemanticRole SemanticRole = EValueLadderSemanticRole::GenericScalar) const;
	int32 ClampLadderIndex(EValueLadderNumericType NumericType, int32 Index, FName UnitKey = NAME_None, EValueLadderSemanticRole SemanticRole = EValueLadderSemanticRole::GenericScalar) const;
	double GetLadderStep(EValueLadderNumericType NumericType, int32 Index, FName UnitKey = NAME_None, EValueLadderSemanticRole SemanticRole = EValueLadderSemanticRole::GenericScalar) const;
	void BuildLadderDisplayValues(EValueLadderNumericType NumericType, TArray<FText>& OutValues, FName UnitKey = NAME_None, EValueLadderSemanticRole SemanticRole = EValueLadderSemanticRole::GenericScalar) const;
};
