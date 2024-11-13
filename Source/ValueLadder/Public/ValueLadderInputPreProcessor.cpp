
#include "ValueLadderInputPreProcessor.h"

#include "Widgets/Input/SSpinBox.h"

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
		const auto& LocalMousePosition = MouseEvent.GetScreenSpacePosition();
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
					
					//Todo Show UI and focus
					TSharedPtr<SWindow> Window = ValueLadderWindow.Pin();
					Window = SNew(SWindow)
						[
							SNew(SBorder)
							[
								SNew(SButton)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("TestButton")))
								]
							]
						];
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
		if(MouseLeaveState==EMouseEnterState::Right)
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
