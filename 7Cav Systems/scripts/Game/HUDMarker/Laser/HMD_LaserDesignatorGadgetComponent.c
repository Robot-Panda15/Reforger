//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Gadget", description: "Rangefinder laser designator: WCS-compatible designation (camera trace) + HUD virtual marker.")]
class HMD_LaserDesignatorGadgetComponentClass : WCS_Armament_HandheldLaserDesignatorComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Extends WCS handheld: replaces designation trace in Update with camera-based trace (no WCS script edits).
//! HUD virtual marker + rangefinder readout in EOnFrame.
class HMD_LaserDesignatorGadgetComponent : WCS_Armament_HandheldLaserDesignatorComponent
{
	[Attribute("4000", UIWidgets.Slider, "Laser trace length (m)", "100 20000 100", category: "Laser")]
	protected float m_fLaserMaxRange;

	[Attribute("4000", UIWidgets.Slider, "HUD visibility / fade distance (m) for this marker", "100 20000 100", category: "Laser")]
	protected float m_fMarkerVisibilityDistance;

	[Attribute("0", UIWidgets.Slider, "Cap laser trace/HUD updates (Hz); 0 = every frame", "0 60 1", category: "Laser")]
	protected float m_fLaserUpdateRateHz;

	[Attribute("1", UIWidgets.CheckBox, "While zoomed through this gadget: draw IFF world dots (not gated by Numpad 9 or character prefab).", category: "HUD")]
	protected bool m_bHudIffWhileZoomed = true;

	[Attribute("1", UIWidgets.CheckBox, "While zoomed: draw this designator's own HUD laser designation dot.", category: "HUD")]
	protected bool m_bHudOwnLaserDesignationWhileZoomed = true;

	[Attribute("1", UIWidgets.CheckBox, "While zoomed: draw other players' laser designation dots.", category: "HUD")]
	protected bool m_bHudForeignLaserDesignationsWhileZoomed = true;

	[Attribute("0", UIWidgets.Slider, "Optional max distance (m) for world IFF/laser dots while zoomed; 0 = use each marker's limit only.", "0 20000 100", category: "HUD")]
	protected float m_fHudWorldMarkerDistanceClampM;

	[Attribute("0.25", UIWidgets.Slider, "Replay full server IFF table to this client while designator viewport is active (s).", "0.05 2 0.05", category: "HUD")]
	protected float m_fDesignatorViewportServerPollSec = 0.25;

	//! 1111-1199 targetable; 1200 non-target for weapon lock (still shown on HUD)
	[RplProp()]
	protected int m_iLaserCode = 1111;

	protected int m_iDesignationId = -1;
	protected float m_fLaserThrottleAccum = 0;

	protected static bool s_bHMDLaserInputListenersRegistered;

	protected static InputManager s_HMDGadgetInputManager;

	protected bool m_bPrevDesignatorViewportFrame;
	protected float m_fDesignatorViewportPollAccum;


	//! Last value sent with RpcAsk_SetDesignating so WCS lock sees IsDesignating when lasing via fire hold (not only toggle).
	protected bool m_bHmdLastEffectiveDesignating;

