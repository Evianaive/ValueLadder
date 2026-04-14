#include "UI/SValueLadderOverlay.h"

#include "ValueLadderLog.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float CellOuterPadding = 2.0f;
	constexpr float StepLinePadding = 1.0f;
	constexpr float ActiveValuePadding = 1.0f;

	FLinearColor GetChromeColor()
	{
		return FLinearColor(0.06f, 0.07f, 0.09f, 0.96f);
	}

	FLinearColor GetIdleCellColor()
	{
		return FLinearColor(0.14f, 0.16f, 0.20f, 0.96f);
	}

	FLinearColor GetActiveCellColor()
	{
		return FLinearColor(0.98f, 0.58f, 0.08f, 0.94f);
	}
}

void SValueLadderOverlay::Construct(const FArguments& InArgs)
{
	UE_LOG(LogValueLadder, Display, TEXT("[OverlayUI] Construct width=%.1f viewportHeight=%.1f rowHeight=%.1f"), ValueLadder::UI::OverlayWidthPx, ValueLadder::UI::LadderViewportHeightPx, ValueLadder::UI::LadderCellHeightPx);

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(ValueLadder::UI::OverlayWidthPx)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush(TEXT("Menu.Background")))
			.BorderBackgroundColor(GetChromeColor())
			.Padding(FMargin(4.0f))
			[
				SAssignNew(LadderViewportBox, SBox)
				.WidthOverride(ValueLadder::UI::LadderListWidthPx)
				.HeightOverride(ValueLadder::UI::LadderViewportHeightPx)
				[
					SAssignNew(LadderListBox, SVerticalBox)
				]
			]
		]
	];
}

void SValueLadderOverlay::UpdateDisplay(const TArray<FText>& InLadderValues, const int32 InActiveIndex, const double InMultiplier, const double InDelta, const FString& InPreviewValue, const bool bCursorLocked, const int32 TickCount, const double TickProgress, const double PixelsToNextTick, const double TickThresholdPx, const double TickValueDelta, const double VerticalDragOffsetPx, const double HorizontalDragOffsetPx)
{
	if (InLadderValues.Num() == 0)
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[OverlayUI] UpdateDisplay received zero rows. activeIndex=%d previewLen=%d cursorLocked=%s verticalOffset=%.2f horizontalOffset=%.2f"), InActiveIndex, InPreviewValue.Len(), bCursorLocked ? TEXT("true") : TEXT("false"), VerticalDragOffsetPx, HorizontalDragOffsetPx);
	}
	else if (!InLadderValues.IsValidIndex(InActiveIndex))
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[OverlayUI] UpdateDisplay received invalid active index=%d for rows=%d. previewLen=%d"), InActiveIndex, InLadderValues.Num(), InPreviewValue.Len());
	}

	if (LadderRowBorders.Num() != InLadderValues.Num())
	{
		RebuildLadderRows(InLadderValues);
	}

	if (!LadderListBox.IsValid())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[OverlayUI] UpdateDisplay has invalid LadderListBox. rows=%d activeIndex=%d"), InLadderValues.Num(), InActiveIndex);
	}
	else
	{
		const float ActiveRowTopPx = static_cast<float>(FMath::Max(InActiveIndex, 0)) * ValueLadder::UI::LadderCellHeightPx;
		const float MinVerticalOffsetPx = ActiveRowTopPx - (ValueLadder::UI::LadderViewportHeightPx - ValueLadder::UI::LadderCellHeightPx);
		const float MaxVerticalOffsetPx = ActiveRowTopPx;
		const float ClampedVerticalOffsetPx = FMath::Clamp(static_cast<float>(VerticalDragOffsetPx), MinVerticalOffsetPx, MaxVerticalOffsetPx);
		if (!FMath::IsNearlyEqual(ClampedVerticalOffsetPx, static_cast<float>(VerticalDragOffsetPx), 0.5f))
		{
			UE_LOG(LogValueLadder, Verbose, TEXT("[OverlayUI] Vertical offset clamped from %.2f to %.2f. activeIndex=%d rows=%d bounds=[%.2f, %.2f]"), VerticalDragOffsetPx, ClampedVerticalOffsetPx, InActiveIndex, InLadderValues.Num(), MinVerticalOffsetPx, MaxVerticalOffsetPx);
		}
		LadderListBox->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, -ClampedVerticalOffsetPx)));
	}

	for (int32 Index = 0; Index < LadderRowStepTexts.Num(); ++Index)
	{
		if (LadderRowStepTexts[Index].IsValid() && InLadderValues.IsValidIndex(Index))
		{
			LadderRowStepTexts[Index]->SetText(InLadderValues[Index]);
		}

		if (LadderRowValueTexts.IsValidIndex(Index) && LadderRowValueTexts[Index].IsValid())
		{
			const bool bIsActive = Index == InActiveIndex;
			LadderRowValueTexts[Index]->SetText(FText::FromString(bIsActive ? InPreviewValue : FString()));
			LadderRowValueTexts[Index]->SetRenderTransform(FSlateRenderTransform(FVector2D(bIsActive ? static_cast<float>(-HorizontalDragOffsetPx * 0.15) : 0.0f, 0.0f)));
		}
	}

	UpdateLadderHighlight(InActiveIndex, bCursorLocked);
}

