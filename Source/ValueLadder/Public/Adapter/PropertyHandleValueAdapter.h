#pragma once

#include "CoreMinimal.h"
#include "PropertyHandle.h"

#include "ValueLadderTypes.h"

struct FValueLadderPropertyTarget
{
	TSharedPtr<IPropertyHandle> PropertyHandle;
	EValueLadderNumericType NumericType = EValueLadderNumericType::Float;
	bool bIsVectorComponent = false;

	bool IsValid() const
	{
		return PropertyHandle.IsValid() && PropertyHandle->IsValidHandle();
	}
};

struct FValueLadderBaselineData
{
	TArray<double> BaselineValues;
	FValueLadderConstraintRange Constraints;
};

class FPropertyHandleValueAdapter
{
public:
	bool CaptureBaseline(const FValueLadderPropertyTarget& Target, FValueLadderBaselineData& OutBaseline, FString& OutError) const;
	bool ApplyDelta(const FValueLadderPropertyTarget& Target, const FValueLadderBaselineData& Baseline, double Delta, bool bInteractive) const;
	bool RestoreBaseline(const FValueLadderPropertyTarget& Target, const FValueLadderBaselineData& Baseline) const;
	FString BuildPreviewText(const FValueLadderPropertyTarget& Target, const FValueLadderBaselineData& Baseline, double Delta) const;

private:
	bool PopulateConstraints(const TSharedPtr<IPropertyHandle>& Handle, FValueLadderConstraintRange& OutConstraints) const;
	bool TryParseBound(const TSharedPtr<IPropertyHandle>& Handle, const FName& MetadataName, double& OutValue) const;

	static double ReadScalar(void* RawData, EValueLadderNumericType NumericType);
	static void WriteScalar(void* RawData, EValueLadderNumericType NumericType, double NewValue);
};
