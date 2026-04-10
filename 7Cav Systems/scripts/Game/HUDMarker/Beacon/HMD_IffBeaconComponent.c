//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "IFF beacon: IR + dynamic label. HUD dot via HMD_PlacedDesignationComponent (visual kind 0) on same entity, or RegisterIffMarker when no designation sibling. Warheads: live on spawn. Attachable: placement + pickup gates.")]
class HMD_IffBeaconComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Server-authoritative beacon state; clients refresh IFF pool from RplProp callbacks (RegisterIffMarker path).
//! Base: no deployment gate; attachable overrides DeploymentGateAllowsIff and AuthorityTickDeploymentGates.
//! Text/number use SCR_AdjustSignalAction (hold interact + scroll) while beacon is OFF.
//! IR: local SpawnEntityPrefabEx(..., false) per machine; prefab from m_sIrLightPrefab (empty = no IR).
//! Dynamic spawn may run OnPostInit before InPlayMode; m_bPendingPlayModeRefresh + CallLater/EOnFrame redo refresh.
//! Attachable subclass sets m_bUnplaceWhenInInventorySlot before super.OnPostInit.
//! With HMD_PlacedDesignationComponent on self or a child: designation owns the HUD row (RegisterDesignation); IFF pool is not used. Child HUD proxy under attachable prefab gates from ShouldShowIffOnHud() in HMD_PlacedDesignationComponent. Legacy: HUDMarkerComponent + RegisterIffMarker + HMD_ResolveIffPoolPresentation.
class HMD_IffBeaconComponent : ScriptComponent
{
	protected static const float BEACON_TOTAL_SECONDS = 1800;
	protected static const int TEXT_COUNT = 5;

	[Attribute(defvalue: "{0BCD51DD36B82132}Prefabs/Items/Equipment/Nightvision/IFF_IR_Light.et", UIWidgets.ResourceNamePicker, "RHS_LightEntity prefab spawned locally while beacon transmits; empty = no IR light.", "et", category: "HMD")]
	protected ResourceName m_sIrLightPrefab;

	[Attribute("1500", UIWidgets.Auto, "IR strobe ON duration (ms)", category: "HMD")]
	protected float m_fIrStrobeOnMs;

	[Attribute("500", UIWidgets.Auto, "IR strobe OFF duration (ms)", category: "HMD")]
	protected float m_fIrStrobeOffMs;

	[Attribute("0", UIWidgets.Auto, "Delay before first IR light spawn (ms) after beacon starts transmitting; 0 = immediate.", category: "HMD")]
	protected float m_fIrLightSpawnDelayMs;

	[Attribute("0 0 0", UIWidgets.EditBox, "World-space offset (m) from beacon origin for IR light position (e.g. 0 0.5 0 = half meter along world +Y).", category: "HMD")]
	protected vector m_vIrLightWorldOffset;

	[RplProp(onRplName: "OnBeaconStateReplicated")]
	protected bool m_bBeaconActive;

	[RplProp(onRplName: "OnBeaconStateReplicated")]
	protected int m_iTextIndex;

	[RplProp(onRplName: "OnBeaconStateReplicated")]
	protected int m_iNumber;

	[RplProp(onRplName: "OnBeaconStateReplicated")]
	protected float m_fBatterySecondsRemaining;

	[RplProp(onRplName: "OnPlacedStateReplicated")]
	protected bool m_bPlacedInWorld;

	//! HMD_IffBeaconComponentAttachable sets true before super.OnPostInit.
	protected bool m_bUnplaceWhenInInventorySlot;

	//! True after authority init or any RplProp callback for this beacon. Until then, warheads mirror HMD_PlacedDesignationComponent (pool row from world, no wait for m_bBeaconActive).
	protected bool m_bSeenAnyBeaconRplSnapshot;

	protected float m_fBatteryBumpAccum;
	protected bool m_bBatteryDrainServerTimeSet;
	protected WorldTimestamp m_ServerTimeLastBatteryDrain;
	protected static const float BATTERY_DRAIN_DT_CAP = 5.0;

	protected IEntity m_pSpawnedIrLight;
	protected bool m_bIrLightSpawnDelayPending;

	//! Pooled IFF row id (RegisterIffMarker / UnregisterIffMarker), same pattern as HMD_PlacedDesignationComponent.m_iDesignationId.
	protected int m_iIffMarkerPoolId = -1;

