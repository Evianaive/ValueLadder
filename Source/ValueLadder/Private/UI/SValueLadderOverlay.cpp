#include "UI/SValueLadderOverlay.h"

#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

void SValueLadderOverlay::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush(TEXT("Menu.Background")))
		.Padding(FMargin(8.0f))
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
	];
}

void SValueLadderOverlay::UpdateDisplay(const double InMultiplier, const double InDelta, const FString& InPreviewValue)
{
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
