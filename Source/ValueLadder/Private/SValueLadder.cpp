// Fill out your copyright notice in the Description page of Project Settings.


#include "SValueLadder.h"

#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SValueLadder::Construct(const FArguments& InArgs)
{
	auto VerticalBox = SNew(SVerticalBox);
	
	Index = InArgs._Index;
	for (int i = 0;i< InArgs._LadderValues.Num();i++)
	{
		auto& LadderValue = InArgs._LadderValues[i];
		VerticalBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage_Lambda([this, i]()->const FSlateBrush*
			{
				if(Index.Get()==i)
				{
					return new FSlateColorBrush{FLinearColor{0.9,0.45,0,0.4}};
				}
				return FAppStyle::Get().GetBrush("NoBrush");
			})
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LadderValue)
			]
		];
	}
	ChildSlot
	[
		VerticalBox
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
