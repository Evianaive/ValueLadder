#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;

class SValueLadderOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SValueLadderOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void UpdateDisplay(double InMultiplier, double InDelta, const FString& InPreviewValue);

private:
	TSharedPtr<STextBlock> MultiplierTextBlock;
	TSharedPtr<STextBlock> DeltaTextBlock;
	TSharedPtr<STextBlock> PreviewTextBlock;
};
