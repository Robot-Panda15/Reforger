//------------------------------------------------------------------------------------------------
//! Extends the default HUD by adding marker dots via CreateLayout - no player controller override
modded class SCR_HUDManagerComponent
{
	protected Widget m_wLaserDesignationMarkerRoot;
	protected Widget m_wIffMarkerRoot;
	protected Widget m_wDesignatorReadoutRoot;
	protected Widget m_wVehicleTurretLaserReadoutRoot;
	protected bool m_bWasInVehicle = false;
	protected ref array<ImageWidget> m_aLaserDesignationMarkerDots = {};
	protected ref array<TextWidget> m_aLaserDesignationMarkerLabels = {};
	protected ref array<ImageWidget> m_aIffMarkerDots = {};
	protected ref array<TextWidget> m_aIffMarkerLabels = {};
	protected ref array<vector> m_HMDMarkerPositions = {};
	protected ref array<string> m_HMDMarkerNames = {};
	protected ref array<int> m_HMDMarkerDotColors = {};
	protected ref array<int> m_HMDMarkerLabelColors = {};
	protected ref array<float> m_HMDMarkerVisDist = {};
	protected ref array<int> m_HMDMarkerVisualKinds = {};
	protected const float MARKER_DOT_SIZE = 12.0;
	protected const int MARKER_POOL_SIZE = 32;

	//------------------------------------------------------------------------------------------------
	protected override void OnInit(IEntity owner)
	{
		super.OnInit(owner);
		HMD_LocalHudMarkerSession.ClearClientSessionState();
		CreateMarkerOverlay();
		HMD_EnsureLaserDesignatorReadoutLayout();
		HMD_HUDLaserInput.RegisterOnce();
	}

	//------------------------------------------------------------------------------------------------
	//! Public so HUDMarkerDisplay (InfoDisplay) can bind readout before first frame if needed.
	void HMD_EnsureLaserDesignatorReadoutLayout()
	{
		if (!m_wDesignatorReadoutRoot)
		{
			m_wDesignatorReadoutRoot = CreateLayout("{68E3221400000003}UI/layouts/HUD/HMD_LaserDesignatorReadout.layout", EHudLayers.MEDIUM, 0);
			if (m_wDesignatorReadoutRoot)
				HMD_LaserDesignatorReadoutUI.BindHandheldFromLayoutRoot(m_wDesignatorReadoutRoot);
		}
		if (!m_wVehicleTurretLaserReadoutRoot)
		{
			m_wVehicleTurretLaserReadoutRoot = CreateLayout("{68E3221400000004}UI/layouts/HUD/HMD_VehicleTurretLaserDesignatorReadout.layout", EHudLayers.MEDIUM, 0);
			if (m_wVehicleTurretLaserReadoutRoot)
				HMD_LaserDesignatorReadoutUI.BindVehicleTurretFromLayoutRoot(m_wVehicleTurretLaserReadoutRoot);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateMarkerOverlay()
	{
		if (!m_wLaserDesignationMarkerRoot)
		{
			m_wLaserDesignationMarkerRoot = CreateLayout("{68E3221400000005}UI/layouts/HUD/HMD_LaserDesignationOverlay.layout", EHudLayers.MEDIUM, 0);
			if (m_wLaserDesignationMarkerRoot)
				HUDMarkerDisplayHelper.FillMarkerPoolFromRoot(m_wLaserDesignationMarkerRoot, MARKER_POOL_SIZE, m_aLaserDesignationMarkerDots, m_aLaserDesignationMarkerLabels);
		}
		if (!m_wIffMarkerRoot)
		{
			m_wIffMarkerRoot = CreateLayout("{68E3221400000006}UI/layouts/HUD/HMD_IffMarkerOverlay.layout", EHudLayers.MEDIUM, 0);
			if (m_wIffMarkerRoot)
				HUDMarkerDisplayHelper.FillMarkerPoolFromRoot(m_wIffMarkerRoot, MARKER_POOL_SIZE, m_aIffMarkerDots, m_aIffMarkerLabels);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_HideAllMarkerDotsAndLabels()
	{
		HUDMarkerDisplayHelper.HideMarkerWidgetPool(m_aLaserDesignationMarkerDots, m_aLaserDesignationMarkerLabels);
		HUDMarkerDisplayHelper.HideMarkerWidgetPool(m_aIffMarkerDots, m_aIffMarkerLabels);
	}

	//------------------------------------------------------------------------------------------------
	//! Public: InfoDisplay path clears overlay when world markers are blocked.
	void HMD_HideAllWorldMarkerDots()
	{
		HMD_HideAllMarkerDotsAndLabels();
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnUpdate(IEntity owner)
	{
		super.OnUpdate(owner);
		HMD_HUDLaserInput.RegisterOnce();
		HUDLaserMarkingComponent.ClientSyncLockedWorldFromHud(0.016);
		if (!m_wLaserDesignationMarkerRoot || !m_wIffMarkerRoot)
			return;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;
		if (SCR_EditorManagerEntity.GetInstance().IsOpened())
		{
			if (m_wDesignatorReadoutRoot)
				m_wDesignatorReadoutRoot.SetVisible(false);
			if (m_wVehicleTurretLaserReadoutRoot)
				m_wVehicleTurretLaserReadoutRoot.SetVisible(false);
			HMD_RangefinderReadout.Apply(HMD_LaserDesignatorReadoutUI.GetRangeWidget(), HMD_LaserDesignatorReadoutUI.GetGridWidget(), HMD_LaserDesignatorReadoutUI.GetBearingWidget(), HMD_LaserDesignatorReadoutUI.GetCodeWidget(), false, false);
			HMD_RangefinderReadout.Apply(HMD_LaserDesignatorReadoutUI.GetVehicleTurretRangeWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretGridWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretBearingWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretCodeWidget(), false, true);
			return;
		}
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return;

		PlayerController pc = GetGame().GetPlayerController();
		SCR_ChimeraCharacter playerChar = null;
		if (pc)
			playerChar = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());
		if (!pc || !playerChar)
		{
			HMD_LocalHudMarkerSession.ClearClientSessionState();
			if (m_wDesignatorReadoutRoot)
				m_wDesignatorReadoutRoot.SetVisible(false);
			if (m_wVehicleTurretLaserReadoutRoot)
				m_wVehicleTurretLaserReadoutRoot.SetVisible(false);
			HMD_RangefinderReadout.Apply(HMD_LaserDesignatorReadoutUI.GetRangeWidget(), HMD_LaserDesignatorReadoutUI.GetGridWidget(), HMD_LaserDesignatorReadoutUI.GetBearingWidget(), HMD_LaserDesignatorReadoutUI.GetCodeWidget(), false, false);
			HMD_RangefinderReadout.Apply(HMD_LaserDesignatorReadoutUI.GetVehicleTurretRangeWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretGridWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretBearingWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretCodeWidget(), false, true);
			if (m_wLaserDesignationMarkerRoot)
				m_wLaserDesignationMarkerRoot.SetVisible(false);
			if (m_wIffMarkerRoot)
				m_wIffMarkerRoot.SetVisible(false);
			HMD_HideAllMarkerDotsAndLabels();
			return;
		}
		HMD_LaserLockState.RefreshLockedTargetReadout();
		SCR_CharacterControllerComponent charController = null;
		if (playerChar)
			charController = SCR_CharacterControllerComponent.Cast(playerChar.FindComponent(SCR_CharacterControllerComponent));
		if (charController)
		{
			ECharacterLifeState lifeState = charController.GetLifeState();
			if (lifeState == ECharacterLifeState.DEAD || lifeState == ECharacterLifeState.INCAPACITATED)
				HMD_LocalHudMarkerSession.ClearClientSessionState();
		}
		bool inVehicle = playerChar && playerChar.IsInVehicle();
		if (m_bWasInVehicle && !inVehicle)
			HMD_LocalHudMarkerSession.ClearClientSessionState();
		if (inVehicle)
		{
			if (charController && charController.GetLifeState() == ECharacterLifeState.ALIVE)
			{
				SyncVehicleLaserComponents(playerChar);
			}
			else
				HMD_LocalHudMarkerSession.ClearClientSessionState();
		}
		else
		{
			HUDMarkerVisibility.ClearVehicleLaserModes();
		}
		m_bWasInVehicle = inVehicle;

		bool showWorldMarkers = HUDMarkerVisibility.ShouldRenderWorldMarkers();
		bool showLaserReadout = HUDMarkerVisibility.ShouldShowLaserDesignatorReadout();
		bool useVehicleTurretReadout = HUDMarkerVisibility.ShouldUseVehicleTurretLaserReadoutLayout();
		bool showHandheldReadout = showLaserReadout && !useVehicleTurretReadout;
		bool showVehicleTurretReadout = showLaserReadout && useVehicleTurretReadout;
		if (m_wLaserDesignationMarkerRoot)
			m_wLaserDesignationMarkerRoot.SetVisible(showWorldMarkers);
		if (m_wIffMarkerRoot)
			m_wIffMarkerRoot.SetVisible(showWorldMarkers);
		if (m_wDesignatorReadoutRoot)
			m_wDesignatorReadoutRoot.SetVisible(showHandheldReadout);
		if (m_wVehicleTurretLaserReadoutRoot)
			m_wVehicleTurretLaserReadoutRoot.SetVisible(showVehicleTurretReadout);
		HMD_RangefinderReadout.Apply(HMD_LaserDesignatorReadoutUI.GetRangeWidget(), HMD_LaserDesignatorReadoutUI.GetGridWidget(), HMD_LaserDesignatorReadoutUI.GetBearingWidget(), HMD_LaserDesignatorReadoutUI.GetCodeWidget(), showHandheldReadout, false);
		HMD_RangefinderReadout.Apply(HMD_LaserDesignatorReadoutUI.GetVehicleTurretRangeWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretGridWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretBearingWidget(), HMD_LaserDesignatorReadoutUI.GetVehicleTurretCodeWidget(), showVehicleTurretReadout, true);

		if (!showWorldMarkers)
		{
			HMD_HideAllMarkerDotsAndLabels();
			return;
		}
		if (m_aLaserDesignationMarkerDots.IsEmpty() || m_aIffMarkerDots.IsEmpty())
			return;

		HUDMarkerDisplayHelper.FetchAndRenderWorldMarkersFromSystem(sys, m_HMDMarkerPositions, m_HMDMarkerNames, m_HMDMarkerDotColors, m_HMDMarkerLabelColors, m_HMDMarkerVisDist, m_HMDMarkerVisualKinds, world, workspace, m_aLaserDesignationMarkerDots, m_aLaserDesignationMarkerLabels, m_aIffMarkerDots, m_aIffMarkerLabels, MARKER_DOT_SIZE);
	}

	//------------------------------------------------------------------------------------------------
	protected void SyncVehicleLaserComponents(SCR_ChimeraCharacter playerChar)
	{
		if (!playerChar)
		{
			HUDMarkerVisibility.ClearVehicleLaserModes();
			return;
		}
		SCR_CompartmentAccessComponent compartmentAccess = SCR_CompartmentAccessComponent.Cast(playerChar.FindComponent(SCR_CompartmentAccessComponent));
		if (!compartmentAccess)
		{
			HUDMarkerVisibility.ClearVehicleLaserModes();
			return;
		}
		BaseCompartmentSlot compartment = compartmentAccess.GetCompartment();
		if (!compartment)
		{
			HUDMarkerVisibility.ClearVehicleLaserModes();
			return;
		}
		IEntity slotOwner = compartment.GetOwner();
		if (!slotOwner)
		{
			HUDMarkerVisibility.ClearVehicleLaserModes();
			return;
		}
		if (HUDMarkerVisibility.IsVehicleBinocularViewActive())
		{
			HUDMarkerVisibility.ClearVehicleLaserModes();
			return;
		}

		IEntity visRoot = HMD_VehicleHUDLaserHelpers.ResolveVehicleHUDVisibilityRoot(slotOwner);
		HUDLaserVisibilityComponent vis = HUDLaserVisibilityComponent.Cast(visRoot.FindComponent(HUDLaserVisibilityComponent));
		if (vis && HUDLaserVisibilityComponent.IsVisibilityEnabledForVehicleSlot(compartment))
			HUDMarkerVisibility.SetVehicleLaserVisibilityEnabled(vis.GetLocalVisibilityEnabled());
		else
			HUDMarkerVisibility.SetVehicleLaserVisibilityEnabled(false);

		IEntity markRoot = HMD_VehicleHUDLaserHelpers.ResolveVehicleHUDMarkingRoot(slotOwner);
		HUDLaserMarkingComponent mark = HMD_VehicleHUDLaserHelpers.FindMarkingComponentOnEntity(markRoot);
		if (mark && HUDLaserMarkingComponent.IsMarkingEnabledForVehicleSlot(compartment))
			HUDMarkerVisibility.SetVehicleLaserMarkingMode(mark.GetLocalMarkingEnabled());
		else
			HUDMarkerVisibility.SetVehicleLaserMarkingMode(false);
	}
}
