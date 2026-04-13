#include "UI/SValueLadderOverlay.h"

#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float LadderRowPadding = 2.0f;
}

void SValueLadderOverlay::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(ValueLadder::UI::OverlayWidthPx)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush(TEXT("Menu.Background")))
			.Padding(FMargin(8.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(ValueLadder::UI::SelectionColumnWidthPx)
					[
						SAssignNew(LadderListBox, SVerticalBox)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(LockStateTextBlock, STextBlock)
						.Text(FText::FromString(TEXT("Select delta")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SAssignNew(TickFormulaTextBlock, STextBlock)
						.Text(FText::FromString(TEXT("1 tick = 12 px -> 100")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SAssignNew(TickProgressTextBlock, STextBlock)
						.Text(FText::FromString(TEXT("Ticks +0 | Next 12 px")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SAssignNew(TickProgressBar, SProgressBar)
						.Percent(0.0f)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SAssignNew(MultiplierTextBlock, STextBlock)
						.Text(FText::FromString(TEXT("x1.0")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SAssignNew(DeltaTextBlock, STextBlock)
						.Text(FText::FromString(TEXT("Delta 0")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SAssignNew(PreviewTextBlock, STextBlock)
						.Text(FText::FromString(TEXT("Value: -")))
					]
				]
			]
		]
	];
}

void SValueLadderOverlay::UpdateDisplay(const TArray<FText>& InLadderValues, const int32 InActiveIndex, const double InMultiplier, const double InDelta, const FString& InPreviewValue, const bool bSelectionLocked, const int32 TickCount, const double TickProgress, const double PixelsToNextTick, const double TickThresholdPx, const double TickValueDelta)
{
	if (LadderRowBorders.Num() != InLadderValues.Num())
	{
		RebuildLadderRows(InLadderValues);
	}

	UpdateLadderHighlight(InActiveIndex, bSelectionLocked);

	if (LockStateTextBlock.IsValid())
	{
		FText LockText = FText::FromString(TEXT("Select delta"));
		if (bSelectionLocked && InLadderValues.IsValidIndex(InActiveIndex))
		{
			LockText = FText::FromString(FString::Printf(TEXT("Locked: %s"), *InLadderValues[InActiveIndex].ToString()));
		}

		LockStateTextBlock->SetText(LockText);
		LockStateTextBlock->SetColorAndOpacity(bSelectionLocked ? FSlateColor(FLinearColor(1.0f, 0.8f, 0.2f)) : FSlateColor::UseForeground());
	}

	if (TickFormulaTextBlock.IsValid())
	{
		TickFormulaTextBlock->SetText(FText::FromString(FString::Printf(TEXT("1 tick = %.3g px -> %.6g"), TickThresholdPx, TickValueDelta)));
	}

	if (TickProgressTextBlock.IsValid())
	{
		TickProgressTextBlock->SetText(FText::FromString(FString::Printf(TEXT("Ticks this row %+d | Next %.1f px"), TickCount, PixelsToNextTick)));
	}

	if (TickProgressBar.IsValid())
	{
		TickProgressBar->SetPercent(TickProgress);
		TickProgressBar->SetFillColorAndOpacity(bSelectionLocked ? FLinearColor(1.0f, 0.62f, 0.0f, 0.85f) : FLinearColor(0.35f, 0.35f, 0.35f, 0.65f));
	}

	if (MultiplierTextBlock.IsValid())
	{
		MultiplierTextBlock->SetText(FText::FromString(FString::Printf(TEXT("x%.3g"), InMultiplier)));
	}

	if (DeltaTextBlock.IsValid())
	{
		DeltaTextBlock->SetText(FText::FromString(FString::Printf(TEXT("Delta %.6g"), InDelta)));
	}

	if (PreviewTextBlock.IsValid())
	{
		PreviewTextBlock->SetText(FText::FromString(FString::Printf(TEXT("Value: %s"), *InPreviewValue)));
	}
}

void SValueLadderOverlay::RebuildLadderRows(const TArray<FText>& InLadderValues)
{
	LadderRowBorders.Reset();
	LadderRowTexts.Reset();

	if (!LadderListBox.IsValid())
	{
		return;
	}

	LadderListBox->ClearChildren();
	LadderRowBorders.Reserve(InLadderValues.Num());
	LadderRowTexts.Reserve(InLadderValues.Num());

	for (const FText& LadderValue : InLadderValues)
	{
		TSharedPtr<SBorder> RowBorder;
		TSharedPtr<STextBlock> RowText;

		LadderListBox->AddSlot()
		.AutoHeight()
		[
			SAssignNew(RowBorder, SBorder)
			.BorderImage(FAppStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(FLinearColor::Transparent)
			.Padding(FMargin(6.0f, LadderRowPadding))
			[
				SAssignNew(RowText, STextBlock)
				.Text(LadderValue)
			]
		];

		LadderRowBorders.Add(RowBorder);
		LadderRowTexts.Add(RowText);
	}
}

void SValueLadderOverlay::UpdateLadderHighlight(const int32 InActiveIndex, const bool bSelectionLocked)
{
	for (int32 Index = 0; Index < LadderRowBorders.Num(); ++Index)
	{
		if (!LadderRowBorders[Index].IsValid())
		{
			continue;
		}

		const bool bIsActive = Index == InActiveIndex;
		const FLinearColor ActiveColor = bSelectionLocked
			? FLinearColor(1.0f, 0.62f, 0.0f, 0.6f)
			: FLinearColor(0.9f, 0.45f, 0.0f, 0.4f);
		LadderRowBorders[Index]->SetBorderBackgroundColor(bIsActive ? ActiveColor : FLinearColor::Transparent);
		if (LadderRowTexts.IsValidIndex(Index) && LadderRowTexts[Index].IsValid())
		{
			LadderRowTexts[Index]->SetColorAndOpacity(bIsActive ? FSlateColor(FLinearColor::White) : FSlateColor::UseForeground());
		}
	}
}
