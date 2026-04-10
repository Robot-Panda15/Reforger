//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "IFF IR + optional HUD dot: no battery, no placement/explosives. IR uses RHS_LightEntity under this entity; strobing starts after optional delay at creation. If HMD_PlacedDesignationComponent is on the same entity, HUDMarkerSystem IFF pool is not used (designation owns the marker). Destroy parent to remove IR.")]
class HMD_IffLifetimeComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Grenade warheads use HMD_IffBeaconComponent (auto active on spawn). Placeables use HMD_IffBeaconComponentAttachable.
//! This component remains for optional prefabs that need IR without full beacon placement semantics.
//! Prefab must include an RHS_LightEntity child (or subtree) for IR strobing.
//! No HMD_IffBeaconExplosiveInventoryItemComponent / NotifyPlacedInWorld / m_bPlacedInWorld.
//! Authority sets m_bBeaconActive true on spawn so clients get IR + HUD; optional TrySetBeaconActive(false) to stop.
//! World IR uses m_bBeaconActive OR m_bOptimisticIrTransmit.
//! Only clear m_bOptimisticIrTransmit when inactive AFTER Rpl has reported active at least once (m_bSeenRplBeaconActiveTrue).
//! Otherwise the first callback can run with default false before the server snapshot, and IR never starts.
//! HUD: RegisterIffMarker pool when no designation HUD sibling; co-located HUDMarkerComponent skips entity Register (see HUDMarkerSystem.GetMarkerData).
class HMD_IffLifetimeComponent : ScriptComponent
{
	protected static const int TEXT_COUNT = 5;

	[Attribute("1500", UIWidgets.Auto, "IR glow time (ms)", category: "HMD")]
	protected float m_fIrStrobeOnMs;

	[Attribute("500", UIWidgets.Auto, "IR sleep time (ms)", category: "HMD")]
	protected float m_fIrStrobeOffMs;

	[Attribute("0", UIWidgets.Auto, "Delay before first IR strobing (ms), counted from entity creation (OnPostInit); 0 = immediate.", category: "HMD")]
	protected float m_fIrLightSpawnDelayMs;

	[RplProp(onRplName: "OnBeaconStateReplicated")]
	protected bool m_bBeaconActive;

	[RplProp(onRplName: "OnBeaconStateReplicated")]
	protected int m_iTextIndex;

	[RplProp(onRplName: "OnBeaconStateReplicated")]
	protected int m_iNumber;

	protected int m_iIffMarkerPoolId = -1;

	protected IEntity m_pIrLightRoot;
	protected bool m_bIrLightSpawnDelayPending;

	//! Client: optional emissive on IR prefab (RHS_StrobeDeviceComponent parity).
	protected ParametricMaterialInstanceComponent m_ParamMatInstComponent;

	//! Not replicated: assume IR on at init (grenade / lifetime). Cleared only after Rpl has shown active once, then inactive.
	protected bool m_bOptimisticIrTransmit = true;
	//! Client: set when m_bBeaconActive replicated true at least once; avoids clearing optimism on default false before first snapshot.
	protected bool m_bSeenRplBeaconActiveTrue;

