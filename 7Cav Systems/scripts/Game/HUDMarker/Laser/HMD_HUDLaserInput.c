//------------------------------------------------------------------------------------------------

//! Vehicle HUD laser: visibility (Numpad *, laser designations only), marking (Numpad /), IFF (Numpad 9), laser code [ ], lock (LSHIFT/LCTRL), slew (Numpad 8).

class HMD_HUDLaserInput

{

	protected static bool s_bRegistered;

	//! Last InputManager we bound listeners to; cleared on disconnect so reconnect re-registers.
	protected static InputManager s_RegisteredInputManager;

	protected static int s_iLockCycleIndex = -1;

	protected static ref array<int> s_CycleHudDesignationIdsScratch = {};



	//------------------------------------------------------------------------------------------------
	protected static void HMD_RemoveInputListeners(InputManager im)
	{
		if (!im)
			return;
		im.RemoveActionListener("HMD_HUDLaserVisibilityToggle", EActionTrigger.DOWN, OnHUDLaserVisibilityToggle);
		im.RemoveActionListener("HMD_HUDLaserMarkingToggle", EActionTrigger.DOWN, OnHUDLaserMarkingToggle);
		im.RemoveActionListener("HMD_LaserCodeIncrement", EActionTrigger.DOWN, OnLaserCodeIncrement);
		im.RemoveActionListener("HMD_LaserCodeDecrement", EActionTrigger.DOWN, OnLaserCodeDecrement);
		im.RemoveActionListener("HMD_LaserLockSearchToggle", EActionTrigger.DOWN, OnLaserLockSearchToggle);
		im.RemoveActionListener("HMD_LaserLockRemove", EActionTrigger.DOWN, OnLaserLockRemove);
		im.RemoveActionListener("HMD_LaserLockSlewToTarget", EActionTrigger.DOWN, OnLaserLockSlewToTarget);
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
		im.AddActionListener("HMD_LaserLockSearchToggle", EActionTrigger.DOWN, OnLaserLockSearchToggle);
		im.AddActionListener("HMD_LaserLockRemove", EActionTrigger.DOWN, OnLaserLockRemove);
		im.AddActionListener("HMD_LaserLockSlewToTarget", EActionTrigger.DOWN, OnLaserLockSlewToTarget);
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
	static void ResetLockCycleIndex()
	{
		s_iLockCycleIndex = -1;
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



	//------------------------------------------------------------------------------------------------

	//! Each press advances through active designations in the current camera view; one target stays selected.
	protected static void OnLaserLockSearchToggle(float value, EActionTrigger reason)

	{

		if (reason != EActionTrigger.DOWN)

			return;

		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();

		if (!localChar)

			return;

		if (!HMD_HudMarkerEligibility.MayUseVehicleHUDLaserMarking(localChar))

			return;

		//! Do not gate on IsLasingActive(): vehicle marking sets it while lasing; lock must run to disable laser + acquire target.

		s_CycleHudDesignationIdsScratch.Clear();
		ChimeraWorld lockWorld = GetGame().GetWorld();
		if (lockWorld)
		{
			HUDMarkerSystem hudSys = HUDMarkerSystem.GetInstance(lockWorld);
			if (hudSys)
				hudSys.CollectDesignationIdsInCameraCone(HMD_VehicleLaserAimHelpers.LOCK_CYCLE_CAMERA_HALF_ANGLE_DEG, s_CycleHudDesignationIdsScratch);
		}
		int cnt = s_CycleHudDesignationIdsScratch.Count();

		if (cnt == 0)

		{

			s_iLockCycleIndex = -1;

			HMD_LaserLockState.SetLocked(false);

			HMD_LaserLockState.SetLockedDesignator(null);

			return;

		}

		s_iLockCycleIndex = (s_iLockCycleIndex + 1) % cnt;

		HMD_LaserLockState.ApplyManualLockToHudDesignation(s_CycleHudDesignationIdsScratch[s_iLockCycleIndex]);

		SCR_CompartmentAccessComponent cacMark = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));
		BaseCompartmentSlot slotMark = null;
		if (cacMark)
			slotMark = cacMark.GetCompartment();
		if (slotMark)
		{
			HUDLaserMarkingComponent mark = HUDLaserMarkingComponent.FindMarkingComponentForVehicleSlot(slotMark);
			if (mark)
				mark.ForceDisableLocalMarking();
		}
		HMD_LaserLockState.RefreshLockedTargetReadout();

	}

	//------------------------------------------------------------------------------------------------
	protected static void OnLaserLockRemove(float value, EActionTrigger reason)
	{
		if (reason != EActionTrigger.DOWN)
			return;
		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();
		if (!localChar)
			return;
		if (!HMD_HudMarkerEligibility.MayUseVehicleHUDLaserMarking(localChar))
			return;
		if (!HMD_LaserLockState.IsLocked())
			return;
		s_iLockCycleIndex = -1;
		HMD_LaserLockState.SetLocked(false);
		HMD_LaserLockState.SetLockedDesignator(null);
	}



	//------------------------------------------------------------------------------------------------

	protected static void OnLaserLockSlewToTarget(float value, EActionTrigger reason)

	{

		if (reason != EActionTrigger.DOWN)

			return;

		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();

		if (!localChar)

			return;

		if (!HMD_HudMarkerEligibility.MayUseVehicleHUDLaserMarking(localChar))

			return;

		if (!HMD_LaserLockState.IsLocked())

			return;

		WCS_Armament_HandheldLaserDesignatorComponent des = HMD_LaserLockState.GetLockedDesignator();

		if (des && des.HasValidDesignation())
			HMD_VehicleLaserAimHelpers.TryBeginSlewToLockedDesignation(localChar, des);
		else
			HMD_VehicleLaserAimHelpers.TryBeginSlewToLockedTargetFromLockState(localChar);

	}

}