void SValueLadderOverlay::RebuildLadderRows(const TArray<FText>& InLadderValues)
{
	LadderRowBorders.Reset();
	LadderRowStepTexts.Reset();
	LadderRowValueTexts.Reset();

	if (!LadderListBox.IsValid())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[OverlayUI] RebuildLadderRows skipped because LadderListBox is invalid. rows=%d"), InLadderValues.Num());
		return;
	}

	UE_LOG(LogValueLadder, Display, TEXT("[OverlayUI] RebuildLadderRows rows=%d"), InLadderValues.Num());

	LadderListBox->ClearChildren();
	LadderRowBorders.Reserve(InLadderValues.Num());
	LadderRowStepTexts.Reserve(InLadderValues.Num());
	LadderRowValueTexts.Reserve(InLadderValues.Num());

	for (const FText& LadderValue : InLadderValues)
	{
		TSharedPtr<SBorder> RowBorder;
		TSharedPtr<STextBlock> StepText;
		TSharedPtr<STextBlock> ValueText;

		LadderListBox->AddSlot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(ValueLadder::UI::LadderCellHeightPx)
			[
				SAssignNew(RowBorder, SBorder)
				.BorderImage(FAppStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(GetIdleCellColor())
				.Padding(FMargin(6.0f, 3.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, StepLinePadding, 0.0f, 0.0f)
					[
						SAssignNew(StepText, STextBlock)
						.Text(LadderValue)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.82f, 0.86f)))
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(0.0f, ActiveValuePadding, 0.0f, 0.0f)
					[
						SAssignNew(ValueText, STextBlock)
						.Text(FText::GetEmpty())
						.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)))
					]
				]
			]
		];

		LadderRowBorders.Add(RowBorder);
		LadderRowStepTexts.Add(StepText);
		LadderRowValueTexts.Add(ValueText);
	}
}

void SValueLadderOverlay::UpdateLadderHighlight(const int32 InActiveIndex, const bool bCursorLocked)
{
	for (int32 Index = 0; Index < LadderRowBorders.Num(); ++Index)
	{
		if (!LadderRowBorders[Index].IsValid())
		{
			continue;
		}

		const bool bIsActive = Index == InActiveIndex;
		LadderRowBorders[Index]->SetBorderBackgroundColor(bIsActive ? GetActiveCellColor() : GetIdleCellColor());
		LadderRowBorders[Index]->SetDesiredSizeScale(bIsActive && bCursorLocked ? FVector2D(1.0f, 1.08f) : FVector2D(1.0f, 1.0f));

		if (LadderRowStepTexts.IsValidIndex(Index) && LadderRowStepTexts[Index].IsValid())
		{
			LadderRowStepTexts[Index]->SetColorAndOpacity(bIsActive ? FSlateColor(FLinearColor(0.10f, 0.10f, 0.12f)) : FSlateColor(FLinearColor(0.80f, 0.82f, 0.86f)));
		}

		if (LadderRowValueTexts.IsValidIndex(Index) && LadderRowValueTexts[Index].IsValid())
		{
			LadderRowValueTexts[Index]->SetColorAndOpacity(bIsActive ? FSlateColor(FLinearColor(0.02f, 0.02f, 0.03f)) : FSlateColor(FLinearColor(0.85f, 0.87f, 0.92f, 0.0f)));
		}
	}
}
