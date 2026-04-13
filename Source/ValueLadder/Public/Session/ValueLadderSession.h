#pragma once

#include "CoreMinimal.h"
#include "ScopedTransaction.h"

#include "Adapter/PropertyHandleValueAdapter.h"

class UValueLadderSettings;

class FValueLadderSession
{
public:
	bool Begin(const FValueLadderPropertyTarget& InTarget, const FText& TransactionText, FString& OutError);
	bool UpdateFromPixelOffset(double PixelOffset, double LadderStep, bool bShiftDown, bool bCtrlDown, const UValueLadderSettings& Settings, FString& OutError);
	void Commit();
	void Cancel();

	bool IsActive() const { return bActive; }
	double GetCurrentDelta() const { return CurrentDelta; }
	double GetCurrentMultiplier() const { return CurrentMultiplier; }
	FString GetPreviewValueText() const;

private:
	void Reset();

	FPropertyHandleValueAdapter Adapter;
	FValueLadderPropertyTarget Target;
	FValueLadderBaselineData Baseline;
	TUniquePtr<FScopedTransaction> ActiveTransaction;

	bool bActive = false;
	double CurrentDelta = 0.0;
	double CurrentMultiplier = 1.0;
	bool bHasSegmentContext = false;
	double SegmentBaseDelta = 0.0;
	double SegmentStartPixelOffset = 0.0;
	double SegmentLadderStep = 0.0;
	double SegmentMultiplier = 1.0;
};
