//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "IFF IR + optional HUD dot: no battery, no placement/explosives. IR delay starts at creation. If HMD_PlacedDesignationComponent is on the same entity, HUDMarkerSystem is not used (designation owns the marker). Destroy parent to remove IR.")]
class HMD_IffLifetimeComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! No HMD_IffBeaconExplosiveInventoryItemComponent / NotifyPlacedInWorld / m_bPlacedInWorld.
//! Authority sets m_bBeaconActive true on spawn so clients get IR + HUD; optional TrySetBeaconActive(false) to stop.
//! Client IR: m_bBeaconActive; OnDelete despawn. HUD: Register only when no HMD_PlacedDesignationComponent; else designation supplies the marker.
class HMD_IffLifetimeComponent : ScriptComponent
{
	protected static const int TEXT_COUNT = 5;

	[Attribute(defvalue: "{0BCD51DD36B82132}Prefabs/Items/Equipment/Nightvision/IFF_IR_Light.et", UIWidgets.ResourceNamePicker, "RHS_LightEntity prefab spawned locally while beacon transmits; empty = no IR light.", "et", category: "HMD")]
	protected ResourceName m_sIrLightPrefab;

	[Attribute("1500", UIWidgets.Auto, "IR strobe ON duration (ms)", category: "HMD")]
	protected float m_fIrStrobeOnMs;

	[Attribute("500", UIWidgets.Auto, "IR strobe OFF duration (ms)", category: "HMD")]
	protected float m_fIrStrobeOffMs;

	[Attribute("0", UIWidgets.Auto, "Delay before first IR light spawn (ms), counted from entity creation (OnPostInit); 0 = immediate.", category: "HMD")]
	protected float m_fIrLightSpawnDelayMs;

	[Attribute("0 0 0", UIWidgets.EditBox, "World-space offset (m) from beacon origin for IR light position (e.g. 0 0.5 0 = half meter along world +Y).", category: "HMD")]
	protected vector m_vIrLightWorldOffset;

	[RplProp(onRplName: "OnBeaconStateReplicated")]
	protected bool m_bBeaconActive;

	[RplProp(onRplName: "OnBeaconStateReplicated")]
	protected int m_iTextIndex;

	[RplProp(onRplName: "OnBeaconStateReplicated")]
	protected int m_iNumber;

	protected float m_fHudRegRetryAccum;
	protected static const float HUD_REG_RETRY_INTERVAL = 0.25;

	protected IEntity m_pSpawnedIrLight;
	protected bool m_bIrLightSpawnDelayPending;

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
	static HMD_IffLifetimeComponent FindOnEntity(IEntity owner)
	{
		if (!owner)
			return null;
		return HMD_IffLifetimeComponent.Cast(owner.FindComponent(HMD_IffLifetimeComponent));
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
	//! Placed laser/designation already registers HUDMarkerSystem; avoid a second IFF dot/label on the same entity.
	protected bool HasPlacedDesignationSibling(IEntity owner)
	{
		if (!owner)
			return false;
		return HMD_PlacedDesignationComponent.Cast(owner.FindComponent(HMD_PlacedDesignationComponent)) != null;
	}

	//------------------------------------------------------------------------------------------------
	void OnBeaconStateReplicated()
	{
		RefreshHudRegistration();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsIrTransmitting()
	{
		return m_bBeaconActive;
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
		if (!GetGame().InPlayMode())
			return;
		if (Replication.IsRunning() && Replication.IsServer() && !Replication.IsClient())
			return;
		IEntity owner = GetOwner();
		if (!owner)
			return;
		if (!m_bBeaconActive)
		{
			DespawnIrLight();
			return;
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
	protected void RefreshHudRegistration()
	{
		if (!GetGame().InPlayMode())
			return;
		if (Replication.IsRunning() && Replication.IsServer() && !Replication.IsClient())
			return;
		IEntity owner = GetOwner();
		ChimeraWorld world = GetGame().GetWorld();
		if (!owner || !world)
			return;
		RefreshIrLightState();
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return;
		if (HasPlacedDesignationSibling(owner))
			return;
		if (m_bBeaconActive)
		{
			string label = BuildMarkerLabel();
			sys.Register(owner, ResolveHudRegisterLifetime(owner), label, Color.FromRGBA(0, 255, 0, 255).PackToInt(), Color.FromRGBA(255, 255, 255, 255).PackToInt());
		}
		else
		{
			sys.RemoveMarkerEntry(owner);
		}
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
				if (sys && !HasPlacedDesignationSibling(owner))
					sys.RemoveMarkerEntry(owner);
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
			if (m_iNumber < 1)
				m_iNumber = 1;
			if (m_iNumber > 7)
				m_iNumber = 7;
			if (m_iTextIndex < 0 || m_iTextIndex >= TEXT_COUNT)
				m_iTextIndex = 0;
			//! Begin IR delay from creation: transmitting on so clients schedule m_fIrLightSpawnDelayMs at first Refresh.
			m_bBeaconActive = true;
			if (Replication.IsRunning())
				Replication.BumpMe();
		}
		SetEventMask(owner, EntityEvent.FRAME | owner.GetEventMask());
		RefreshHudRegistration();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		if (!GetGame().InPlayMode() || !owner)
			return;

		bool runClientHudRetry = !Replication.IsRunning() || Replication.IsClient();
		if (runClientHudRetry && m_bBeaconActive)
		{
			ChimeraWorld world = GetGame().GetWorld();
			if (world && !HUDMarkerSystem.GetInstance(world))
			{
				m_fHudRegRetryAccum += timeSlice;
				if (m_fHudRegRetryAccum >= HUD_REG_RETRY_INTERVAL)
				{
					m_fHudRegRetryAccum = 0;
					RefreshHudRegistration();
				}
			}
			else
			{
				m_fHudRegRetryAccum = 0;
			}
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
			m_bBeaconActive = true;
		else
			m_bBeaconActive = false;
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
