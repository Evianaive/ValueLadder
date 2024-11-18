
#include "ValueLadderInputPreProcessor.h"

#include "SValueLadder.h"
#include "ValueLadderSettings.h"


#define LOCTEXT_NAMESPACE "ValueBuilder"


struct FValueLadderHolderBase
{
	TSharedRef<SValueLadder> MakeValueLadder()
	{
		return SNew(SValueLadder)
			.LadderValues(MoveTemp(LadderValueStrings));
	}
	// TSharedRef<SValueLadder> MakeValueLadder(const TFunction<int32()>& GetIndexFunction)
	// {
	// 	return SNew(SValueLadder)
	// 		.LadderValues(MoveTemp(LadderValueStrings))
	// 		.Index(GetIndexFunction);
	// }
	TArray<FText> LadderValueStrings;
};

template<typename T>
struct FValueLadderHolder : FValueLadderHolderBase
{
	FValueLadderHolder(){};
	FValueLadderHolder(const TArray<T>& InValues, int32 InStartIndex)
	:Values(&InValues)
	,StartIndex(InStartIndex)
	{
		if(!Values->Num())
			return;
		LadderValueStrings.Reserve(Values->Num());
		for (T LadderValue : *Values)
		{
			FText& Text = LadderValueStrings.AddDefaulted_GetRef();
			if constexpr (std::is_same_v<T,int32>)
				Text = FText::FromString(FString::FromInt(LadderValue));			
			else
				Text = FText::FromString(FString::SanitizeFloat(LadderValue));
		}
	}
	const TArray<T>* Values {nullptr};
	int32 StartIndex{0};
};
// template struct FValueLadderHolder<float>;
// template struct FValueLadderHolder<int32>;

using TValueLadderBuilder = TVariant<FValueLadderHolder<int32>,FValueLadderHolder<float>>;

template<typename TLadderValuesProvider>
void static CreateValueLadder(TOptional<TValueLadderBuilder>& Builder,const FName ValueType, const TLadderValuesProvider* LadderValuesProvider)
{
	TSet<FName> IntTypes{"int","int64","int32","int16","int8","uint64","uint32","uint16","uint8"};
	TSet<FName> FloatTypes{"float","double"};

	if(IntTypes.Contains(ValueType))
	{
		// const TArray<int32>& Values = LadderValuesProvider.template GetLadderValues<int32>();
		Builder.Emplace(
			TValueLadderBuilder{TInPlaceType<FValueLadderHolder<int32>>(),
			LadderValuesProvider->template GetLadderValues<int32>(),
			LadderValuesProvider->template GetDefaultLadderValuesIndex<int32>()}
			);
	}
	else if(FloatTypes.Contains(ValueType))
	{
		// const TArray<float>& Values = LadderValuesProvider.template GetLadderValues<float>();
		Builder.Emplace(
			TValueLadderBuilder{TInPlaceType<FValueLadderHolder<float>>(),
			LadderValuesProvider->template GetLadderValues<float>(),
			LadderValuesProvider->template GetDefaultLadderValuesIndex<float>()}
			);		
	}
}

FValueLadderInputPreProcessor::FValueLadderInputPreProcessor()
{
}

void FValueLadderInputPreProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
}