	//------------------------------------------------------------------------------------------------
	protected static IEntity HMD_ResolveLocalCharacterEntity()
	{
		IEntity main = SCR_PlayerController.GetLocalMainEntity();
		if (main)
			return main;
		return SCR_PlayerController.GetLocalControlledEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! Binocular / gadget ADS often does not feed the same action as infantry WeaponFire; check common fire bindings.
	protected static bool HMD_IsFireInputHeld(InputManager im)
	{
		if (!im)
			return false;
		if (im.GetActionValue("WeaponFire") > 0)
			return true;
		if (im.GetActionValue("GadgetFire") > 0)
			return true;
		if (im.GetActionValue("CharacterFire") > 0)
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Public for HMD weapon station / lock helpers
	int GetLaserCodeForWeaponSystems()
	{
		return m_iLaserCode;
	}

	//------------------------------------------------------------------------------------------------
	bool GetHudIffWhileZoomed()
	{
		return m_bHudIffWhileZoomed;
	}

	//------------------------------------------------------------------------------------------------
	bool GetHudOwnLaserDesignationWhileZoomed()
	{
		return m_bHudOwnLaserDesignationWhileZoomed;
	}

	//------------------------------------------------------------------------------------------------
	bool GetHudForeignLaserDesignationsWhileZoomed()
	{
		return m_bHudForeignLaserDesignationsWhileZoomed;
	}

	//------------------------------------------------------------------------------------------------
	float GetHudWorldMarkerDistanceClampM()
	{
		return m_fHudWorldMarkerDistanceClampM;
	}

	//------------------------------------------------------------------------------------------------
	float GetHudWorldMarkerUpdateRateHz()
	{
		return m_fLaserUpdateRateHz;
	}

	//------------------------------------------------------------------------------------------------
	//! Static so InputManager can RemoveActionListener after disconnect / reconnect.
	protected static void OnLaserDesignateAction(float value, EActionTrigger reason)
	{
		if (reason != EActionTrigger.DOWN)
			return;
		Game game = GetGame();
		if (!game || !game.InPlayMode())
			return;
		IEntity localChar = HMD_ResolveLocalCharacterEntity();
		if (!localChar)
			return;
		SCR_GadgetManagerComponent gm = SCR_GadgetManagerComponent.GetGadgetManager(localChar);
		if (!gm)
			return;
		IEntity held = gm.GetHeldGadget();
		if (!held)
			return;
		HMD_LaserDesignatorGadgetComponent comp = HMD_HandheldOpticZoom.FindHmdDesignatorOnGadget(held);
		if (!comp)
			return;
		comp.OnDesignateToggle();
	}

	//------------------------------------------------------------------------------------------------
	//! Keep RpcAsk_SetDesignating edge-sync aligned with toggle so fire-only lasing still re-Rpcs after toggle-off.
	override void OnDesignateToggle()
	{
		super.OnDesignateToggle();
		m_bHmdLastEffectiveDesignating = m_bIsDesignating;
	}

	//------------------------------------------------------------------------------------------------
	//! Held gadget first, then vehicle marking (turret 1311-1399 / camera 1111-1200). Called from global HUD input.
	static void ApplyLaserCodeDelta(int delta)
	{
		Game game = GetGame();
		if (!game || !game.InPlayMode())
			return;
		IEntity localChar = HMD_ResolveLocalCharacterEntity();
		if (!localChar)
			return;

		SCR_GadgetManagerComponent gm = SCR_GadgetManagerComponent.GetGadgetManager(localChar);
		if (gm)
		{
			IEntity held = gm.GetHeldGadget();
			if (held)
			{
				HMD_LaserDesignatorGadgetComponent comp = HMD_HandheldOpticZoom.FindHmdDesignatorOnGadget(held);
				if (comp)
				{
					comp.AdjustLaserCode(delta);
					return;
				}
			}
		}

		HMD_VehicleHUDLaserHelpers.TryAdjustVehicleMarkingLaserCode(delta);
	}

	//------------------------------------------------------------------------------------------------
	protected string GetLaserCodeName()
	{
		return string.Format("%1", m_iLaserCode);
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	protected void HMD_OnLaserCodeReplicated()
	{
		if (m_iDesignationId < 0)
			return;
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return;
		sys.UpdateDesignationName(m_iDesignationId, GetLaserCodeName());
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_ApplyLaserCodeDeltaAuthority(int delta)
	{
		int next = HMD_WrapLaserCode(m_iLaserCode + delta);
		if (next == m_iLaserCode)
			return;
		m_iLaserCode = next;
		if (Replication.IsRunning())
			Replication.BumpMe();
		if (m_iDesignationId < 0)
			return;
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return;
		sys.UpdateDesignationName(m_iDesignationId, GetLaserCodeName());
	}

	//------------------------------------------------------------------------------------------------
	protected void AdjustLaserCode(int delta)
	{
		if (!Replication.IsRunning())
		{
			HMD_ApplyLaserCodeDeltaAuthority(delta);
			return;
		}
		if (Replication.IsServer())
		{
			HMD_ApplyLaserCodeDeltaAuthority(delta);
			return;
		}
		Rpc(RpcAsk_AdjustLaserCode, delta);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AdjustLaserCode(int delta)
	{
		HMD_ApplyLaserCodeDeltaAuthority(delta);
	}

	//------------------------------------------------------------------------------------------------
	protected static int HMD_WrapLaserCode(int code)
	{
		if (code < 1111)
			return 1200;
		if (code > 1200)
			return 1111;
		return code;
	}

	//------------------------------------------------------------------------------------------------
	protected static void HMD_ClearGadgetInputListeners()
	{
		if (!s_HMDGadgetInputManager)
			return;
		s_HMDGadgetInputManager.RemoveActionListener("HMD_LaserDesignate", EActionTrigger.DOWN, OnLaserDesignateAction);
		s_HMDGadgetInputManager = null;
		s_bHMDLaserInputListenersRegistered = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_RegisterInputListenersOnce()
	{
		Game game = GetGame();
		if (!game || !game.InPlayMode())
			return;
		if (!HMD_ResolveLocalCharacterEntity())
		{
			HMD_ClearGadgetInputListeners();
			return;
		}
		InputManager im = game.GetInputManager();
		if (!im)
			return;
		if (s_bHMDLaserInputListenersRegistered && s_HMDGadgetInputManager == im)
			return;
		HMD_ClearGadgetInputListeners();
		im.AddActionListener("HMD_LaserDesignate", EActionTrigger.DOWN, OnLaserDesignateAction);
		s_HMDGadgetInputManager = im;
		s_bHMDLaserInputListenersRegistered = true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HMD_IsEntityUnderParent(IEntity ent, IEntity ancestor)
	{
		IEntity e = ent;
		int depth = 0;
		while (e && depth < 16)
		{
			if (e == ancestor)
				return true;
			e = e.GetParent();
			depth++;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLocalHoldingThisGadget(IEntity gadgetEntity, IEntity localChar)
	{
		if (!gadgetEntity)
			return false;
		if (!localChar)
			localChar = HMD_ResolveLocalCharacterEntity();
		if (!localChar)
			return false;

		SCR_GadgetManagerComponent gm = SCR_GadgetManagerComponent.GetGadgetManager(localChar);
		if (gm)
		{
			if (gm.GetHeldGadget() == gadgetEntity)
				return true;
			SCR_GadgetComponent heldComp = gm.GetHeldGadgetComponent();
			if (heldComp && heldComp.GetOwner() == gadgetEntity)
				return true;
		}
		CharacterControllerComponent ccc = null;
		ChimeraCharacter chForCcc = ChimeraCharacter.Cast(localChar);
		if (chForCcc)
			ccc = chForCcc.GetCharacterController();
		if (ccc)
		{
			if (ccc.GetAttachedGadgetAtLeftHandSlot() == gadgetEntity)
				return true;
			if (ccc.GetCurrentItemInHands() == gadgetEntity)
				return true;
			if (ccc.GetRightHandItem() == gadgetEntity)
				return true;
		}
		SCR_GadgetComponent gc = SCR_GadgetComponent.Cast(gadgetEntity.FindComponent(SCR_GadgetComponent));
		if (gc)
		{
			ChimeraCharacter gOwner = gc.GetCharacterOwner();
			ChimeraCharacter localAsCh = ChimeraCharacter.Cast(localChar);
			if (gOwner && localAsCh && gOwner == localAsCh)
			{
				if (gc.GetMode() == EGadgetMode.IN_HAND)
					return true;
			}
		}
		if (HMD_HandheldOpticZoom.IsZoomedForHMD() && HMD_IsEntityUnderParent(gadgetEntity, localChar))
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! World-marker distance clamp while designator viewport is active (matches zoom policy intent).
	protected float HMD_GetMarkerMaxDistanceMForViewport()
	{
		if (m_fHudWorldMarkerDistanceClampM > 0)
			return m_fHudWorldMarkerDistanceClampM;
		float a = m_fLaserMaxRange;
		float b = m_fMarkerVisibilityDistance;
		if (b > a)
			return b;
		return a;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: drive HMD_DesignatorViewportState + periodic server IFF replay while local player uses designator zoom with overlay eligibility.
	protected void HMD_UpdateDesignatorViewportForClient(ChimeraWorld world, float timeSlice, HUDMarkerSystem sys, bool holdingGadget)
	{
		if (!world || !sys)
			return;
		if (Replication.IsRunning() && !Replication.IsClient())
			return;

		bool inViewport = false;
		if (holdingGadget
			&& HMD_HandheldOpticZoom.IsZoomedForHMD()
			&& !HMD_HudMarkerEligibility.IsVehicleBinocularViewActive()
			&& HMD_HudMarkerEligibility.PassesHudMarkerWorldOverlayEligibility()
			&& HMD_HandheldOpticZoom.FindActiveLocalDesignatorComp() == this)
		{
			inViewport = true;
		}

		if (!inViewport)
		{
			if (HMD_DesignatorViewportState.IsActive())
				HMD_DesignatorViewportState.Deactivate();
			m_bPrevDesignatorViewportFrame = false;
			m_fDesignatorViewportPollAccum = 0;
			return;
		}

		float maxDist = HMD_GetMarkerMaxDistanceMForViewport();
		HMD_DesignatorViewportState.Activate(maxDist);

		float pollSec = m_fDesignatorViewportServerPollSec;
		if (pollSec < 0.05)
			pollSec = 0.05;

		if (!m_bPrevDesignatorViewportFrame)
		{
			m_fDesignatorViewportPollAccum = 0;
			HMD_HudMarkerIffClientSinkComponent sink = HMD_HudMarkerIffClientSinkComponent.GetLocalSink();
			if (sink)
				sink.ClientRequestDesignatorViewportSync();
		}
		else
		{
			m_fDesignatorViewportPollAccum += timeSlice;
			if (m_fDesignatorViewportPollAccum >= pollSec)
			{
				m_fDesignatorViewportPollAccum = 0;
				HMD_HudMarkerIffClientSinkComponent sink = HMD_HudMarkerIffClientSinkComponent.GetLocalSink();
				if (sink)
					sink.ClientRequestDesignatorViewportSync();
			}
		}

		m_bPrevDesignatorViewportFrame = true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLasingActive()
	{
		if (m_bIsDesignating)
			return true;
		InputManager im = GetGame().GetInputManager();
		return HMD_IsFireInputHeld(im);
	}

	//------------------------------------------------------------------------------------------------
	protected static vector ComputeLaserHitPos(BaseWorld world, vector start, vector dirNorm, float maxRange, IEntity gadgetEntity, IEntity localChar, out float traceFrac)
	{
		vector end = start + (dirNorm * maxRange);
		TraceParam trace = new TraceParam();
		trace.Start = start;
		trace.End = end;
		trace.Flags = TraceFlags.DEFAULT | TraceFlags.ANY_CONTACT;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		if (localChar)
			trace.Exclude = localChar.GetRootParent();
		ref array<IEntity> excl = {};
		if (gadgetEntity)
			excl.Insert(gadgetEntity);
		trace.ExcludeArray = excl;
		float frac = world.TraceMove(trace, null);
		if (frac < 0)
			frac = 0;
		if (frac > 1)
			frac = 1;
		traceFrac = frac;
		return start + (end - start) * frac;
	}

	//------------------------------------------------------------------------------------------------
	//! Camera-based designation (replaces WCS head/eye trace) - same replication path as PerformDesignation
	protected void HMD_CameraPerformDesignation(float timeSlice)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;

		IEntity characterOwner = owner.GetParent();
		if (!characterOwner)
			return;

		IEntity localChar = SCR_PlayerController.GetLocalMainEntity();
		if (!localChar || characterOwner != localChar)
			return;

		vector camTM[4];
		world.GetCurrentCamera(camTM);
		vector start = camTM[3];
		vector dir = camTM[2].Normalized();

		float traceFrac;
		vector hitPos = ComputeLaserHitPos(world, start, dir, m_fLaserMaxRange, owner, localChar, traceFrac);

		if (traceFrac > 0 && traceFrac < 0.9999)
		{
			if (m_RplComponent)
			{
				if (IsMaster())
				{
					SetDesignationPosition(hitPos);
				}
				else
				{
					Rpc(RpcAsk_SetDesignationPosition, hitPos);
				}
			}
			else
			{
				SetDesignationPosition(hitPos);
			}
		}
		else
		{
			if (m_RplComponent)
			{
				if (IsMaster())
				{
					m_bHasValidDesignation = false;
					Replication.BumpMe();
				}
				else
				{
					Rpc(RpcAsk_SetDesignationInvalid);
				}
			}
			else
			{
				m_bHasValidDesignation = false;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDesignatorLowered()
	{
		m_bHmdLastEffectiveDesignating = false;
		super.OnDesignatorLowered();
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces WCS::Update (head-aim PerformDesignation). Do NOT call SCR_GadgetComponent.Update(this) or super.Update(WCS):
	//! virtual dispatch re-enters HMD::Update and overflows the stack.
	override void Update(float timeSlice)
	{
		IEntity owner = GetOwner();
		InputManager im = GetGame().GetInputManager();
		bool fireHeld = HMD_IsFireInputHeld(im);
		bool effectiveLasing = m_bIsDesignating || fireHeld;

		if (owner)
		{
			RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
			if (rpl && rpl.IsOwner() && effectiveLasing != m_bHmdLastEffectiveDesignating)
			{
				m_bHmdLastEffectiveDesignating = effectiveLasing;
				Rpc(RpcAsk_SetDesignating, effectiveLasing);
			}
		}

		if (!effectiveLasing)
			return;

		if (!owner)
			return;

		IEntity characterOwner = owner.GetParent();
		if (!characterOwner)
			return;

		IEntity localPlayer = SCR_PlayerController.GetLocalMainEntity();
		if (!localPlayer || characterOwner != localPlayer)
			return;

		m_fUpdateTimer += timeSlice;
		if (m_fUpdateTimer >= m_fUpdateInterval)
		{
			m_fUpdateTimer = 0;
			HMD_CameraPerformDesignation(timeSlice);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! HMD uses global one-shot listeners (HMD_RegisterInputListenersOnce); do not duplicate WCS toggle here.
	override void RegisterActions()
	{
	}

	//------------------------------------------------------------------------------------------------
	override void UnRegisterActions()
	{
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (m_iLaserCode < 1111 || m_iLaserCode > 1200)
			m_iLaserCode = 1111;
		HMD_ApplyBinocularDesignationConfigFromCharacter();
		if (owner)
			SetEventMask(owner, EntityEvent.FRAME | owner.GetEventMask());
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_ApplyBinocularDesignationConfigFromCharacter()
	{
		IEntity ch = HMD_ResolveLocalCharacterEntity();
		if (!ch)
			return;
		HMD_LaserDesignationBinocularComponent cfg = HMD_LaserDesignationBinocularComponent.Cast(ch.FindComponent(HMD_LaserDesignationBinocularComponent));
		if (!cfg)
			return;
		m_fLaserMaxRange = cfg.GetOwnDesignationDistanceM();
		m_fMarkerVisibilityDistance = cfg.GetOwnDesignationDistanceM();
		m_fLaserUpdateRateHz = cfg.GetDesignationUpdateRateHz();
		//! Default code is replicated (RplProp); only authority sets it here so clients are not reset by local cfg apply each init.
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			m_iLaserCode = HMD_LaserCodeRules.WrapHandheldRange(cfg.GetDefaultLaserCode());
			if (m_iLaserCode < 1111 || m_iLaserCode > 1200)
				m_iLaserCode = 1111;
			if (Replication.IsRunning())
				Replication.BumpMe();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		if (System.IsConsoleApp())
			return;
		HMD_RegisterInputListenersOnce();

		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		if (!owner)
			return;

		IEntity localChar = HMD_ResolveLocalCharacterEntity();
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
		{
			if (!HUDMarkerVisibility.IsVehicleLaserMarkingMode())
				HMD_RangefinderHUDState.Clear();
			return;
		}

		bool holdingGadget = IsLocalHoldingThisGadget(owner, localChar);

		HMD_UpdateDesignatorViewportForClient(world, timeSlice, sys, holdingGadget);

		if (!holdingGadget)
		{
			m_fLaserThrottleAccum = 0;
			if (!HUDMarkerVisibility.IsVehicleLaserMarkingMode())
				HMD_RangefinderHUDState.Clear();
			if (m_iDesignationId >= 0)
			{
				sys.UnregisterDesignation(m_iDesignationId);
				m_iDesignationId = -1;
			}
			return;
		}

		HMD_RangefinderHUDState.SetDesignatorCode(m_iLaserCode);

		bool lasingInput = IsLasingActive();

		if (!lasingInput)
		{
			m_fLaserThrottleAccum = 0;
			HMD_RangefinderHUDState.ClearLasingReadoutOnly();
			if (m_iDesignationId >= 0)
			{
				sys.UnregisterDesignation(m_iDesignationId);
				m_iDesignationId = -1;
			}
			return;
		}

		if (m_fLaserUpdateRateHz > 0)
		{
			float interval = 1.0 / m_fLaserUpdateRateHz;
			m_fLaserThrottleAccum += timeSlice;
			if (m_fLaserThrottleAccum < interval)
				return;
			m_fLaserThrottleAccum -= interval;
		}

		vector camTM[4];
		world.GetCurrentCamera(camTM);
		vector start = camTM[3];
		vector dir = camTM[2].Normalized();

		float traceFrac;
		vector hitPos = ComputeLaserHitPos(world, start, dir, m_fLaserMaxRange, owner, localChar, traceFrac);

		float rangeM = vector.Distance(start, hitPos);
		bool maxRangeExceeded = traceFrac >= 0.9999 || rangeM >= m_fLaserMaxRange - 0.25;

		string gridStr = "";
		float bearingDeg = 0;
		if (!maxRangeExceeded)
		{
			gridStr = HMD_RangefinderGeo.FormatEightDigitGrid(hitPos);
			bearingDeg = HMD_RangefinderGeo.BearingDegCameraToTarget(start, hitPos);
		}
		HMD_RangefinderHUDState.SetLasingReadout(rangeM, gridStr, bearingDeg, maxRangeExceeded);

		if (maxRangeExceeded)
		{
			if (m_iDesignationId >= 0)
			{
				sys.UnregisterDesignation(m_iDesignationId);
				m_iDesignationId = -1;
			}
			return;
		}

		int red = Color.FromRGBA(255, 0, 0, 255).PackToInt();
		int white = Color.FromRGBA(255, 255, 255, 255).PackToInt();

		if (m_iDesignationId < 0)
			m_iDesignationId = sys.RegisterDesignation(hitPos, GetLaserCodeName(), red, white, m_fMarkerVisibilityDistance);
		else
			sys.UpdateDesignation(m_iDesignationId, hitPos);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		HMD_DesignatorViewportState.Deactivate();
		if (GetGame().InPlayMode())
		{
			HMD_RangefinderHUDState.Clear();
			ChimeraWorld world = GetGame().GetWorld();
			if (world)
			{
				HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
				if (sys && m_iDesignationId >= 0)
					sys.UnregisterDesignation(m_iDesignationId);
			}
		}
		m_iDesignationId = -1;
		HMD_LaserLockState.ClearIfLockedDesignator(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	void ~HMD_LaserDesignatorGadgetComponent()
	{
		HMD_RangefinderHUDState.Clear();
		if (!GetGame() || !GetGame().InPlayMode())
			return;
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys || m_iDesignationId < 0)
			return;
		sys.UnregisterDesignation(m_iDesignationId);
	}
}
