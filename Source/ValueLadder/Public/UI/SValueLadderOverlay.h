#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SBorder;
class STextBlock;
class SVerticalBox;

class SValueLadderOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SValueLadderOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void UpdateDisplay(const TArray<FText>& InLadderValues, int32 InActiveIndex, double InMultiplier, double InDelta, const FString& InPreviewValue);

private:
	void RebuildLadderRows(const TArray<FText>& InLadderValues);
	void UpdateLadderHighlight(int32 InActiveIndex);

	TSharedPtr<SVerticalBox> LadderListBox;
	TArray<TSharedPtr<SBorder>> LadderRowBorders;
	TArray<TSharedPtr<STextBlock>> LadderRowTexts;
	TSharedPtr<STextBlock> MultiplierTextBlock;
	TSharedPtr<STextBlock> DeltaTextBlock;
	TSharedPtr<STextBlock> PreviewTextBlock;
};
