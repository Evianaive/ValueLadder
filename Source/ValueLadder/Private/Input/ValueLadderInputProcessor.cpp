#include "Input/ValueLadderInputProcessor.h"

#include "Input/ValueLadderTargetRegistry.h"
#include "UI/SValueLadderOverlay.h"
#include "ValueLadderSettings.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "ValueLadderInputProcessor"

FValueLadderInputProcessor::FValueLadderInputProcessor()
{
}

void FValueLadderInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
}

bool FValueLadderInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (bDragging)
	{
		return true;
	}

	ClearPendingActivation();

	const UValueLadderSettings* Settings = GetDefault<UValueLadderSettings>();
	if (Settings == nullptr)
	{
		return false;
	}

	if (MouseEvent.GetEffectingButton() != Settings->TriggerMouseButton)
	{
		return false;
	}

	if (Settings->bRequireAltModifier && !MouseEvent.IsAltDown())
	{
		return false;
	}

	const FVector2D CursorPosition = MouseEvent.GetScreenSpacePosition();
	const FWidgetPath WidgetsUnderCursor = SlateApp.LocateWindowUnderMouse(CursorPosition, SlateApp.GetInteractiveTopLevelWindows());

	FValueLadderPropertyTarget Target;
	if (!FValueLadderTargetRegistry::Get().ResolveTargetFromWidgetPath(WidgetsUnderCursor, Target))
	{
		return false;
	}

	if (Target.bIsVectorComponent)
	{
		if (!Settings->bEnableVector)
		{
			return false;
		}
	}
	else if (!Settings->SupportsType(Target.NumericType))
	{
		return false;
	}

	bPendingActivation = true;
	PendingTarget = Target;
	DragStartPosition = CursorPosition;

	// Left mouse relies on the native numeric widget to establish focus/capture first.
	// Non-left triggers need us to hold onto the gesture lifecycle ourselves.
	return Settings->TriggerMouseButton != EKeys::LeftMouseButton;
}

bool FValueLadderInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	const UValueLadderSettings* Settings = GetDefault<UValueLadderSettings>();
	if (bPendingActivation)
	{
		if (Settings == nullptr || MouseEvent.GetEffectingButton() == Settings->TriggerMouseButton)
		{
			ClearPendingActivation();
			return false;
		}
	}

	if (!bDragging)
	{
		return false;
	}

	if (Settings == nullptr)
	{
		Session.Cancel();
		bDragging = false;
		DestroyOverlay();
		return false;
	}

	if (MouseEvent.GetEffectingButton() != Settings->TriggerMouseButton)
	{
		return false;
	}

	Session.Commit();
	bDragging = false;
	DestroyOverlay();

	return true;
}

bool FValueLadderInputProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	const UValueLadderSettings* Settings = GetDefault<UValueLadderSettings>();

	if (!bDragging && bPendingActivation)
	{
		if (Settings == nullptr)
		{
			ClearPendingActivation();
			return false;
		}

		if (!MouseEvent.IsMouseButtonDown(Settings->TriggerMouseButton))
		{
			ClearPendingActivation();
			return false;
		}

		const FVector2D MouseOffset = MouseEvent.GetScreenSpacePosition() - DragStartPosition;
		const float ActivationThresholdPx = FMath::Max(Settings->DragActivationThresholdPx, 0.5f);
		if (MouseOffset.SizeSquared() < FMath::Square(ActivationThresholdPx))
		{
			return false;
		}

		FString BeginError;
		if (!Session.Begin(PendingTarget, LOCTEXT("ValueLadderDrag", "Value Ladder Drag"), BeginError))
		{
			UE_LOG(LogTemp, Warning, TEXT("ValueLadder begin failed after focus press: %s"), *BeginError);
			ClearPendingActivation();
			return false;
		}

		bDragging = true;
		ClearPendingActivation();

		if (Settings->bShowOverlay)
		{
			EnsureOverlay(SlateApp, MouseEvent.GetScreenSpacePosition());
		}
	}

	if (!bDragging)
	{
		return false;
	}

	if (Settings == nullptr)
	{
		Session.Cancel();
		bDragging = false;
		DestroyOverlay();
		return false;
	}

	const double PixelOffset = MouseEvent.GetScreenSpacePosition().X - DragStartPosition.X;
	FString SessionError;
	if (!Session.UpdateFromPixelOffset(PixelOffset, MouseEvent.IsShiftDown(), MouseEvent.IsControlDown(), *Settings, SessionError))
	{
		UE_LOG(LogTemp, Warning, TEXT("ValueLadder update failed: %s"), *SessionError);
		Session.Cancel();
		bDragging = false;
		DestroyOverlay();
		return true;
	}

	UpdateOverlay(MouseEvent.GetScreenSpacePosition());
	return true;
}

bool FValueLadderInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (bPendingActivation && InKeyEvent.GetKey() == EKeys::Escape)
	{
		ClearPendingActivation();
		return false;
	}

	if (!bDragging)
	{
		return false;
	}

	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		Session.Cancel();
		bDragging = false;
		ClearPendingActivation();
		DestroyOverlay();
		return true;
	}

	return false;
}

void FValueLadderInputProcessor::EnsureOverlay(FSlateApplication& SlateApp, const FVector2D& CursorPosition)
{
	if (OverlayWindow.IsValid())
	{
		return;
	}

	OverlayWidget = SNew(SValueLadderOverlay);
	TSharedRef<SWindow> Window = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::Autosized)
		.FocusWhenFirstShown(false)
		.ScreenPosition(CursorPosition + FVector2D(16.0f, 16.0f))
		[
			OverlayWidget.ToSharedRef()
		];

	SlateApp.AddWindow(Window, true);
	OverlayWindow = Window;
}

void FValueLadderInputProcessor::UpdateOverlay(const FVector2D& CursorPosition)
{
	if (OverlayWidget.IsValid())
	{
		OverlayWidget->UpdateDisplay(Session.GetCurrentMultiplier(), Session.GetCurrentDelta(), Session.GetPreviewValueText());
	}

	if (OverlayWindow.IsValid())
	{
		OverlayWindow.Pin()->MoveWindowTo(CursorPosition + FVector2D(16.0f, 16.0f));
	}
}

void FValueLadderInputProcessor::DestroyOverlay()
{
	if (OverlayWindow.IsValid())
	{
		OverlayWindow.Pin()->RequestDestroyWindow();
		OverlayWindow.Reset();
	}

	OverlayWidget.Reset();
}

void FValueLadderInputProcessor::ClearPendingActivation()
{
	bPendingActivation = false;
	PendingTarget = FValueLadderPropertyTarget();
}

#undef LOCTEXT_NAMESPACE
