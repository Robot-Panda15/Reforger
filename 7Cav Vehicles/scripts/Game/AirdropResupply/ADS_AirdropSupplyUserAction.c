//------------------------------------------------------------------------------------------------
//! Vehicle action: spawns a supply box at m_vSpawnOffsetLocal from the aircraft center (local axes).
//! Requires SCR_ResourceComponent on the vehicle and enough SUPPLIES in its container; deducts m_iSupplyCost.
//! Prefab must add ActionContexts merge "ADS_AirdropContext" and ParentContextList that name.
//! CanBeShown/Performed use occupant check; super() distance to context is not required so
//! pilot seat still passes when vanilla airdrop_action origin is far away.
class ADS_AirdropSupplyUserAction : ScriptedUserAction
{
	[Attribute("0 0 0", desc: "Offset from aircraft center [m], local axes: X=right, Y=up, Z=forward")]
	protected vector m_vSpawnOffsetLocal;

	[Attribute("{EF693C583CAB7964}Prefabs/Props/Military/CISS/SupplyDrop/CAV_SuppliesDrop_US_EquipmentBox.et", params: "et")]
	protected ResourceName m_rSupplyPrefab;

	[Attribute("200", desc: "SUPPLIES resource consumed per drop (SCR_ResourceComponent container)", params: "0 inf")]
	protected int m_iSupplyCost;

	protected const int ADS_LOG_EVERY_N = 30;

	protected int m_iADS_LogCounter;
	protected bool m_bADS_LoggedFirstShown;
	protected bool m_bADS_LoggedFirstPerformed;

	//------------------------------------------------------------------------------------------------
	protected void ADS_DebugLog(string msg)
	{
		Print(string.Format("[ADS_Airdrop] %1", msg), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected static string ADS_FormatEntity(IEntity ent)
	{
		if (!ent)
			return "null";

		EntityPrefabData pd = ent.GetPrefabData();
		string pfn = "<?prefab?>";
		if (pd)
		{
			string n = pd.GetPrefabName();
			if (n && n != string.Empty)
				pfn = n;
		}

		return string.Format("name='%1' id=%2 prefab=%3", ent.GetName(), ent.GetID(), pfn);
	}

	//------------------------------------------------------------------------------------------------
	protected bool ADS_ShouldLogThrottle()
	{
		m_iADS_LogCounter++;
		return ((m_iADS_LogCounter % ADS_LOG_EVERY_N) == 0);
	}

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);

		ADS_DebugLog(string.Format(
			"Init | owner=%1 | aircraft(Resolve)=%2 | server=%3 client=%4 replRunning=%5",
			ADS_FormatEntity(pOwnerEntity),
			ADS_FormatEntity(ResolveAircraftEntity()),
			Replication.IsServer(),
			Replication.IsClient(),
			Replication.IsRunning()));
	}

