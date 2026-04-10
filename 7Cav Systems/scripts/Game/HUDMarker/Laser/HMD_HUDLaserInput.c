//------------------------------------------------------------------------------------------------

//! Vehicle HUD laser: visibility (Numpad *, laser designations only), marking (Numpad /), IFF (Numpad 9), laser code [ ]. Hunter-killer lock / slew (LSHIFT, LCTRL, Numpad 8) disabled — no actions registered.

class HMD_HUDLaserInput

{

	protected static bool s_bRegistered;

	//! Last InputManager we bound listeners to; cleared on disconnect so reconnect re-registers.
	protected static InputManager s_RegisteredInputManager;



	//------------------------------------------------------------------------------------------------
	protected static void HMD_RemoveInputListeners(InputManager im)
	{
		if (!im)
			return;
		im.RemoveActionListener("HMD_HUDLaserVisibilityToggle", EActionTrigger.DOWN, OnHUDLaserVisibilityToggle);
		im.RemoveActionListener("HMD_HUDLaserMarkingToggle", EActionTrigger.DOWN, OnHUDLaserMarkingToggle);
		im.RemoveActionListener("HMD_LaserCodeIncrement", EActionTrigger.DOWN, OnLaserCodeIncrement);
		im.RemoveActionListener("HMD_LaserCodeDecrement", EActionTrigger.DOWN, OnLaserCodeDecrement);
		im.RemoveActionListener("HMD_HUDIffMarkersToggle", EActionTrigger.DOWN, OnHUDIffMarkersToggle);
	}

	//------------------------------------------------------------------------------------------------
	protected static void HMD_AddInputListeners(InputManager im)
	{
		if (!im)
			return;
		im.AddActionListener("HMD_HUDLaserVisibilityToggle", EActionTrigger.DOWN, OnHUDLaserVisibilityToggle);
		im.AddActionListener("HMD_HUDLaserMarkingToggle", EActionTrigger.DOWN, OnHUDLaserMarkingToggle);
		im.AddActionListener("HMD_LaserCodeIncrement", EActionTrigger.DOWN, OnLaserCodeIncrement);
		im.AddActionListener("HMD_LaserCodeDecrement", EActionTrigger.DOWN, OnLaserCodeDecrement);
		im.AddActionListener("HMD_HUDIffMarkersToggle", EActionTrigger.DOWN, OnHUDIffMarkersToggle);
	}

	//------------------------------------------------------------------------------------------------
	//! Call from HUD each frame: after reconnect the InputManager may be new; drop listeners when no local pawn.
	static void RegisterOnce()
	{
		if (!GetGame())
			return;
		if (!GetGame().InPlayMode())
		{
			GetGame().GetCallqueue().CallLater(RegisterOnce, 500, false);
			return;
		}
		if (!SCR_PlayerController.GetLocalControlledEntity())
		{
			HMD_RemoveInputListeners(s_RegisteredInputManager);
			s_RegisteredInputManager = null;
			s_bRegistered = false;
			GetGame().GetCallqueue().CallLater(RegisterOnce, 250, false);
			return;
		}
		InputManager im = GetGame().GetInputManager();
		if (!im)
			return;
		if (s_bRegistered && s_RegisteredInputManager == im)
			return;
		HMD_RemoveInputListeners(s_RegisteredInputManager);
		s_RegisteredInputManager = null;
		s_bRegistered = false;
		HMD_AddInputListeners(im);
		s_RegisteredInputManager = im;
		s_bRegistered = true;
	}

	//------------------------------------------------------------------------------------------------
	//! No-op: lock cycle UI disabled; callers may still reset session state.
	static void ResetLockCycleIndex()
	{
	}



	//------------------------------------------------------------------------------------------------

	protected static void OnHUDLaserVisibilityToggle(float value, EActionTrigger reason)

	{

		if (reason != EActionTrigger.DOWN)

			return;

		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();

		if (!localChar)

			return;

		if (!HMD_HudMarkerEligibility.MayUseVehicleHUDLaserVisibility(localChar))

			return;

		SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));

		if (!cac)

			return;

		BaseCompartmentSlot slot = cac.GetCompartment();

		if (!slot)

			return;

		IEntity eligRoot = HMD_VehicleHUDLaserHelpers.ResolveVehicleHudMarkerEligibilityVehicleRoot(slot.GetOwner());

		if (!eligRoot)

			return;

		HMD_HudMarkerEligibilityVehicleComponent elig = HMD_HudMarkerEligibilityVehicleComponent.Cast(eligRoot.FindComponent(HMD_HudMarkerEligibilityVehicleComponent));

		if (!elig)

			return;

		elig.RequestToggleLocalVisibility(localChar, slot);

	}



	//------------------------------------------------------------------------------------------------

	protected static void OnHUDLaserMarkingToggle(float value, EActionTrigger reason)

	{

		if (reason != EActionTrigger.DOWN)

			return;

		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();

		if (!localChar)

			return;

		if (!HMD_HudMarkerEligibility.MayUseVehicleHUDLaserMarking(localChar))

			return;

		SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));

		if (!cac)

			return;

		BaseCompartmentSlot slot = cac.GetCompartment();

		if (!slot)

			return;

		IEntity markRoot = HMD_VehicleHUDLaserHelpers.ResolveVehicleHUDMarkingRoot(slot.GetOwner());

		if (!markRoot)

			return;

		HUDLaserMarkingComponent mark = HMD_VehicleHUDLaserHelpers.FindMarkingComponentOnEntity(markRoot);

		if (!mark)

			return;

		mark.RequestToggleLocalMarking(localChar, slot);

	}

	//------------------------------------------------------------------------------------------------
	protected static void OnHUDIffMarkersToggle(float value, EActionTrigger reason)
	{
		if (reason != EActionTrigger.DOWN)
			return;
		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();
		if (!localChar)
			return;
		if (!HMD_HudMarkerEligibility.MayUseVehicleHudIffMarkersToggle(localChar))
			return;
		HUDMarkerVisibility.ToggleIffMarkers();
	}

	//------------------------------------------------------------------------------------------------

	protected static void OnLaserCodeIncrement(float value, EActionTrigger reason)

	{

		if (reason != EActionTrigger.DOWN)

			return;

		HMD_LaserDesignatorGadgetComponent.ApplyLaserCodeDelta(1);

	}



	//------------------------------------------------------------------------------------------------

	protected static void OnLaserCodeDecrement(float value, EActionTrigger reason)

	{

		if (reason != EActionTrigger.DOWN)

			return;

		HMD_LaserDesignatorGadgetComponent.ApplyLaserCodeDelta(-1);

	}

}

