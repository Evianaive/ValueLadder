#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SBorder;
class SBox;
class STextBlock;
class SVerticalBox;

namespace ValueLadder::UI
{
	inline constexpr float LadderListWidthPx = 126.0f;
	inline constexpr float OverlayWidthPx = 126.0f;
	inline constexpr float LadderViewportHeightPx = 176.0f;
	inline constexpr float LadderCellHeightPx = 32.0f;
}

class SValueLadderOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SValueLadderOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void UpdateDisplay(const TArray<FText>& InLadderValues, int32 InActiveIndex, double InMultiplier, double InDelta, const FString& InPreviewValue, bool bCursorLocked, int32 TickCount, double TickProgress, double PixelsToNextTick, double TickThresholdPx, double TickValueDelta, double VerticalDragOffsetPx, double HorizontalDragOffsetPx);

private:
	void RebuildLadderRows(const TArray<FText>& InLadderValues);
	void UpdateLadderHighlight(int32 InActiveIndex, bool bCursorLocked);

	TSharedPtr<SBox> LadderViewportBox;
	TSharedPtr<SVerticalBox> LadderListBox;
	TArray<TSharedPtr<SBorder>> LadderRowBorders;
	TArray<TSharedPtr<STextBlock>> LadderRowStepTexts;
	TArray<TSharedPtr<STextBlock>> LadderRowValueTexts;
};