	//! OnPostInit ran before InPlayMode (dynamic spawn / load); run RefreshHudRegistration once play is active.
	protected bool m_bPendingPlayModeRefresh;
	protected int m_iPlayModeRefreshPolls;
	protected static const int PLAY_MODE_REFRESH_POLL_MAX = 400;

	//------------------------------------------------------------------------------------------------
	protected static vector IrWorldOffsetToLocal(vector worldOff, vector parentWorld[4])
	{
		vector localOff;
		localOff[0] = parentWorld[0][0] * worldOff[0] + parentWorld[1][0] * worldOff[1] + parentWorld[2][0] * worldOff[2];
		localOff[1] = parentWorld[0][1] * worldOff[0] + parentWorld[1][1] * worldOff[1] + parentWorld[2][1] * worldOff[2];
		localOff[2] = parentWorld[0][2] * worldOff[0] + parentWorld[1][2] * worldOff[1] + parentWorld[2][2] * worldOff[2];
		return localOff;
	}

	//------------------------------------------------------------------------------------------------
	static HMD_IffBeaconComponent FindOnEntity(IEntity owner)
	{
		if (!owner)
			return null;
		//! Attachable subclass is returned by base-type lookup (no forward decl to deprecated subclass type).
		return HMD_IffBeaconComponent.Cast(owner.FindComponent(HMD_IffBeaconComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShouldUnplaceWhenInInventorySlot()
	{
		return m_bUnplaceWhenInInventorySlot;
	}

	//------------------------------------------------------------------------------------------------
	//! Attachable subclass overrides to require world placement; base warheads always allow IFF/IR/HUD once active + battery.
	protected bool DeploymentGateAllowsIff()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Attachable override: clear `m_bPlacedInWorld` when the item returns to an inventory slot.
	protected void AuthorityTickDeploymentGates(IEntity owner)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! First HMD_PlacedDesignationComponent under `root` (children only; not `root` itself).
	protected static HMD_PlacedDesignationComponent HMD_FindFirstPlacedDesignationInDescendants(IEntity root)
	{
		if (!root)
			return null;
		IEntity ch = root.GetChildren();
		while (ch)
		{
			HMD_PlacedDesignationComponent pd = HMD_PlacedDesignationComponent.Cast(ch.FindComponent(HMD_PlacedDesignationComponent));
			if (pd)
				return pd;
			pd = HMD_FindFirstPlacedDesignationInDescendants(ch);
			if (pd)
				return pd;
			ch = ch.GetSibling();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasSiblingDesignationHud(IEntity owner)
	{
		if (!owner)
			return false;
		if (HMD_PlacedDesignationComponent.Cast(owner.FindComponent(HMD_PlacedDesignationComponent)))
			return true;
		return HMD_FindFirstPlacedDesignationInDescendants(owner) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected float GetEffectiveBatteryForTransmitGate()
	{
		float b = m_fBatterySecondsRemaining;
		if (!ShouldUnplaceWhenInInventorySlot() && b <= 0)
			return 1;
		return b;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the IffMarker pool row should exist: attachables use Rpl active; warheads show until first Rpl snapshot then follow m_bBeaconActive.
	protected bool IffPooledRowShouldShow()
	{
		if (ShouldUnplaceWhenInInventorySlot())
			return m_bBeaconActive;
		if (!m_bSeenAnyBeaconRplSnapshot)
			return true;
		return m_bBeaconActive;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsIrTransmitting()
	{
		float bat = GetEffectiveBatteryForTransmitGate();
		if (bat <= 0)
			return false;
		if (!DeploymentGateAllowsIff())
			return false;
		return IffPooledRowShouldShow();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAuthorityTransmitting()
	{
		return DeploymentGateAllowsIff() && m_bBeaconActive && GetEffectiveBatteryForTransmitGate() > 0;
	}

	//------------------------------------------------------------------------------------------------
	protected string BuildMarkerLabel()
	{
		string t = "HLS";
		switch (m_iTextIndex)
		{
			case 0: { t = "HLS"; break; }
			case 1: { t = "VKY"; break; }
			case 2: { t = "BSH"; break; }
			case 3: { t = "BND"; break; }
			case 4: { t = "MSF"; break; }
		}
		int n = m_iNumber;
		if (n < 1)
			n = 1;
		if (n > 7)
			n = 7;
		return string.Format("%1-%2", t, n);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the IFF HUD dot should show: placed (if attachable), pooled visibility rules, and battery.
	bool ShouldShowIffOnHud()
	{
		return DeploymentGateAllowsIff() && IffPooledRowShouldShow() && GetEffectiveBatteryForTransmitGate() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Label under the IFF HUD dot (text index + number).
	string GetMarkerLabelForHud()
	{
		return BuildMarkerLabel();
	}

	//------------------------------------------------------------------------------------------------
	protected int HMD_BumpPlacedDesignationHudCachesInHierarchy(IEntity node)
	{
		int n = 0;
		if (!node)
			return 0;
		HMD_PlacedDesignationComponent pd = HMD_PlacedDesignationComponent.Cast(node.FindComponent(HMD_PlacedDesignationComponent));
		if (pd)
		{
			pd.HMD_InvalidateCachedHudLabel();
			n++;
		}
		IEntity ch = node.GetChildren();
		while (ch)
		{
			n += HMD_BumpPlacedDesignationHudCachesInHierarchy(ch);
			ch = ch.GetSibling();
		}
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! Child proxy prefabs (HMD_PlacedDesignation under this entity) must refresh HUD name when text/number replicate.
	protected void HMD_BumpDesignationProxyChildPlacedCaches()
	{
		IEntity o = GetOwner();
		if (!o)
			return;
		IEntity ch = o.GetChildren();
		while (ch)
		{
			HMD_BumpPlacedDesignationHudCachesInHierarchy(ch);
			ch = ch.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnBeaconStateReplicated()
	{
		m_bSeenAnyBeaconRplSnapshot = true;
		HMD_BumpDesignationProxyChildPlacedCaches();
		RefreshHudRegistration();
		ScheduleDeferredClientBeaconVisualRefresh();
	}

	//------------------------------------------------------------------------------------------------
	void OnPlacedStateReplicated()
	{
		m_bSeenAnyBeaconRplSnapshot = true;
		RefreshHudRegistration();
		ScheduleDeferredClientBeaconVisualRefresh();
	}

	//------------------------------------------------------------------------------------------------
	protected void ScheduleDeferredClientBeaconVisualRefresh()
	{
		if (!GetGame())
			return;
		if (!Replication.IsRunning() || !Replication.IsClient())
			return;
		GetGame().GetCallqueue().CallLater(DeferredClientBeaconVisualRefresh, 0, false);
		GetGame().GetCallqueue().CallLater(DeferredClientBeaconVisualRefresh, 75, false);
		GetGame().GetCallqueue().CallLater(DeferredClientBeaconVisualRefresh, 250, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeferredClientBeaconVisualRefresh()
	{
		RefreshHudRegistration();
	}

	//------------------------------------------------------------------------------------------------
	//! CallQueue fallback when EOnFrame does not run until play (or runs late).
	protected void DeferredRefreshWhenPlayMode()
	{
		if (!GetOwner() || !GetGame())
			return;
		if (!GetGame().InPlayMode())
		{
			if (m_iPlayModeRefreshPolls < PLAY_MODE_REFRESH_POLL_MAX)
			{
				m_iPlayModeRefreshPolls++;
				GetGame().GetCallqueue().CallLater(DeferredRefreshWhenPlayMode, 25, false);
			}
			return;
		}
		m_iPlayModeRefreshPolls = 0;
		if (!m_bPendingPlayModeRefresh)
			return;
		m_bPendingPlayModeRefresh = false;
		RefreshHudRegistration();
		ScheduleDeferredClientBeaconVisualRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Placement callbacks may run on the placing client; RplProps only replicate from authority. Mirror TrySetBeaconActive routing.
	void NotifyPlacedInWorld()
	{
		if (!Replication.IsRunning() || Replication.IsServer())
			ApplyPlacedInWorldOnAuthority();
		else
			Rpc(RpcAsk_NotifyPlacedInWorld);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_NotifyPlacedInWorld()
	{
		ApplyPlacedInWorldOnAuthority();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPlacedInWorldOnAuthority()
	{
		m_bPlacedInWorld = true;
		if (Replication.IsRunning())
			Replication.BumpMe();
		RefreshHudRegistration();
		ScheduleDeferredClientBeaconVisualRefresh();
	}

	//------------------------------------------------------------------------------------------------
	bool ShouldHideInventoryActions()
	{
		return IsAuthorityTransmitting();
	}

	//------------------------------------------------------------------------------------------------
	protected void StopIrStrobeLoop()
	{
		GetGame().GetCallqueue().Remove(IrStrobeOnPhaseEnd);
		GetGame().GetCallqueue().Remove(IrStrobeOffPhaseEnd);
	}

	//------------------------------------------------------------------------------------------------
	protected void CancelIrLightSpawnDelay()
	{
		GetGame().GetCallqueue().Remove(IrLightDelayedSpawn);
		m_bIrLightSpawnDelayPending = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void IrLightDelayedSpawn()
	{
		m_bIrLightSpawnDelayPending = false;
		if (m_pSpawnedIrLight && !m_pSpawnedIrLight.GetWorld())
			m_pSpawnedIrLight = null;
		IEntity owner = GetOwner();
		if (!owner || m_pSpawnedIrLight || !IsIrTransmitting())
			return;
		SpawnIrLightIfNeeded(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void IrStrobeOnPhaseEnd()
	{
		IEntity owner = GetOwner();
		if (!owner || !m_pSpawnedIrLight || !IsIrTransmitting())
		{
			StopIrStrobeLoop();
			return;
		}
		RHS_LightEntity le = RHS_LightEntity.Cast(m_pSpawnedIrLight);
		if (le)
			le.SetEnabledWithIRCheck(false);
		float offMs = m_fIrStrobeOffMs;
		if (offMs < 1)
			offMs = 1;
		GetGame().GetCallqueue().CallLater(IrStrobeOffPhaseEnd, offMs, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void IrStrobeOffPhaseEnd()
	{
		IEntity owner = GetOwner();
		if (!owner || !m_pSpawnedIrLight || !IsIrTransmitting())
		{
			StopIrStrobeLoop();
			return;
		}
		RHS_LightEntity le = RHS_LightEntity.Cast(m_pSpawnedIrLight);
		if (le)
			le.SetEnabledWithIRCheck(true);
		float onMs = m_fIrStrobeOnMs;
		if (onMs < 1)
			onMs = 1;
		GetGame().GetCallqueue().CallLater(IrStrobeOnPhaseEnd, onMs, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartIrStrobeLoop(IEntity owner)
	{
		StopIrStrobeLoop();
		if (!m_pSpawnedIrLight || !owner)
			return;
		RHS_LightEntity le = RHS_LightEntity.Cast(m_pSpawnedIrLight);
		if (le)
			le.SetEnabledWithIRCheck(true);
		float onMs = m_fIrStrobeOnMs;
		if (onMs < 1)
			onMs = 1;
		GetGame().GetCallqueue().CallLater(IrStrobeOnPhaseEnd, onMs, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void DespawnIrLight()
	{
		CancelIrLightSpawnDelay();
		StopIrStrobeLoop();
		if (!m_pSpawnedIrLight)
			return;
		IEntity lightEnt = m_pSpawnedIrLight;
		m_pSpawnedIrLight = null;
		SCR_EntityHelper.DeleteEntityAndChildren(lightEnt);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnIrLightIfNeeded(IEntity owner)
	{
		if (m_pSpawnedIrLight && !m_pSpawnedIrLight.GetWorld())
			m_pSpawnedIrLight = null;
		if (m_pSpawnedIrLight)
			return;
		ResourceName prefabName = m_sIrLightPrefab;
		if (!prefabName || prefabName == "")
			return;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		vector worldOff = m_vIrLightWorldOffset;
		vector parentWorld[4];
		owner.GetWorldTransform(parentWorld);
		vector localPos = Vector(0, 0, 0);
		float offSq = worldOff[0] * worldOff[0] + worldOff[1] * worldOff[1] + worldOff[2] * worldOff[2];
		if (offSq > 1e-12)
			localPos = IrWorldOffsetToLocal(worldOff, parentWorld);
		EntitySpawnParams sp = new EntitySpawnParams();
		sp.TransformMode = ETransformMode.LOCAL;
		sp.Parent = owner;
		sp.Transform[0] = Vector(1, 0, 0);
		sp.Transform[1] = Vector(0, 1, 0);
		sp.Transform[2] = Vector(0, 0, 1);
		sp.Transform[3] = localPos;
		IEntity spawned = GetGame().SpawnEntityPrefabEx(prefabName, false, world, sp);
		if (!spawned)
			return;
		m_pSpawnedIrLight = spawned;
		StartIrStrobeLoop(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshIrLightState()
	{
		if (!GetGame() || !GetGame().InPlayMode())
			return;
		IEntity owner = GetOwner();
		if (!owner)
			return;
		if (!IsIrTransmitting())
		{
			DespawnIrLight();
			return;
		}
		if (m_pSpawnedIrLight && !m_pSpawnedIrLight.GetWorld())
		{
			m_pSpawnedIrLight = null;
			CancelIrLightSpawnDelay();
		}
		if (m_pSpawnedIrLight)
			return;
		if (m_bIrLightSpawnDelayPending)
			return;
		float delayMs = m_fIrLightSpawnDelayMs;
		if (delayMs < 0)
			delayMs = 0;
		if (delayMs > 0)
		{
			m_bIrLightSpawnDelayPending = true;
			GetGame().GetCallqueue().CallLater(IrLightDelayedSpawn, delayMs, false);
			return;
		}
		SpawnIrLightIfNeeded(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_ClearIffMarkerPoolRow(HUDMarkerSystem sys)
	{
		if (!sys || m_iIffMarkerPoolId < 0)
			return;
		sys.UnregisterIffMarker(m_iIffMarkerPoolId);
		m_iIffMarkerPoolId = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Same pattern as HMD_PlacedDesignationComponent: RegisterIffMarker / Update* / UnregisterIffMarker every EOnFrame on gameplay clients (local pool from entity origin + presentation).
	protected void RefreshLocalHudMarkerOnly()
	{
		if (!GetGame() || !GetGame().InPlayMode())
			return;
		IEntity owner = GetOwner();
		ChimeraWorld world = GetGame().GetWorld();
		if (!owner || !world)
			return;

		if (Replication.IsRunning() && Replication.IsServer() && !Replication.IsClient())
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return;
		if (HasSiblingDesignationHud(owner))
		{
			HMD_ClearIffMarkerPoolRow(sys);
			return;
		}
		if (DeploymentGateAllowsIff() && IffPooledRowShouldShow() && GetEffectiveBatteryForTransmitGate() > 0)
		{
			vector pos = owner.GetOrigin();
			string label;
			int dot;
			int lblCol;
			float visDist;
			HUDMarkerComponent.HMD_ResolveIffPoolPresentation(owner, BuildMarkerLabel(), label, dot, lblCol, visDist);
			float lifeSec = m_fBatterySecondsRemaining;
			if (lifeSec <= 0)
				lifeSec = GetEffectiveBatteryForTransmitGate();
			if (m_iIffMarkerPoolId < 0)
				m_iIffMarkerPoolId = sys.RegisterIffMarker(pos, label, dot, lblCol, visDist, lifeSec, RplId.Invalid());
			else
			{
				sys.UpdateIffMarkerRow(m_iIffMarkerPoolId, pos, label, dot, lblCol, visDist, lifeSec);
			}
		}
		else
			HMD_ClearIffMarkerPoolRow(sys);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshHudRegistration()
	{
		if (!GetGame())
			return;
		if (!GetGame().InPlayMode())
			return;
		IEntity owner = GetOwner();
		ChimeraWorld world = GetGame().GetWorld();
		if (!owner || !world)
			return;
		RefreshIrLightState();
		//! Headless dedicated has no local player HUD; gameplay clients use RegisterIffMarker pool (designation-style).
		if (Replication.IsRunning() && Replication.IsServer() && !Replication.IsClient())
			return;
		if (!HUDMarkerSystem.GetInstance(world))
			return;
		RefreshLocalHudMarkerOnly();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		DespawnIrLight();
		if (GetGame() && GetGame().InPlayMode() && owner)
		{
			ChimeraWorld world = GetGame().GetWorld();
			if (world)
			{
				HUDMarkerSystem sysDel = HUDMarkerSystem.GetInstance(world);
				if (sysDel)
					HMD_ClearIffMarkerPoolRow(sysDel);
			}
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			if (m_fBatterySecondsRemaining <= 0)
				m_fBatterySecondsRemaining = BEACON_TOTAL_SECONDS;
			if (m_iNumber < 1)
				m_iNumber = 1;
			if (m_iNumber > 7)
				m_iNumber = 7;
			if (m_iTextIndex < 0 || m_iTextIndex >= TEXT_COUNT)
				m_iTextIndex = 0;
			if (!ShouldUnplaceWhenInInventorySlot())
			{
				m_bPlacedInWorld = true;
				m_bBeaconActive = true;
			}
			m_bSeenAnyBeaconRplSnapshot = true;
			if (Replication.IsRunning())
				Replication.BumpMe();
		}
		SetEventMask(owner, EntityEvent.FRAME | owner.GetEventMask());
		if (!GetGame() || !GetGame().InPlayMode())
		{
			m_bPendingPlayModeRefresh = true;
			m_iPlayModeRefreshPolls = 0;
			if (GetGame())
				GetGame().GetCallqueue().CallLater(DeferredRefreshWhenPlayMode, 0, false);
		}
		RefreshHudRegistration();
		ScheduleDeferredClientBeaconVisualRefresh();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		if (!owner)
			return;

		if (m_bPendingPlayModeRefresh && GetGame() && GetGame().InPlayMode())
		{
			m_bPendingPlayModeRefresh = false;
			m_iPlayModeRefreshPolls = 0;
			RefreshHudRegistration();
			ScheduleDeferredClientBeaconVisualRefresh();
		}

		if (!GetGame() || !GetGame().InPlayMode())
			return;

		if (!Replication.IsRunning() || Replication.IsServer())
		{
			AuthorityTickDeploymentGates(owner);

			if (DeploymentGateAllowsIff() && m_bBeaconActive && m_fBatterySecondsRemaining > 0)
			{
				ChimeraWorld w = GetGame().GetWorld();
				if (w)
				{
					WorldTimestamp now = w.GetServerTimestamp();
					if (!m_bBatteryDrainServerTimeSet)
					{
						m_ServerTimeLastBatteryDrain = now;
						m_bBatteryDrainServerTimeSet = true;
					}
					float dt = now.DiffMilliseconds(m_ServerTimeLastBatteryDrain) * 0.001;
					if (dt < 0)
						dt = 0;
					if (dt > BATTERY_DRAIN_DT_CAP)
						dt = BATTERY_DRAIN_DT_CAP;
					m_ServerTimeLastBatteryDrain = now;
					m_fBatterySecondsRemaining -= dt;
					if (m_fBatterySecondsRemaining <= 0)
					{
						m_fBatterySecondsRemaining = 0;
						m_bBeaconActive = false;
						m_bBatteryDrainServerTimeSet = false;
						m_fBatteryBumpAccum = 0;
						if (Replication.IsRunning())
							Replication.BumpMe();
						RefreshHudRegistration();
					}
					else if (Replication.IsRunning())
					{
						m_fBatteryBumpAccum += dt;
						if (m_fBatteryBumpAccum >= 1.0)
						{
							m_fBatteryBumpAccum = 0;
							Replication.BumpMe();
						}
					}
				}
			}
			else
			{
				m_bBatteryDrainServerTimeSet = false;
			}
		}

		//! IR: evaluate every frame (spawn/despawn follows IsIrTransmitting); pool was previously tied to the same flag and lagged dynamic spawns.
		RefreshIrLightState();

		if (!(Replication.IsRunning() && Replication.IsServer() && !Replication.IsClient()))
			RefreshLocalHudMarkerOnly();
	}

	//------------------------------------------------------------------------------------------------
	float GetTextIndexNormalized01()
	{
		int idx = m_iTextIndex;
		if (idx < 0)
			idx = 0;
		if (idx >= TEXT_COUNT)
			idx = TEXT_COUNT - 1;
		float denom = TEXT_COUNT - 1;
		if (denom <= 0)
			return 0;
		return idx / denom;
	}

	//------------------------------------------------------------------------------------------------
	float GetNumberNormalized01()
	{
		int n = m_iNumber;
		if (n < 1)
			n = 1;
		if (n > 7)
			n = 7;
		return (n - 1) / 6.0;
	}

	//------------------------------------------------------------------------------------------------
	float GetBatteryFraction01()
	{
		if (BEACON_TOTAL_SECONDS <= 0)
			return 0;
		float f = m_fBatterySecondsRemaining / BEACON_TOTAL_SECONDS;
		if (f < 0)
			f = 0;
		if (f > 1)
			f = 1;
		return f;
	}

	//------------------------------------------------------------------------------------------------
	int GetBatteryPercent()
	{
		return (int)(GetBatteryFraction01() * 100.0 + 0.5);
	}

	//------------------------------------------------------------------------------------------------
	float GetBatteryMinutesRemaining()
	{
		float sec = m_fBatterySecondsRemaining;
		if (sec < 0)
			sec = 0;
		return sec / 60.0;
	}

	//------------------------------------------------------------------------------------------------
	bool IsBeaconActive()
	{
		return m_bBeaconActive;
	}

	//------------------------------------------------------------------------------------------------
	bool CanConfigure()
	{
		return !m_bBeaconActive && m_fBatterySecondsRemaining > 0;
	}

	//------------------------------------------------------------------------------------------------
	string GetPreviewLabel()
	{
		return BuildMarkerLabel();
	}

	//------------------------------------------------------------------------------------------------
	string GetPreviewTextCode()
	{
		string t = "HLS";
		switch (m_iTextIndex)
		{
			case 0: { t = "HLS"; break; }
			case 1: { t = "VKY"; break; }
			case 2: { t = "BSH"; break; }
			case 3: { t = "BND"; break; }
			case 4: { t = "MSF"; break; }
		}
		return t;
	}

	//------------------------------------------------------------------------------------------------
	string GetPreviewNumberString()
	{
		int n = m_iNumber;
		if (n < 1)
			n = 1;
		if (n > 7)
			n = 7;
		return string.Format("%1", n);
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerCycleTextDir(int dir)
	{
		if (m_bBeaconActive || m_fBatterySecondsRemaining <= 0)
			return;
		if (dir > 0)
		{
			m_iTextIndex++;
			if (m_iTextIndex >= TEXT_COUNT)
				m_iTextIndex = 0;
		}
		else
		{
			m_iTextIndex--;
			if (m_iTextIndex < 0)
				m_iTextIndex = TEXT_COUNT - 1;
		}
		if (Replication.IsRunning())
			Replication.BumpMe();
		HMD_BumpDesignationProxyChildPlacedCaches();
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerCycleNumberDir(int dir)
	{
		if (m_bBeaconActive || m_fBatterySecondsRemaining <= 0)
			return;
		if (dir > 0)
		{
			m_iNumber++;
			if (m_iNumber > 7)
				m_iNumber = 1;
		}
		else
		{
			m_iNumber--;
			if (m_iNumber < 1)
				m_iNumber = 7;
		}
		if (Replication.IsRunning())
			Replication.BumpMe();
		HMD_BumpDesignationProxyChildPlacedCaches();
	}

	//------------------------------------------------------------------------------------------------
	void TryCycleTextDirection(int dir)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
			ServerCycleTextDir(dir);
		else
			Rpc(RpcAsk_CycleTextDir, dir);
	}

	//------------------------------------------------------------------------------------------------
	void TryCycleNumberDirection(int dir)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
			ServerCycleNumberDir(dir);
		else
			Rpc(RpcAsk_CycleNumberDir, dir);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_CycleTextDir(int dir)
	{
		ServerCycleTextDir(dir);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_CycleNumberDir(int dir)
	{
		ServerCycleNumberDir(dir);
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerSetBeaconActive(bool active)
	{
		if (active)
		{
			if (m_fBatterySecondsRemaining <= 0)
				return;
			if (!DeploymentGateAllowsIff())
				return;
			m_bBeaconActive = true;
		}
		else
			m_bBeaconActive = false;
		if (Replication.IsRunning())
			Replication.BumpMe();
		RefreshHudRegistration();
	}

	//------------------------------------------------------------------------------------------------
	void TrySetBeaconActive(bool active)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
			ServerSetBeaconActive(active);
		else
			Rpc(RpcAsk_SetBeaconActive, active);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetBeaconActive(bool active)
	{
		ServerSetBeaconActive(active);
	}
}