bool FValueLadderInputPreProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp,
	const FPointerEvent& MouseEvent)
{
	if(MouseEnterState != EMouseEnterState::None)
		return true;
	
	if(MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		MouseEnterState = EMouseEnterState::Middle;
		const FVector2D& LocalMousePosition = MouseEvent.GetScreenSpacePosition();
		FWidgetPath widgetsUnderCursor = SlateApp.LocateWindowUnderMouse(LocalMousePosition, SlateApp.GetInteractiveTopLevelWindows());
		FScopedSwitchWorldHack SwitchWorld(widgetsUnderCursor);
		FString DebugMessage = TEXT("");
			
		TSet<FKey> CurPressedKeys = SlateApp.GetPressedMouseButtons();
			
		for(FKey CurPressedKey : CurPressedKeys)
		{
			DebugMessage += CurPressedKey.ToString() + " + ";
		}
		DebugMessage += "\n";
		/*后创建的Widget在更上层*/
		FString WidgetName = widgetsUnderCursor.Widgets.Last().Widget->GetTypeAsString();
		if(WidgetName == "STextBlock")
		{
			// May contain PropertyEditorNumeric
			for (int i = widgetsUnderCursor.Widgets.Num() - 2; i >= 0; i--)
			{
				WidgetName = widgetsUnderCursor.Widgets[i].Widget->GetTypeAsString();
				if(WidgetName.Contains(TEXT("SPropertyEditorNumeric<")))
				{
					WidgetName.RemoveAt(0,23);
					WidgetName.RemoveAt(WidgetName.Len()-1);
					
					TOptional<TValueLadderBuilder> Builder;
					CreateValueLadder(Builder,FName(WidgetName),GetDefault<UValueLadderSettings>());
					if(!Builder.IsSet())
					{
						UE_LOG(LogTemp,Log,TEXT("SPropertyEditorNumeric<%s> is not supported"),*WidgetName);
						return true;
					}
					TSharedPtr<SWindow> Window = ValueLadderWindow.Pin();
					auto ValueLadderWidget = ::Visit([](auto& Holder){return Holder.MakeValueLadder();},Builder.GetValue());
										
					// int32 Size = reinterpret_cast<FValueLadderHolderBase*>(&Builder.GetValue())->LadderValueStrings.Num();
					int32 StartIndex = ::Visit([](auto& Holder){return Holder.StartIndex;},Builder.GetValue());
					ValueLadderWidget->SetIndex(StartIndex);
					Window = SNew(SWindow)
						.ScreenPosition(LocalMousePosition - FVector2D(50,10+StartIndex*20))
						.CreateTitleBar(false)
						.AutoCenter(EAutoCenter::None)
						.SizingRule(ESizingRule::Autosized)
						// .ClientSize(FVector2D(100,Size*50))
						[
							ValueLadderWidget
						];
					//Todo Show UI and focus
					SlateApp.AddWindow(Window.ToSharedRef());
					ValueLadderWindow = Window;
					return true;
				}
			}
		}
	}
	else if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		MouseEnterState = EMouseEnterState::Left;		
	else if(MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		MouseEnterState = EMouseEnterState::Right;
	else
		MouseEnterState = EMouseEnterState::None;
	
	return IInputProcessor::HandleMouseButtonDownEvent(SlateApp, MouseEvent);
}

bool FValueLadderInputPreProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp,
	const FPointerEvent& MouseEvent)
{
	EMouseEnterState MouseLeaveState = EMouseEnterState::None;
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		MouseLeaveState = EMouseEnterState::Left;		
	}
	else if(MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		MouseLeaveState = EMouseEnterState::Middle;
	}
	else if(MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		MouseLeaveState = EMouseEnterState::Right;
	}
	
	if(MouseEnterState == MouseLeaveState)
	{
		if(MouseLeaveState==EMouseEnterState::Middle)
		{
			if(ValueLadderWindow.IsValid())
				ValueLadderWindow.Pin()->RequestDestroyWindow();
		}
		MouseEnterState = EMouseEnterState::None;
	}
	return IInputProcessor::HandleMouseButtonUpEvent(SlateApp, MouseEvent);
}

bool FValueLadderInputPreProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if(MouseEnterState == EMouseEnterState::Middle)
	{
		
	}
	return IInputProcessor::HandleMouseMoveEvent(SlateApp, MouseEvent);
}

bool FValueLadderInputPreProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	return IInputProcessor::HandleKeyDownEvent(SlateApp, InKeyEvent);
}

bool FValueLadderInputPreProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	return IInputProcessor::HandleKeyUpEvent(SlateApp, InKeyEvent);
}

bool FValueLadderInputPreProcessor::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp,
	const FPointerEvent& MouseEvent)
{
	return IInputProcessor::HandleMouseButtonDoubleClickEvent(SlateApp, MouseEvent);
}

bool FValueLadderInputPreProcessor::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp,
	const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent)
{
	return IInputProcessor::HandleMouseWheelOrGestureEvent(SlateApp, InWheelEvent, InGestureEvent);
}

#undef LOCTEXT_NAMESPACE