	//------------------------------------------------------------------------------------------------
	protected bool ADS_UserIsInAircraftVehicle(IEntity user, IEntity aircraft)
	{
		if (!user || !aircraft)
			return false;

		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(user);
		if (!ch || !ch.IsInVehicle())
			return false;

		IEntity userVeh = SCR_CompartmentAccessComponent.GetVehicleIn(ch);
		if (!userVeh)
			return false;

		IEntity ar = aircraft;
		IEntity arRoot = ar.GetRootParent();
		if (arRoot)
			ar = arRoot;

		IEntity uvRoot = userVeh.GetRootParent();
		if (uvRoot)
			userVeh = uvRoot;

		return userVeh == ar;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ResourceComponent ADS_GetVehicleResourceComponent(IEntity aircraft)
	{
		if (!aircraft)
			return null;
		SCR_ResourceComponent res = SCR_ResourceComponent.FindResourceComponent(aircraft, false);
		if (res)
			return res;
		//! Some vehicle prefabs attach resources under a child entity; proxies may need hierarchy search.
		return SCR_ResourceComponent.FindResourceComponent(aircraft, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Dedicated clients often do not get authoritative SUPPLIES amounts on the vehicle proxy; server PerformAction re-checks.
	protected bool ADS_IsDedicatedClient()
	{
		return Replication.IsRunning() && Replication.IsClient() && !Replication.IsServer();
	}

	//------------------------------------------------------------------------------------------------
	protected bool ADS_CanAffordDrop(IEntity aircraft, out SCR_ResourceContainer supplyContainer)
	{
		supplyContainer = null;
		SCR_ResourceComponent res = ADS_GetVehicleResourceComponent(aircraft);
		if (!res)
			return false;

		SCR_ResourceContainer ctr = res.GetContainer(EResourceType.SUPPLIES);
		if (!ctr)
			return false;

		supplyContainer = ctr;
		if (ADS_IsDedicatedClient())
			return true;
		return ctr.GetResourceValue() >= m_iSupplyCost;
	}

	//------------------------------------------------------------------------------------------------
	protected void ADS_SetCannotPerformResourceReason(IEntity aircraft)
	{
		SCR_ResourceComponent res = ADS_GetVehicleResourceComponent(aircraft);
		if (!res)
		{
			SetCannotPerformReason("Vehicle has no SCR_ResourceComponent");
			return;
		}
		SCR_ResourceContainer ctr = res.GetContainer(EResourceType.SUPPLIES);
		if (!ctr)
		{
			SetCannotPerformReason("Vehicle has no SUPPLIES container");
			return;
		}
		SetCannotPerformReason(string.Format("Insufficient supplies (%1 required)", m_iSupplyCost));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user)
			return false;

		IEntity aircraft = ResolveAircraftEntity();
		bool inVeh = ADS_UserIsInAircraftVehicle(user, aircraft);
		bool superShown = super.CanBeShownScript(user);
		bool ok = inVeh;

		if (!m_bADS_LoggedFirstShown)
		{
			m_bADS_LoggedFirstShown = true;
			ADS_DebugLog(string.Format(
				"FIRST CanBeShown | inThisVehicle=%1 | super.CanBeShownScript=%2 | finalShown=%3 (script uses inVeh only; if super=0 try context/radius) | user=%4 | owner=%5 | aircraft=%6",
				inVeh,
				superShown,
				ok,
				ADS_FormatEntity(user),
				ADS_FormatEntity(GetOwner()),
				ADS_FormatEntity(aircraft)));
		}

		if (ADS_ShouldLogThrottle())
		{
			ADS_DebugLog(string.Format(
				"CanBeShownScript -> %1 | super=%2 | user=%3 | owner=%4",
				ok,
				superShown,
				ADS_FormatEntity(user),
				ADS_FormatEntity(GetOwner())));
		}

		return ok;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		IEntity aircraft = ResolveAircraftEntity();
		bool inVeh = ADS_UserIsInAircraftVehicle(user, aircraft);
		bool superPerf = super.CanBePerformedScript(user);

		SCR_ResourceContainer ctr;
		bool afford = false;
		if (aircraft)
			afford = ADS_CanAffordDrop(aircraft, ctr);

		if (inVeh && !afford)
			ADS_SetCannotPerformResourceReason(aircraft);

		bool finalOk = inVeh && afford;

		if (!m_bADS_LoggedFirstPerformed)
		{
			m_bADS_LoggedFirstPerformed = true;
			ADS_DebugLog(string.Format(
				"FIRST CanBePerformed | inThisVehicle=%1 | afford=%2 | super=%3 | final=%4 | cannotPerformReason='%5' | user=%6 | owner=%7 | aircraft=%8",
				inVeh,
				afford,
				superPerf,
				finalOk,
				m_sCannotPerformReason,
				ADS_FormatEntity(user),
				ADS_FormatEntity(GetOwner()),
				ADS_FormatEntity(aircraft)));
		}

		if (ADS_ShouldLogThrottle())
		{
			ADS_DebugLog(string.Format(
				"CanBePerformedScript -> %1 | super=%2 | cannotPerformReason='%3' | user=%4 | owner=%5",
				finalOk,
				superPerf,
				m_sCannotPerformReason,
				ADS_FormatEntity(user),
				ADS_FormatEntity(GetOwner())));
		}

		if (!inVeh)
			return false;

		if (!afford)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity ResolveAircraftEntity()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;

		if (owner.FindComponent(BaseCompartmentManagerComponent))
			return owner;

		IEntity parent = owner.GetParent();
		if (parent && parent.FindComponent(BaseCompartmentManagerComponent))
			return parent;

		return owner;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
		{
			ADS_DebugLog("PerformAction skipped: not server");
			return;
		}

		IEntity aircraft = ResolveAircraftEntity();
		if (!aircraft)
		{
			ADS_DebugLog("PerformAction failed: ResolveAircraftEntity null");
			return;
		}

		SCR_ResourceContainer supplyCtr;
		if (!ADS_CanAffordDrop(aircraft, supplyCtr))
		{
			ADS_DebugLog("PerformAction failed: cannot afford drop (resource check)");
			ADS_SetCannotPerformResourceReason(aircraft);
			return;
		}

		if (!supplyCtr.DecreaseResourceValue(m_iSupplyCost, true))
		{
			ADS_DebugLog(string.Format(
				"PerformAction failed: DecreaseResourceValue returned false | cost=%1 have=%2",
				m_iSupplyCost,
				supplyCtr.GetResourceValue()));
			return;
		}

		Resource res = Resource.Load(m_rSupplyPrefab);
		if (!res || !res.IsValid())
		{
			ADS_DebugLog(string.Format("PerformAction failed: invalid supply prefab resource | %1", m_rSupplyPrefab));
			return;
		}

		vector mat[4];
		aircraft.GetWorldTransform(mat);

		vector worldPos = mat[3];
		vector right = mat[0];
		vector up = mat[1];
		vector forward = mat[2];
		vector off = m_vSpawnOffsetLocal;
		vector spawnPos = worldPos + right * off[0] + up * off[1] + forward * off[2];
		mat[3] = spawnPos;

		EntitySpawnParams esp = new EntitySpawnParams();
		esp.TransformMode = ETransformMode.WORLD;
		esp.Transform = mat;

		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
		{
			ADS_DebugLog("PerformAction failed: GetWorld null");
			return;
		}

		GetGame().SpawnEntityPrefab(res, world, esp);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Drop Infantry Resupply";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return true;
	}
}
