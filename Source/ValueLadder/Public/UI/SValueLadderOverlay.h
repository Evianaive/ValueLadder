#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SBorder;
class SProgressBar;
class STextBlock;
class SVerticalBox;

namespace ValueLadder::UI
{
	inline constexpr float SelectionColumnWidthPx = 100.0f;
	inline constexpr float OverlayWidthPx = 180.0f;
}

class SValueLadderOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SValueLadderOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void UpdateDisplay(const TArray<FText>& InLadderValues, int32 InActiveIndex, double InMultiplier, double InDelta, const FString& InPreviewValue, bool bSelectionLocked, int32 TickCount, double TickProgress, double PixelsToNextTick, double TickThresholdPx, double TickValueDelta);

private:
	void RebuildLadderRows(const TArray<FText>& InLadderValues);
	void UpdateLadderHighlight(int32 InActiveIndex, bool bSelectionLocked);

	TSharedPtr<SVerticalBox> LadderListBox;
	TArray<TSharedPtr<SBorder>> LadderRowBorders;
	TArray<TSharedPtr<STextBlock>> LadderRowTexts;
	TSharedPtr<STextBlock> LockStateTextBlock;
	TSharedPtr<STextBlock> TickFormulaTextBlock;
	TSharedPtr<STextBlock> TickProgressTextBlock;
	TSharedPtr<SProgressBar> TickProgressBar;
	TSharedPtr<STextBlock> MultiplierTextBlock;
	TSharedPtr<STextBlock> DeltaTextBlock;
	TSharedPtr<STextBlock> PreviewTextBlock;
};
