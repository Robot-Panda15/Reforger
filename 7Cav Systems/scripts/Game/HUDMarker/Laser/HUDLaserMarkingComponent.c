//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "HUD", description: "Base: slot policy + vehicle laser marking (Numpad /). Extends WCS handheld designator for missiles + ALL_DESIGNATORS. Includes HUD weapon lock replication. Use HUDLaserCameraMarkingComponent or HUDLaserTurretMarkingComponent on the appropriate entity.")]

class HUDLaserMarkingComponentClass : WCS_Armament_HandheldLaserDesignatorComponentClass
{
}

//------------------------------------------------------------------------------------------------

//! Slot policy and local marking toggle; replicated laser state synced via RPC for non-gunners.
//! Also: WCS designator surface for weapon lock + AGM guidance.
//! HUD LSHIFT lock: see HMD_LaserLockState (client UI) -> ClientSyncLockedWorldFromHud -> RpcAsk_SetWeaponLaserLockState -> m_bWeaponLaserLockWorld (server) -> HMD_WcsLaserVehicleDesignatorBridge.TryGetHmdLaserTargetWorld.

class HUDLaserMarkingComponent : WCS_Armament_HandheldLaserDesignatorComponent
{
	[Attribute("", UIWidgets.EditBox, "Comma-separated compartment Unique names that may use marking. Empty = no seats.", category: "HUD")]
	protected string m_sMarkingSlotNames;

	protected ref array<string> m_aMarkingSlotNamesCache;
	protected bool m_bMarkingSlotNamesParsed;

	protected ref array<string> m_aDiscoveredSlotNames;
	protected bool m_bDiscoveredSlotsParsed;

	protected bool m_bLocalMarkingEnabled;

	protected int m_iDesignationId = -1;

	//! Network mirror: gunner updates server; RpcDo broadcast applies on non-owner clients.
	protected vector m_vRplLaserHitWorld;
	protected bool m_bRplLaserHitValid;
	protected int m_iRplDisplayLaserCode;

	[Attribute("4000", UIWidgets.Slider, "Laser trace length (m)", "100 20000 100", category: "Laser")]
	protected float m_fLaserMaxRange;

	[Attribute("0", UIWidgets.Slider, "Cap trace/HUD updates (Hz); 0 = every frame", "0 60 1", category: "Laser")]
	protected float m_fLaserUpdateRateHz;

	[Attribute("0", UIWidgets.CheckBox, "Ground Vehicle Codes (1200): iterate 1211-1299", category: "Laser")]
	protected bool m_bGroundVehicleCodes1200;

	[Attribute("0", UIWidgets.CheckBox, "Air Vehicle Codes (1300): iterate 1311-1399", category: "Laser")]
	protected bool m_bAirVehicleCodes1300;

	//! Current code in the selected band; not an inspector field.
	protected int m_iDisplayLaserCode;

	protected float m_fLaserThrottleAccum;

	//! Server: HUD LSHIFT lock world for missile guidance (replicated from owner client).
	protected bool m_bWeaponLaserLockActive;
	protected vector m_vWeaponLaserLockWorld;

