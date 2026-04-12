#include "Session/ValueLadderSession.h"

#include "ValueLadderSettings.h"

bool FValueLadderSession::Begin(const FValueLadderPropertyTarget& InTarget, const FText& TransactionText, FString& OutError)
{
	Reset();

	Target = InTarget;
	if (!Adapter.CaptureBaseline(Target, Baseline, OutError))
	{
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
	return true;
}

bool FValueLadderSession::UpdateFromPixelOffset(
	const double PixelOffset,
	const bool bShiftDown,
	const bool bCtrlDown,
	const UValueLadderSettings& Settings,
	FString& OutError)
{
	if (!bActive)
	{
		OutError = TEXT("No active Value Ladder session.");
		return false;
	}

	CurrentMultiplier = Settings.ResolveStepMultiplier(bShiftDown, bCtrlDown);
	CurrentDelta = Settings.ComputeDeltaFromPixelOffset(PixelOffset, bShiftDown, bCtrlDown);

	if (!Adapter.ApplyDelta(Target, Baseline, CurrentDelta, true))
	{
		OutError = TEXT("Failed to apply delta to property target.");
		return false;
	}

	return true;
}

void FValueLadderSession::Commit()
{
	if (!bActive)
	{
		return;
	}

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
	Target = FValueLadderPropertyTarget();
	Baseline = FValueLadderBaselineData();
	ActiveTransaction.Reset();
}