	//------------------------------------------------------------------------------------------------
	static HMD_IffLifetimeComponent FindOnEntity(IEntity owner)
	{
		if (!owner)
			return null;
		return HMD_IffLifetimeComponent.Cast(owner.FindComponent(HMD_IffLifetimeComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShouldProcessLocalIrAndHud()
	{
		if (!GetGame().InPlayMode())
			return false;
		if (Replication.IsRunning() && Replication.IsServer() && !Replication.IsClient())
			return false;
		return true;
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
	protected float ResolveHudRegisterLifetime(IEntity owner)
	{
		if (!owner)
			return -1;
		HUDMarkerComponent hud = HUDMarkerComponent.Cast(owner.FindComponent(HUDMarkerComponent));
		if (hud)
			return hud.GetLifetimeSeconds();
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Designation HUD component already registers HUDMarkerSystem; avoid a second IFF dot/label on the same entity.
	protected bool HasSiblingDesignationHud(IEntity owner)
	{
		if (!owner)
			return false;
		return HMD_PlacedDesignationComponent.Cast(owner.FindComponent(HMD_PlacedDesignationComponent)) != null;
	}

	//------------------------------------------------------------------------------------------------
	void OnBeaconStateReplicated()
	{
		if (m_bBeaconActive)
			m_bSeenRplBeaconActiveTrue = true;
		else if (m_bSeenRplBeaconActiveTrue)
			m_bOptimisticIrTransmit = false;
		RefreshHudRegistration();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsIrTransmitting()
	{
		return m_bBeaconActive || m_bOptimisticIrTransmit;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindAttachedIrLightRoot(IEntity node)
	{
		if (!node)
			return null;
		if (RHS_LightEntity.Cast(node))
			return node;
		IEntity ch = node.GetChildren();
		while (ch)
		{
			IEntity found = FindAttachedIrLightRoot(ch);
			if (found)
				return found;
			ch = ch.GetSibling();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Prefab root may be GenericEntity with RHS_LightEntity on self or first-level children.
	protected RHS_LightEntity ResolveRhsLightEntity(IEntity spawned)
	{
		if (!spawned)
			return null;
		RHS_LightEntity le = RHS_LightEntity.Cast(spawned);
		if (le)
			return le;
		IEntity ch = spawned.GetChildren();
		while (ch)
		{
			le = RHS_LightEntity.Cast(ch);
			if (le)
				return le;
			ch = ch.GetSibling();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected ParametricMaterialInstanceComponent ResolveParametricMaterialInstance(IEntity root)
	{
		if (!root)
			return null;
		ParametricMaterialInstanceComponent p = ParametricMaterialInstanceComponent.Cast(root.FindComponent(ParametricMaterialInstanceComponent));
		if (p)
			return p;
		IEntity ch = root.GetChildren();
		while (ch)
		{
			p = ParametricMaterialInstanceComponent.Cast(ch.FindComponent(ParametricMaterialInstanceComponent));
			if (p)
				return p;
			ch = ch.GetSibling();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void StopIrStrobeLoop()
	{
		GetGame().GetCallqueue().Remove(IrStrobeEffectScheduleOff);
		GetGame().GetCallqueue().Remove(IrStrobeEffectScheduleOn);
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
		IEntity owner = GetOwner();
		if (!owner || m_pIrLightRoot || !IsIrTransmitting())
			return;
		TryResolveAttachedIrLight(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! RHS_LightEntity.SetEnabledWithIRCheck only.
	protected void IrStrobeEffect(bool pNewState)
	{
		if (!IsIrTransmitting() || !m_pIrLightRoot)
		{
			StopIrStrobeLoop();
			return;
		}

		GetGame().GetCallqueue().Remove(IrStrobeEffectScheduleOff);
		GetGame().GetCallqueue().Remove(IrStrobeEffectScheduleOn);

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());

		RHS_LightEntity le = ResolveRhsLightEntity(m_pIrLightRoot);
		if (le)
			le.SetEnabledWithIRCheck(pNewState);

		float glowMs = m_fIrStrobeOnMs;
		if (glowMs < 1)
			glowMs = 1;
		float sleepMs = m_fIrStrobeOffMs;
		if (sleepMs < 1)
			sleepMs = 1;

		float timeDelay = sleepMs;
		if (pNewState)
			timeDelay = glowMs;

		if (!m_ParamMatInstComponent)
			m_ParamMatInstComponent = ResolveParametricMaterialInstance(m_pIrLightRoot);
		if (m_ParamMatInstComponent)
		{
			bool nvCanSeeIr = pc && !pc.RHS_IsNVOff();
			float em = 0;
			if (pNewState && nvCanSeeIr)
				em = 1000;
			m_ParamMatInstComponent.SetEmissiveMultiplier(em);
		}

		if (pNewState)
			GetGame().GetCallqueue().CallLater(IrStrobeEffectScheduleOff, timeDelay, false);
		else
			GetGame().GetCallqueue().CallLater(IrStrobeEffectScheduleOn, timeDelay, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void IrStrobeEffectScheduleOff()
	{
		IrStrobeEffect(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void IrStrobeEffectScheduleOn()
	{
		IrStrobeEffect(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartIrStrobeLoop(IEntity owner)
	{
		StopIrStrobeLoop();
		if (!m_pIrLightRoot || !owner)
			return;
		IrStrobeEffect(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void DespawnIrLight()
	{
		CancelIrLightSpawnDelay();
		StopIrStrobeLoop();
		if (m_pIrLightRoot)
		{
			if (m_ParamMatInstComponent)
				m_ParamMatInstComponent.SetEmissiveMultiplier(0);
			RHS_LightEntity le = ResolveRhsLightEntity(m_pIrLightRoot);
			if (le)
				le.SetEnabledWithIRCheck(false);
		}
		m_ParamMatInstComponent = null;
		m_pIrLightRoot = null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TryResolveAttachedIrLight(IEntity owner)
	{
		if (m_pIrLightRoot)
			return true;
		IEntity found = FindAttachedIrLightRoot(owner);
		if (!found || !ResolveRhsLightEntity(found))
			return false;
		m_pIrLightRoot = found;
		StartIrStrobeLoop(owner);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshIrLightState()
	{
		if (!ShouldProcessLocalIrAndHud())
			return;
		IEntity owner = GetOwner();
		if (!owner)
			return;
		if (!IsIrTransmitting())
		{
			DespawnIrLight();
			return;
		}
		if (m_pIrLightRoot)
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
		TryResolveAttachedIrLight(owner);
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
	//! Same pattern as HMD_PlacedDesignationComponent: every-frame RegisterIffMarker / Update* / clear on IFF pool (local rows from entity).
	protected void RefreshLocalHudMarkerOnly()
	{
		if (!GetGame().InPlayMode())
			return;
		IEntity owner = GetOwner();
		ChimeraWorld world = GetGame().GetWorld();
		if (!owner || !world)
			return;
		if (Replication.IsRunning() && Replication.IsServer() && !Replication.IsClient())
			return;
		if (!ShouldProcessLocalIrAndHud())
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return;
		if (HasSiblingDesignationHud(owner))
		{
			HMD_ClearIffMarkerPoolRow(sys);
			return;
		}
		if (IsIrTransmitting())
		{
			vector pos = owner.GetOrigin();
			string label;
			int dot;
			int lblCol;
			float visDist;
			HUDMarkerComponent.HMD_ResolveIffPoolPresentation(owner, BuildMarkerLabel(), label, dot, lblCol, visDist);
			float lifeSec = ResolveHudRegisterLifetime(owner);
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
		if (!GetGame().InPlayMode())
			return;
		IEntity owner = GetOwner();
		ChimeraWorld world = GetGame().GetWorld();
		if (!owner || !world)
			return;
		if (ShouldProcessLocalIrAndHud())
			RefreshIrLightState();
		if (Replication.IsRunning() && Replication.IsServer() && !Replication.IsClient())
			return;
		if (!ShouldProcessLocalIrAndHud())
			return;
		if (!HUDMarkerSystem.GetInstance(world))
			return;
		RefreshLocalHudMarkerOnly();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		DespawnIrLight();
		if (GetGame().InPlayMode() && owner)
		{
			ChimeraWorld world = GetGame().GetWorld();
			if (world)
			{
				HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
				if (sys)
					HMD_ClearIffMarkerPoolRow(sys);
			}
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_bOptimisticIrTransmit = true;
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			if (m_iNumber < 1)
				m_iNumber = 1;
			if (m_iNumber > 7)
				m_iNumber = 7;
			if (m_iTextIndex < 0 || m_iTextIndex >= TEXT_COUNT)
				m_iTextIndex = 0;
			//! Begin IR delay from creation: transmitting on so clients schedule m_fIrLightSpawnDelayMs at first Refresh.
			m_bBeaconActive = true;
			m_bSeenRplBeaconActiveTrue = true;
			if (Replication.IsRunning())
				Replication.BumpMe();
		}
		SetEventMask(owner, EntityEvent.FRAME | owner.GetEventMask());
		RefreshHudRegistration();
		//! After Rpl snapshot (remote client + listen host): onRplName may lag first frame.
		if (Replication.IsRunning() && Replication.IsClient())
		{
			GetGame().GetCallqueue().CallLater(DeferredRefreshAfterReplication, 0, false);
			GetGame().GetCallqueue().CallLater(DeferredRefreshAfterReplication, 100, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DeferredRefreshAfterReplication()
	{
		RefreshHudRegistration();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		if (!GetGame().InPlayMode() || !owner)
			return;

		if (ShouldProcessLocalIrAndHud())
		{
			RefreshIrLightState();
			RefreshLocalHudMarkerOnly();
		}
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
	bool IsBeaconActive()
	{
		return m_bBeaconActive;
	}

	//------------------------------------------------------------------------------------------------
	bool CanConfigure()
	{
		return !m_bBeaconActive;
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
		if (m_bBeaconActive)
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
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerCycleNumberDir(int dir)
	{
		if (m_bBeaconActive)
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
			m_bBeaconActive = true;
			m_bSeenRplBeaconActiveTrue = true;
		}
		else
		{
			m_bBeaconActive = false;
		}
		if (!active)
			m_bOptimisticIrTransmit = false;
		else
			m_bOptimisticIrTransmit = true;
		if (Replication.IsRunning())
			Replication.BumpMe();
		if (!Replication.IsRunning() || Replication.IsClient())
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
