#include "UI/SValueLadderOverlay.h"

#include "ValueLadderLog.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float StepLinePadding = 0.0f;
	constexpr float ActiveValuePadding = 0.0f;

	FLinearColor GetChromeColor()
	{
		return FLinearColor(0.02f, 0.02f, 0.02f, 0.96f);
	}

	FLinearColor GetIdleCellColor()
	{
		return FLinearColor(0.08f, 0.08f, 0.08f, 0.96f);
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
			.Padding(FMargin(ValueLadder::UI::OverlayChromeInsetPx))
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

	if (LadderViewportBox.IsValid())
	{
		const float ContentHeightPx = InLadderValues.Num() > 0
			? static_cast<float>(InLadderValues.Num()) * ValueLadder::UI::LadderRowStridePx - ValueLadder::UI::LadderRowSpacingPx
			: ValueLadder::UI::LadderCellHeightPx;
		LadderViewportBox->SetHeightOverride(ContentHeightPx);
	}

	if (!LadderListBox.IsValid())
	{
		UE_LOG(LogValueLadder, Warning, TEXT("[OverlayUI] UpdateDisplay has invalid LadderListBox. rows=%d activeIndex=%d"), InLadderValues.Num(), InActiveIndex);
	}
	else
	{
		LadderListBox->SetRenderTransform(FSlateRenderTransform(FVector2D::ZeroVector));
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
			const FString ActiveDeltaText = FString::Printf(TEXT("%+.3g"), InDelta);
			LadderRowValueTexts[Index]->SetText(FText::FromString(bIsActive ? ActiveDeltaText : FString()));
			LadderRowValueTexts[Index]->SetRenderTransform(FSlateRenderTransform(FVector2D::ZeroVector));
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
		.Padding(0.0f, 0.0f, 0.0f, ValueLadder::UI::LadderRowSpacingPx)
		[
			SNew(SBox)
			.HeightOverride(ValueLadder::UI::LadderCellHeightPx)
			[
				SAssignNew(RowBorder, SBorder)
				.BorderImage(FAppStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(GetIdleCellColor())
				.Padding(FMargin(6.0f, 2.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, StepLinePadding, 0.0f, 0.0f)
					[
						SAssignNew(StepText, STextBlock)
						.Text(LadderValue)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.62f, 0.66f)))
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
		const FLinearColor ActiveRowColor = bCursorLocked ? GetActiveCellColor() : FLinearColor(0.86f, 0.48f, 0.07f, 0.92f);
		LadderRowBorders[Index]->SetBorderBackgroundColor(bIsActive ? ActiveRowColor : GetIdleCellColor());
		LadderRowBorders[Index]->SetDesiredSizeScale(bIsActive && bCursorLocked ? FVector2D(1.0f, 1.03f) : FVector2D(1.0f, 1.0f));

		if (LadderRowStepTexts.IsValidIndex(Index) && LadderRowStepTexts[Index].IsValid())
		{
			LadderRowStepTexts[Index]->SetColorAndOpacity(bIsActive ? FSlateColor(FLinearColor(0.10f, 0.10f, 0.12f)) : FSlateColor(FLinearColor(0.60f, 0.62f, 0.66f)));
		}

		if (LadderRowValueTexts.IsValidIndex(Index) && LadderRowValueTexts[Index].IsValid())
		{
			LadderRowValueTexts[Index]->SetColorAndOpacity(bIsActive ? FSlateColor(FLinearColor(0.02f, 0.02f, 0.03f)) : FSlateColor(FLinearColor(0.85f, 0.87f, 0.92f, 0.0f)));
		}
	}
}
