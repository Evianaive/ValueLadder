#include "UI/SValueLadderOverlay.h"

#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float LadderRowPadding = 2.0f;
}

void SValueLadderOverlay::Construct(const FArguments& InArgs)
{
	ChildSlot
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
				SAssignNew(LadderListBox, SVerticalBox)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
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
	];
}

void SValueLadderOverlay::UpdateDisplay(const TArray<FText>& InLadderValues, const int32 InActiveIndex, const double InMultiplier, const double InDelta, const FString& InPreviewValue)
{
	if (LadderRowBorders.Num() != InLadderValues.Num())
	{
		RebuildLadderRows(InLadderValues);
	}

	UpdateLadderHighlight(InActiveIndex);

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

void SValueLadderOverlay::UpdateLadderHighlight(const int32 InActiveIndex)
{
	for (int32 Index = 0; Index < LadderRowBorders.Num(); ++Index)
	{
		if (!LadderRowBorders[Index].IsValid())
		{
			continue;
		}

		const bool bIsActive = Index == InActiveIndex;
		LadderRowBorders[Index]->SetBorderBackgroundColor(bIsActive ? FLinearColor(0.9f, 0.45f, 0.0f, 0.4f) : FLinearColor::Transparent);
		if (LadderRowTexts.IsValidIndex(Index) && LadderRowTexts[Index].IsValid())
		{
			LadderRowTexts[Index]->SetColorAndOpacity(bIsActive ? FSlateColor(FLinearColor::White) : FSlateColor::UseForeground());
		}
	}
}
