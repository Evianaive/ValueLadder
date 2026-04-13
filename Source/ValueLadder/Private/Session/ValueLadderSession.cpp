#include "Session/ValueLadderSession.h"

#include "ValueLadderLog.h"
#include "ValueLadderSettings.h"

bool FValueLadderSession::Begin(const FValueLadderPropertyTarget& InTarget, const FText& TransactionText, FString& OutError)
{
	Reset();

	Target = InTarget;
	if (!Adapter.CaptureBaseline(Target, Baseline, OutError))
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[Session] Begin failed during baseline capture: %s"), *OutError);
		Reset();
		return false;
	}

	ActiveTransaction = MakeUnique<FScopedTransaction>(TransactionText, true);

	if (Target.PropertyHandle.IsValid())
	{
		TArray<UObject*> OuterObjects;
		Target.PropertyHandle->GetOuterObjects(OuterObjects);
		for (UObject* OuterObject : OuterObjects)
		{
			if (OuterObject != nullptr)
			{
				OuterObject->Modify();
			}
		}
	}

	bActive = true;
	UE_LOG(LogValueLadder, Display, TEXT("[Session] Begin success. BaselineValues=%d"), Baseline.BaselineValues.Num());
	return true;
}

bool FValueLadderSession::UpdateFromPixelOffset(
	const double PixelOffset,
	const double LadderStep,
	const bool bShiftDown,
	const bool bCtrlDown,
	const UValueLadderSettings& Settings,
	FString& OutError)
{
	if (!bActive)
	{
		OutError = TEXT("No active Value Ladder session.");
		UE_LOG(LogValueLadder, Warning, TEXT("[Session] Update rejected: %s"), *OutError);
		return false;
	}

	const double NewMultiplier = Settings.ResolveStepMultiplier(bShiftDown, bCtrlDown);
	if (!bHasSegmentContext)
	{
		bHasSegmentContext = true;
		SegmentBaseDelta = 0.0;
		SegmentStartPixelOffset = 0.0;
		SegmentLadderStep = LadderStep;
		SegmentMultiplier = NewMultiplier;
	}
	else if (!FMath::IsNearlyEqual(SegmentLadderStep, LadderStep) || !FMath::IsNearlyEqual(SegmentMultiplier, NewMultiplier))
	{
		SegmentBaseDelta = ValueLadder::Math::ComputeSegmentedDelta(
			SegmentBaseDelta,
			PixelOffset,
			SegmentStartPixelOffset,
			Settings.DragActivationThresholdPx,
			SegmentLadderStep,
			SegmentMultiplier);
		SegmentStartPixelOffset = PixelOffset;
		SegmentLadderStep = LadderStep;
		SegmentMultiplier = NewMultiplier;

		UE_LOG(
			LogValueLadder,
			VeryVerbose,
			TEXT("[Session] Re-anchored drag segment baseDelta=%.6g startPixel=%.6g ladderStep=%.6g multiplier=%.6g"),
			SegmentBaseDelta,
			SegmentStartPixelOffset,
			SegmentLadderStep,
			SegmentMultiplier);
	}

	const double NewDelta = ValueLadder::Math::ComputeSegmentedDelta(
		SegmentBaseDelta,
		PixelOffset,
		SegmentStartPixelOffset,
		Settings.DragActivationThresholdPx,
		SegmentLadderStep,
		SegmentMultiplier);

	if (FMath::IsNearlyEqual(CurrentMultiplier, NewMultiplier) && FMath::IsNearlyEqual(CurrentDelta, NewDelta))
	{
		UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Session] Update skipped; quantized delta unchanged (delta=%.6g multiplier=%.6g)."), NewDelta, NewMultiplier);
		return true;
	}

	CurrentMultiplier = NewMultiplier;
	CurrentDelta = NewDelta;

	if (!Adapter.ApplyDelta(Target, Baseline, CurrentDelta, true))
	{
		OutError = TEXT("Failed to apply delta to property target.");
		UE_LOG(LogValueLadder, Warning, TEXT("[Session] Update failed: %s delta=%.6g multiplier=%.6g ladderStep=%.6g"), *OutError, CurrentDelta, CurrentMultiplier, LadderStep);
		return false;
	}

	UE_LOG(LogValueLadder, VeryVerbose, TEXT("[Session] Update success delta=%.6g multiplier=%.6g ladderStep=%.6g pixelOffset=%.6g"), CurrentDelta, CurrentMultiplier, LadderStep, PixelOffset);
	return true;
}

void FValueLadderSession::Commit()
{
	if (!bActive)
	{
		return;
	}

	UE_LOG(LogValueLadder, Display, TEXT("[Session] Commit delta=%.6g multiplier=%.6g"), CurrentDelta, CurrentMultiplier);

	if (Target.PropertyHandle.IsValid())
	{
		Target.PropertyHandle->NotifyFinishedChangingProperties();
	}

	ActiveTransaction.Reset();
	Reset();
}

void FValueLadderSession::Cancel()
{
	if (!bActive)
	{
		return;
	}

	UE_LOG(LogValueLadder, Display, TEXT("[Session] Cancel delta=%.6g multiplier=%.6g"), CurrentDelta, CurrentMultiplier);

	Adapter.RestoreBaseline(Target, Baseline);
	if (ActiveTransaction.IsValid())
	{
		ActiveTransaction->Cancel();
		ActiveTransaction.Reset();
	}

	Reset();
}

FString FValueLadderSession::GetPreviewValueText() const
{
	return Adapter.BuildPreviewText(Target, Baseline, CurrentDelta);
}

void FValueLadderSession::Reset()
{
	bActive = false;
	CurrentDelta = 0.0;
	CurrentMultiplier = 1.0;
	bHasSegmentContext = false;
	SegmentBaseDelta = 0.0;
	SegmentStartPixelOffset = 0.0;
	SegmentLadderStep = 0.0;
	SegmentMultiplier = 1.0;
	Target = FValueLadderPropertyTarget();
	Baseline = FValueLadderBaselineData();
	ActiveTransaction.Reset();
}
