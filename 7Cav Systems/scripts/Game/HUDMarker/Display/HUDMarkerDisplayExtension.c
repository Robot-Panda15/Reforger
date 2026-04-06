//------------------------------------------------------------------------------------------------

//! Modded HUDMarkerDisplay - laser designation readout UI; world marker dots live on SCR_HUDManagerComponent (dual layouts).

modded class HUDMarkerDisplay

{

	//------------------------------------------------------------------------------------------------

	//! World dots are rendered from HMD_LaserDesignationOverlay + HMD_IffMarkerOverlay on the HUD manager, not this InfoDisplay prefab pool.

	override protected void PopulateMarkerPool()

	{

	}



	//------------------------------------------------------------------------------------------------

	override void DisplayStartDraw(IEntity owner)

	{

		super.DisplayStartDraw(owner);

		PlayerController pc = GetGame().GetPlayerController();

		SCR_HUDManagerComponent hudMgr = null;

		if (pc)

			hudMgr = SCR_HUDManagerComponent.Cast(pc.FindComponent(SCR_HUDManagerComponent));

		if (hudMgr)

			hudMgr.HMD_EnsureLaserDesignatorReadoutLayout();

	}



	//------------------------------------------------------------------------------------------------

	override protected void DisplayUpdate(IEntity owner, float timeSlice)

	{

		super.DisplayUpdate(owner, timeSlice);



		bool editorOpen = SCR_EditorManagerEntity.GetInstance().IsOpened();

		ChimeraWorld world = GetGame().GetWorld();

		bool showReadout = !editorOpen && m_wRoot && world && HUDMarkerVisibility.ShouldShowLaserDesignatorReadout();

		bool useVehicleTurretReadout = HUDMarkerVisibility.ShouldUseVehicleTurretLaserReadoutLayout();

		bool showHandheldReadout = showReadout && !useVehicleTurretReadout;

		bool showVehicleTurretReadout = showReadout && useVehicleTurretReadout;

		Widget readoutRoot = HMD_LaserDesignatorReadoutUI.GetRootWidget();

		if (readoutRoot)

			readoutRoot.SetVisible(showHandheldReadout);

		Widget vehReadoutRoot = HMD_LaserDesignatorReadoutUI.GetVehicleTurretRootWidget();

		if (vehReadoutRoot)

			vehReadoutRoot.SetVisible(showVehicleTurretReadout);

		HMD_RangefinderReadout.Apply(HMD_LaserDesignatorReadoutUI.GetRangeWidget(), HMD_LaserDesignatorReadoutUI.GetGridWidget(), HMD_LaserDesignatorReadoutUI.GetBearingWidget(), HMD_LaserDesignatorReadoutUI.GetCodeWidget(), showHandheldReadout, false);

		HMD_RangefinderReadout.Apply(HMD_LaserDesignatorReadoutUI.GetVehicleTurretRangeWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretGridWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretBearingWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretCodeWidget(), showVehicleTurretReadout, true);



		if (editorOpen)

			return;

		if (!m_wRoot || !world)

			return;



		PlayerController pc = GetGame().GetPlayerController();

		SCR_HUDManagerComponent hudMgr = null;

		if (pc)

			hudMgr = SCR_HUDManagerComponent.Cast(pc.FindComponent(SCR_HUDManagerComponent));

		if (!HUDMarkerVisibility.ShouldRenderWorldMarkers())

		{

			if (hudMgr)

				hudMgr.HMD_HideAllWorldMarkerDots();

			return;

		}

	}

}