	protected static bool s_bClientHadLockForRpc;
	protected static float s_fLockRpcThrottleAccum;
	protected static vector s_vClientLastLockRpcWorld;
	protected static HUDLaserMarkingComponent s_pLastLockRpcMarking;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (owner)
			m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		HMD_ClampInitialDisplayLaserCode();
		HMD_TryApplyDesignationConfig();
		if (owner)
			SetEventMask(owner, EntityEvent.FRAME | owner.GetEventMask());
	}

	//------------------------------------------------------------------------------------------------
	//! If both boxes checked, air wins. If neither, use HMD_DefaultUseAirVehicleCodes (camera = ground, turret = air).
	protected bool HMD_UseAirVehicleCodes()
	{
		if (m_bAirVehicleCodes1300 && m_bGroundVehicleCodes1200)
			return true;
		if (m_bAirVehicleCodes1300 && !m_bGroundVehicleCodes1200)
			return true;
		if (m_bGroundVehicleCodes1200 && !m_bAirVehicleCodes1300)
			return false;
		return HMD_DefaultUseAirVehicleCodes();
	}

	//------------------------------------------------------------------------------------------------
	protected bool HMD_DefaultUseAirVehicleCodes()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_ClampInitialDisplayLaserCode()
	{
		if (HMD_UseAirVehicleCodes())
		{
			if (m_iDisplayLaserCode < 1311 || m_iDisplayLaserCode > 1399)
				m_iDisplayLaserCode = 1311;
		}
		else
		{
			if (m_iDisplayLaserCode < 1211 || m_iDisplayLaserCode > 1299)
				m_iDisplayLaserCode = 1211;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Optional HMD_LaserDesignation* components on the same entity override inspector defaults.
	protected void HMD_TryApplyDesignationConfig()
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Distance passed to HUDMarkerSystem for local designation dot fade (world clamp also uses eligibility policy).
	protected float HMD_GetMarkerDotRegistrationVisibilityDistanceM()
	{
		return m_fLaserMaxRange;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HMD_VecNearEqual(vector a, vector b)
	{
		return vector.DistanceSq(a, b) < 0.000001;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets replicated fields only (no Bump). Server applies authoritative state for all clients.
	protected void HMD_InternallySetReplicatedLaser(vector hitPos, bool valid, int displayCode)
	{
		if (valid)
		{
			if (m_bRplLaserHitValid && HMD_VecNearEqual(m_vRplLaserHitWorld, hitPos) && m_iRplDisplayLaserCode == displayCode)
				return;
			m_vRplLaserHitWorld = hitPos;
			m_bRplLaserHitValid = true;
			m_iRplDisplayLaserCode = displayCode;
		}
		else
		{
			if (!m_bRplLaserHitValid)
				return;
			m_bRplLaserHitValid = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: RpcAsk server; server applies then RpcDo broadcast so non-owner clients get laser state.
	protected void HMD_PushReplicatedLaserHit(vector hitPos, bool valid, int displayCode)
	{
		IEntity ent = GetOwner();
		if (!ent)
			return;
		if (!m_RplComponent)
			m_RplComponent = RplComponent.Cast(ent.FindComponent(RplComponent));
		if (!m_RplComponent)
		{
			HMD_InternallySetReplicatedLaser(hitPos, valid, displayCode);
			Replication.BumpMe();
			return;
		}
		if (Replication.IsServer())
		{
			HMD_InternallySetReplicatedLaser(hitPos, valid, displayCode);
			Rpc(RpcDo_HMD_SyncVehicleLaserToClients, hitPos, valid, displayCode);
		}
		else
		{
			Rpc(RpcAsk_HMD_SetVehicleLaserReplication, hitPos, valid, displayCode);
		}
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_HMD_SetVehicleLaserReplication(vector hitPos, bool valid, int displayCode)
	{
		HMD_InternallySetReplicatedLaser(hitPos, valid, displayCode);
		Rpc(RpcDo_HMD_SyncVehicleLaserToClients, hitPos, valid, displayCode);
	}

	//------------------------------------------------------------------------------------------------
	//! Proxies receive hit/code here; owner skips (gunner uses local trace / server already set).
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcDo_HMD_SyncVehicleLaserToClients(vector hitPos, bool valid, int displayCode)
	{
		IEntity ent = GetOwner();
		if (!m_RplComponent && ent)
			m_RplComponent = RplComponent.Cast(ent.FindComponent(RplComponent));
		if (m_RplComponent && m_RplComponent.IsOwner())
			return;
		HMD_InternallySetReplicatedLaser(hitPos, valid, displayCode);
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_ApplyReplicatedLaserHud(IEntity owner, BaseWorld world, HUDMarkerSystem sys, float markerVisDistance)
	{
		if (!m_bRplLaserHitValid)
		{
			HMD_UnregisterDesignation();
			return;
		}
		string label = string.Format("%1", m_iRplDisplayLaserCode);
		vector hitPos = m_vRplLaserHitWorld;
		HMD_RegisterOrUpdateLocalDesignation(sys, hitPos, label, markerVisDistance, HMD_MarkerVisuals.KIND_FOREIGN_DESIGNATION);
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_UnregisterDesignation()
	{
		if (m_iDesignationId < 0)
			return;
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
		{
			m_iDesignationId = -1;
			return;
		}
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (sys)
			sys.UnregisterDesignation(m_iDesignationId);
		m_iDesignationId = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Red circle = own (local gunner); green + KIND_FOREIGN_DESIGNATION = others' replicated vehicle laser (HUD pool only).
	protected void HMD_RegisterOrUpdateLocalDesignation(HUDMarkerSystem sys, vector hitPos, string label, float visDist, int visualKind = -1)
	{
		int white = Color.FromRGBA(255, 255, 255, 255).PackToInt();
		int markerCol = Color.FromRGBA(255, 0, 0, 255).PackToInt();
		if (visualKind == HMD_MarkerVisuals.KIND_FOREIGN_DESIGNATION)
			markerCol = Color.FromRGBA(0, 255, 0, 255).PackToInt();
		if (m_iDesignationId < 0)
			m_iDesignationId = sys.RegisterDesignation(hitPos, label, markerCol, white, visDist, visualKind);
		else
		{
			sys.UpdateDesignation(m_iDesignationId, hitPos);
			sys.UpdateDesignationName(m_iDesignationId, label);
			sys.UpdateDesignationVisualKind(m_iDesignationId, visualKind);
			sys.UpdateDesignationMarkerColors(m_iDesignationId, markerCol, white);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_SyncDesignationLabelFromDisplayCode(int displayCode)
	{
		if (m_iDesignationId < 0)
			return;
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return;
		sys.UpdateDesignationName(m_iDesignationId, string.Format("%1", displayCode));
	}

	//------------------------------------------------------------------------------------------------
	protected int HMD_WrapVehicleMarkingCode(int candidate)
	{
		if (HMD_UseAirVehicleCodes())
			return HMD_LaserCodeRules.WrapAirVehicleMarking(candidate);
		return HMD_LaserCodeRules.WrapGroundVehicleMarking(candidate);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectHudLaserMarkingComponentsInHierarchy(IEntity ent, notnull array<HUDLaserMarkingComponent> outMarks)
	{
		HMD_VehicleHUDLaserHelpers.CollectHudLaserMarkingComponentsInHierarchy(ent, outMarks);
	}

	//------------------------------------------------------------------------------------------------
	//! ResolveVehicleHUDMarkingRoot only walks parents; marking on a child (e.g. turret) needs a vehicle-wide search.
	static HUDLaserMarkingComponent FindMarkingComponentForVehicleSlot(BaseCompartmentSlot slot)
	{
		if (!slot || !slot.GetOwner())
			return null;
		IEntity markRoot = HMD_VehicleHUDLaserHelpers.ResolveVehicleHUDMarkingRoot(slot.GetOwner());
		HUDLaserMarkingComponent m = HMD_VehicleHUDLaserHelpers.FindMarkingComponentOnEntity(markRoot);
		if (m && m.IsLocalSlotAllowed(slot))
			return m;
		IEntity vehicleRoot = slot.GetOwner().GetRootParent();
		if (!vehicleRoot)
			vehicleRoot = slot.GetOwner();
		ref array<HUDLaserMarkingComponent> marks = {};
		CollectHudLaserMarkingComponentsInHierarchy(vehicleRoot, marks);
		int i;
		for (i = 0; i < marks.Count(); i++)
		{
			HUDLaserMarkingComponent cand = marks[i];
			if (cand && cand.IsLocalSlotAllowed(slot))
				return cand;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsMarkingEnabledForVehicleSlot(BaseCompartmentSlot slot)
	{
		if (!slot)
			return false;
		return FindMarkingComponentForVehicleSlot(slot) != null;
	}

	//------------------------------------------------------------------------------------------------
	bool GetLocalMarkingEnabled()
	{
		return m_bLocalMarkingEnabled;
	}

	//------------------------------------------------------------------------------------------------
	//! Read replicated gunner laser (server) for missile designation bridge.
	bool GetReplicatedLaserHitValid()
	{
		return m_bRplLaserHitValid;
	}

	int GetReplicatedLaserDisplayCode()
	{
		return m_iRplDisplayLaserCode;
	}

	vector GetReplicatedLaserHitWorld()
	{
		return m_vRplLaserHitWorld;
	}

	//------------------------------------------------------------------------------------------------
	protected void ParseMarkingSlotNames()
	{
		if (m_bMarkingSlotNamesParsed)
			return;
		m_bMarkingSlotNamesParsed = true;
		if (!m_aMarkingSlotNamesCache)
			m_aMarkingSlotNamesCache = new array<string>();
		HMD_VehicleHudSlotNamePolicy.ParseCommaSlotList(m_sMarkingSlotNames, m_aMarkingSlotNamesCache);
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureDiscoveredCompartmentNames()
	{
		if (m_bDiscoveredSlotsParsed)
			return;
		m_bDiscoveredSlotsParsed = true;
		if (!m_aDiscoveredSlotNames)
			m_aDiscoveredSlotNames = new array<string>();
		HMD_VehicleHudSlotNamePolicy.EnsureDiscoveredCompartmentNames(GetOwner(), m_aDiscoveredSlotNames);
	}

	//------------------------------------------------------------------------------------------------
	bool IsLocalSlotAllowed(BaseCompartmentSlot slot)
	{
		ParseMarkingSlotNames();
		if (!m_aMarkingSlotNamesCache || m_aMarkingSlotNamesCache.Count() == 0)
			return false;
		EnsureDiscoveredCompartmentNames();
		return HMD_VehicleHudSlotNamePolicy.EvaluateSlotAgainstLists(slot, m_aMarkingSlotNamesCache, m_aDiscoveredSlotNames);
	}

	//------------------------------------------------------------------------------------------------
	void RequestToggleLocalMarking(IEntity localCharacter, BaseCompartmentSlot slot)
	{
		if (!localCharacter || !slot)
			return;
		if (FindMarkingComponentForVehicleSlot(slot) != this)
			return;
		bool wasOff = !m_bLocalMarkingEnabled;
		m_bLocalMarkingEnabled = !m_bLocalMarkingEnabled;
		if (m_bLocalMarkingEnabled && wasOff)
		{
			HMD_LaserLockState.ClearAll();
			HMD_HUDLaserInput.ResetLockCycleIndex();
		}
		if (!m_bLocalMarkingEnabled)
			HMD_OnLocalMarkingTurnedOff();
		Rpc(RpcAsk_HMD_SetLocalMarkingEnabledOnServer, m_bLocalMarkingEnabled);
	}

	//------------------------------------------------------------------------------------------------
	//! Turn off own marking laser without toggling (e.g. when acquiring a designation lock).
	void ForceDisableLocalMarking()
	{
		if (!m_bLocalMarkingEnabled)
			return;
		m_bLocalMarkingEnabled = false;
		HMD_OnLocalMarkingTurnedOff();
		Rpc(RpcAsk_HMD_SetLocalMarkingEnabledOnServer, false);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_HMD_SetLocalMarkingEnabledOnServer(bool enabled)
	{
		m_bLocalMarkingEnabled = enabled;
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_OnLocalMarkingTurnedOff()
	{
		if (m_iDesignationId >= 1)
		{
			ChimeraWorld world = GetGame().GetWorld();
			if (world)
			{
				HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
				if (sys)
				{
					vector pos;
					if (sys.TryGetDesignationWorldPositionById(m_iDesignationId, pos))
					{
						string nm = "";
						sys.TryGetDesignationNameById(m_iDesignationId, nm);
						HMD_LaserLockState.MigrateHudLockBeforeUnregisterLocalDesignation(m_iDesignationId, pos, nm, GetReplicatedLaserDisplayCode());
					}
				}
			}
		}
		HMD_PushReplicatedLaserHit(vector.Zero, false, 0);
		HMD_UnregisterDesignation();
		HMD_LaserMarkingTraceUtils.ClearVehicleMarkingReadout();
	}

	//------------------------------------------------------------------------------------------------
	//! HUDLaserTurretMarkingComponent: HMD helmet gate outside gunner optic. Camera marking: always true.
	protected bool HMD_PassesTurretLaserOutsideCameraHmdGate()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Default: camera LOS. Turret overrides for bone + fallback.
	protected bool HMD_GetLaserAim(IEntity owner, BaseWorld world, out vector outStart, out vector outDirNorm)
	{
		if (!owner || !world)
			return false;
		vector camTM[4];
		world.GetCurrentCamera(camTM);
		outStart = camTM[3];
		outDirNorm = camTM[2].Normalized();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void AdjustLaserCode(int delta)
	{
		int next = HMD_WrapVehicleMarkingCode(m_iDisplayLaserCode + delta);
		if (next == m_iDisplayLaserCode)
			return;
		m_iDisplayLaserCode = next;
		HMD_RangefinderHUDState.SetDesignatorCode(m_iDisplayLaserCode);
		HMD_SyncDesignationLabelFromDisplayCode(m_iDisplayLaserCode);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		if (System.IsConsoleApp())
			return;
		if (!GetGame().InPlayMode())
			return;
		if (!owner)
			return;
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return;
		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();
		bool bLocalMarkingThisVehicle = false;
		if (localChar && GetLocalMarkingEnabled())
		{
			SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));
			BaseCompartmentSlot slot = null;
			if (cac)
				slot = cac.GetCompartment();
			if (cac && slot && IsLocalSlotAllowed(slot) && HMD_VehicleHUDLaserHelpers.ResolveVehicleHUDMarkingRoot(slot.GetOwner()) == owner)
				bLocalMarkingThisVehicle = true;
		}
		if (!bLocalMarkingThisVehicle)
		{
			HMD_ApplyReplicatedLaserHud(owner, world, sys, HMD_GetMarkerDotRegistrationVisibilityDistanceM());
			return;
		}
		if (!localChar || !GetLocalMarkingEnabled())
		{
			m_fLaserThrottleAccum = 0;
			HMD_UnregisterDesignation();
			HMD_LaserMarkingTraceUtils.ClearVehicleMarkingReadout();
			return;
		}
		SCR_CompartmentAccessComponent cac2 = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));
		if (!cac2)
		{
			HMD_UnregisterDesignation();
			HMD_LaserMarkingTraceUtils.ClearVehicleMarkingReadout();
			return;
		}
		BaseCompartmentSlot slot2 = cac2.GetCompartment();
		if (!slot2 || !IsLocalSlotAllowed(slot2))
		{
			HMD_UnregisterDesignation();
			HMD_LaserMarkingTraceUtils.ClearVehicleMarkingReadout();
			return;
		}
		if (HMD_VehicleHUDLaserHelpers.ResolveVehicleHUDMarkingRoot(slot2.GetOwner()) != owner)
		{
			HMD_UnregisterDesignation();
			HMD_LaserMarkingTraceUtils.ClearVehicleMarkingReadout();
			return;
		}
		if (!HMD_PassesTurretLaserOutsideCameraHmdGate())
		{
			m_fLaserThrottleAccum = 0;
			HMD_UnregisterDesignation();
			HMD_LaserMarkingTraceUtils.ClearVehicleMarkingReadout();
			HMD_PushReplicatedLaserHit(vector.Zero, false, 0);
			return;
		}
		bool bVehicleBinocular = HUDMarkerVisibility.IsVehicleBinocularViewActive();
		if (m_fLaserUpdateRateHz > 0)
		{
			float interval = 1.0 / m_fLaserUpdateRateHz;
			m_fLaserThrottleAccum += timeSlice;
			if (m_fLaserThrottleAccum < interval)
				return;
			m_fLaserThrottleAccum -= interval;
		}
		vector start;
		vector dir;
		if (!HMD_GetLaserAim(owner, world, start, dir))
			return;
		float traceFrac;
		vector hitPos = HMD_LaserMarkingTraceUtils.ComputeLaserHitPos(world, start, dir, m_fLaserMaxRange, owner, localChar, traceFrac);
		float rangeM = vector.Distance(start, hitPos);
		bool maxRangeExceeded = traceFrac >= 0.9999 || rangeM >= m_fLaserMaxRange - 0.25;
		if (bVehicleBinocular)
			HMD_LaserMarkingTraceUtils.ClearVehicleMarkingReadout();
		else
		{
			HMD_RangefinderHUDState.SetDesignatorCode(m_iDisplayLaserCode);
			string gridStr = "";
			float bearingDeg = 0;
			if (!maxRangeExceeded)
			{
				gridStr = HMD_RangefinderGeo.FormatEightDigitGrid(hitPos);
				bearingDeg = HMD_RangefinderGeo.BearingDegCameraToTarget(start, hitPos);
			}
			HMD_RangefinderHUDState.SetLasingReadout(rangeM, gridStr, bearingDeg, maxRangeExceeded);
		}
		if (maxRangeExceeded)
		{
			HMD_UnregisterDesignation();
			HMD_PushReplicatedLaserHit(vector.Zero, false, 0);
			return;
		}
		HMD_PushReplicatedLaserHit(hitPos, true, m_iDisplayLaserCode);
		string label = string.Format("%1", m_iDisplayLaserCode);
		HMD_RegisterOrUpdateLocalDesignation(sys, hitPos, label, HMD_GetMarkerDotRegistrationVisibilityDistanceM());
	}

	//------------------------------------------------------------------------------------------------
	//! WCS: AGM/missile cone uses this; HUD dots use HUDMarkerSystem pool (see HMD_MarkerVisuals).
	override void Update(float timeSlice)
	{
	}

	override vector GetDesignatedLocation()
	{
		if (!GetReplicatedLaserHitValid())
			return vector.Zero;
		return GetReplicatedLaserHitWorld();
	}

	override bool HasValidDesignation()
	{
		return GetReplicatedLaserHitValid();
	}

	override bool IsDesignating()
	{
		return HasValidDesignation();
	}

	int GetLaserCodeForWeaponSystems()
	{
		if (GetReplicatedLaserHitValid())
			return GetReplicatedLaserDisplayCode();
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	bool IsWeaponLaserLockActive()
	{
		return m_bWeaponLaserLockActive;
	}

	vector GetWeaponLaserLockWorld()
	{
		return m_vWeaponLaserLockWorld;
	}

	//------------------------------------------------------------------------------------------------
	static HUDLaserMarkingComponent FindForLocalGunnerVehicle(IEntity localChar)
	{
		if (!localChar)
			return null;
		SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));
		if (!cac)
			return null;
		BaseCompartmentSlot slot = cac.GetCompartment();
		if (!slot || !slot.GetOwner())
			return null;
		//! FindComponent(HUDLaserMarkingComponent) misses turret/camera subclasses; match slot policy first (same as marking toggle).
		HUDLaserMarkingComponent m = FindMarkingComponentForVehicleSlot(slot);
		if (m)
			return m;
		IEntity root = slot.GetOwner().GetRootParent();
		if (!root)
			root = slot.GetOwner();
		ref array<HUDLaserMarkingComponent> marks = {};
		HMD_VehicleHUDLaserHelpers.CollectHudLaserMarkingComponentsInHierarchy(root, marks);
		if (marks.Count() == 0)
			return null;
		return marks[0];
	}

	//------------------------------------------------------------------------------------------------
	void ClientPushLockState(bool active, vector worldPos)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;
		//! Do not require Rpl IsOwner on the marking entity; gunner client often is not network owner of the vehicle entity; RPC still reaches server.
		if (!Replication.IsRunning())
		{
			HMD_ApplyWeaponLaserLockStateToVehicleHierarchy(active, worldPos);
			return;
		}
		Rpc(RpcAsk_SetWeaponLaserLockState, active, worldPos);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: push HUD lock world position to server (throttled) so missiles use the locked designation.
	//! Runs from SCR_HUDManagerComponent.OnUpdate. Must run on listen-server hosts: they are IsServer and IsClient; old gate skipped all IsServer and never sent RpcAsk_SetWeaponLaserLockState.
	static void ClientSyncLockedWorldFromHud(float timeSlice)
	{
		if (!GetGame().InPlayMode())
			return;
		//! Dedicated headless: no local player; skip. Listen host / some Workbench builds report IsClient false while a local pilot exists; do not skip if GetLocalControlledEntity is set.
		if (Replication.IsRunning() && Replication.IsServer() && !Replication.IsClient())
		{
			IEntity localEarly = SCR_PlayerController.GetLocalControlledEntity();
			if (!localEarly)
				return;
		}
		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();
		if (!localChar)
			return;
		HUDLaserMarkingComponent comp = FindForLocalGunnerVehicle(localChar);
		if (!HMD_LaserLockState.IsLocked())
		{
			if (s_bClientHadLockForRpc)
			{
				HUDLaserMarkingComponent clearOn = comp;
				if (!clearOn)
					clearOn = s_pLastLockRpcMarking;
				if (clearOn)
					clearOn.ClientPushLockState(false, vector.Zero);
				s_bClientHadLockForRpc = false;
				s_fLockRpcThrottleAccum = 0;
				s_pLastLockRpcMarking = null;
			}
			return;
		}
		if (!comp)
			return;
		vector w;
		if (!HMD_LaserLockState.TryGetLockedTargetWorldPosition(w))
			return;
		s_fLockRpcThrottleAccum += timeSlice;
		float distSq = vector.DistanceSq(w, s_vClientLastLockRpcWorld);
		bool needPush = !s_bClientHadLockForRpc || s_fLockRpcThrottleAccum >= 0.2 || distSq >= 4.0;
		if (!needPush)
			return;
		s_bClientHadLockForRpc = true;
		s_fLockRpcThrottleAccum = 0;
		s_vClientLastLockRpcWorld = w;
		s_pLastLockRpcMarking = comp;
		comp.ClientPushLockState(true, w);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: apply lock to every HUD marking comp on this vehicle so TryGet finds it (RPC may land on turret vs hull child).
	protected void HMD_ApplyWeaponLaserLockStateToVehicleHierarchy(bool active, vector worldPos)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;
		IEntity root = owner.GetRootParent();
		if (!root)
			root = owner;
		ref array<HUDLaserMarkingComponent> marks = {};
		HMD_VehicleHUDLaserHelpers.CollectHudLaserMarkingComponentsInHierarchy(root, marks);
		if (marks.Count() == 0)
		{
			m_bWeaponLaserLockActive = active;
			if (active)
				m_vWeaponLaserLockWorld = worldPos;
			else
				m_vWeaponLaserLockWorld = vector.Zero;
			return;
		}
		int i;
		for (i = 0; i < marks.Count(); i++)
		{
			HUDLaserMarkingComponent markComp = marks[i];
			if (!markComp)
				continue;
			markComp.m_bWeaponLaserLockActive = active;
			if (active)
				markComp.m_vWeaponLaserLockWorld = worldPos;
			else
				markComp.m_vWeaponLaserLockWorld = vector.Zero;
		}
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetWeaponLaserLockState(bool active, vector worldPos)
	{
		HMD_ApplyWeaponLaserLockStateToVehicleHierarchy(active, worldPos);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame().InPlayMode())
			HMD_OnLocalMarkingTurnedOff();
		HMD_LaserLockState.ClearIfLockedDesignator(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	void ~HUDLaserMarkingComponent()
	{
		HMD_UnregisterDesignation();
	}
}
