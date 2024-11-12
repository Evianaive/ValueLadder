#include "D:\Program Files\Epic Games\UE_5.0_main\Engine\Intermediate\Build\Win64\x64\UnrealEditorGPF\Development\UnrealEd\SharedPCH.UnrealEd.Project.ValApi.Cpp20.InclOrderOldest.h"
#include "ValueLadderInputPreProcessor.h"

FValueLadderInputPreProcessor::FValueLadderInputPreProcessor()
{
}

void FValueLadderInputPreProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
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
		
	}
	return IInputProcessor::HandleMouseButtonUpEvent(SlateApp, MouseEvent);
}

bool FValueLadderInputPreProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp,
	const FPointerEvent& MouseEvent)
{
	if(MouseEnterState != EMouseEnterState::None)
		return true;
	
	if(MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		MouseEnterState = EMouseEnterState::Middle;
		//Todo Check Hover UI Element
		// SlateApp.
		FWidgetPath widgetsUnderCursor = SlateApp.LocateWindowUnderMouse(LocalMousePosition, SlateApp.GetInteractiveTopLevelWindows());
		FScopedSwitchWorldHack SwitchWorld(widgetsUnderCursor);
		FString DebugMessage = TEXT("");
			
		TSet<FKey> CurPressedKeys = SlateApp.GetPressedMouseButtons();
			
		for(FKey CurPressedKey : CurPressedKeys)
		{
			DebugMessage += CurPressedKey.ToString() + " + ";
		}
		DebugMessage += "\n";
		/*似乎后创建的节点在更上层*/
		for (int i = widgetsUnderCursor.Widgets.Num() - 1; i >= 0; i--)
		{
			FString widgetName = widgetsUnderCursor.Widgets[i].Widget->GetTypeAsString();
			if(widgetName == "SGraphPanel")
			{
				FName GraphClassName = StaticCastSharedRef<SGraphPanel>(widgetsUnderCursor.Widgets[i].Widget)->GetGraphObj()->GetClass()->GetFName();
				DebugMessage += GraphClassName.ToString() + TEXT(" : ");
				DebugMessage += StaticCastSharedRef<SGraphPanel>(widgetsUnderCursor.Widgets[i].Widget)->GetViewOffset().ToString();
			}
					
			DebugMessage +=  widgetName + TEXT("\n");
			// if (widgetName == "SGraphPanel")
			// {
			// 	ctx.IsCursorInsidePanel = true;
			// 	ctx.GraphPanel = StaticCastSharedRef<SGraphPanel>(widgetsUnderCursor.Widgets[i].Widget);
			// 	ctx.PanelGeometry = widgetsUnderCursor.Widgets[i].Geometry;
			// }
		}
		{
			//Todo Show UI and focus
			return true;	
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
