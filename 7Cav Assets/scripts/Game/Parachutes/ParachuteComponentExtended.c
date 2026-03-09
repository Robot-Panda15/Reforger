class ParachuteComponentExtendedClass : ParachuteComponentClass {}
class ParachuteComponentExtended : ParachuteComponent
{
	[Attribute("3.0", UIWidgets.Slider, "Deploy invincibility duration (s)", "0.5 10 0.5", category : "Landing")]
	protected float m_fDeployInvincibilityDuration;

	protected static void DebugChuteLog(string msg)
	{
		Print(msg);
		FileHandle f = FileIO.OpenFile("$logs:DEBUG_CHUTE_1c2333.txt", FileMode.APPEND);
		if (f) { f.WriteLine(msg); f.Close(); }
	}

	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		DebugChuteLog("[DEBUG_CHUTE] ParachuteComponentExtended EOnInit - MOD LOADED");
	}

	protected void EnableDeployInvincibility(IEntity pilot)
	{
		if (!pilot)
			return;
		DamageManagerComponent dmg = DamageManagerComponent.Cast(pilot.FindComponent(DamageManagerComponent));
		if (dmg)
			dmg.EnableDamageHandling(false);
	}

	protected void RestoreDeployInvincibility(IEntity pilot)
	{
		if (!pilot)
			return;
		DamageManagerComponent dmg = DamageManagerComponent.Cast(pilot.FindComponent(DamageManagerComponent));
		if (dmg)
			dmg.EnableDamageHandling(true);
	}

	override void RpcAskDeployParachute()
	{
		DebugChuteLog("[DEBUG_CHUTE] RpcAskDeployParachute called");
		if (m_bParachuteDeployed)
			return;

		IEntity pilot = GetPilotEntity();
		if (!pilot)
			return;

		EnableDeployInvincibility(pilot);

		ParachuteItemComponent item = ResolveParachuteItem_Server(pilot);
		if (!item)
		{
			RestoreDeployInvincibility(pilot);
			return;
		}

		if (!MayDeployParachute_Internal(pilot, item))
		{
			RestoreDeployInvincibility(pilot);
			return;
		}

		ResourceName prefab = item.GetParachutePrefab();
		if (prefab == "")
		{
			RestoreDeployInvincibility(pilot);
			return;
		}

		EntitySpawnParams sp = new EntitySpawnParams;
		sp.TransformMode = ETransformMode.WORLD;
		pilot.GetWorldTransform(sp.Transform);

		IEntity spawned = GetGame().SpawnEntityPrefabEx(prefab, false, GetGame().GetWorld(), sp);
		ParachuteDeployedEntity chute = ParachuteDeployedEntity.Cast(spawned);
		if (!chute)
		{
			RestoreDeployInvincibility(pilot);
			return;
		}

		item.SetParachuteUsed_Server();

		m_DeployedParachute = chute;
		m_ParachuteItem = item;

		GiveChuteOwnershipToController(chute);

		BaseCompartmentManagerComponent bcm = BaseCompartmentManagerComponent.Cast(chute.FindComponent(BaseCompartmentManagerComponent));
		if (!bcm)
		{
			RestoreDeployInvincibility(pilot);
			return;
		}

		array<BaseCompartmentSlot> slots = {};
		bcm.GetCompartments(slots);

		BaseCompartmentSlot pilotSlot = null;
		foreach (BaseCompartmentSlot s : slots)
		{
			if (!s) continue;
			if (s.GetType() == ECompartmentType.CARGO)
			{
				pilotSlot = s;
				break;
			}
		}

		if (!pilotSlot)
		{
			RestoreDeployInvincibility(pilot);
			return;
		}

		m_vDeployVelocity = pilot.GetPhysics().GetVelocity();

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(
			pilot.FindComponent(SCR_CompartmentAccessComponent));
		chute.InitializePilot(pilot, access, m_vDeployVelocity);

		// Start invincibility BEFORE GetInVehicle so PhysicsBlock prevents pilot-chute collision during transition
		ParachuteDeployedEntityExtended chuteExt = ParachuteDeployedEntityExtended.Cast(chute);
		if (chuteExt)
			chuteExt.StartDeployInvincibility(m_fDeployInvincibilityDuration);

		if (access)
			access.GetInVehicle(chute, pilotSlot, true, 0, ECloseDoorAfterActions.INVALID, true);

		m_DeployedChuteId = chute.GetRplId();
		m_iChuteSlotId = pilotSlot.GetCompartmentSlotID();
		m_bParachuteDeployed = true;

		Replication.BumpMe();
		GetGame().GetCallqueue().CallLater(Do_SetupDeployedChute_Owner, 50, false, m_DeployedChuteId, m_iChuteSlotId, m_vDeployVelocity);
		GetGame().GetCallqueue().CallLater(RestoreDeployInvincibility, (int)(m_fDeployInvincibilityDuration * 1000), false, pilot);
	}

	override void Do_SetupDeployedChute_Owner(RplId chuteId, int slotId, vector deployVel)
	{
		Rpc(RpcDo_SetupDeployedChute_Owner, chuteId, slotId, deployVel);
	}

	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		SCR_ChimeraCharacter pilot = SCR_ChimeraCharacter.Cast(to);
		if (!pilot)
		{
			m_PilotEntity = null;
			m_CompartmentAccess = null;
			m_ParachuteItem = null;
			return;
		}

		m_PilotEntity = to;
		m_CompartmentAccess = SCR_CompartmentAccessComponent.Cast(to.FindComponent(SCR_CompartmentAccessComponent));
		RefreshParachuteItemFromInventory(to);

		if (!IsAuthority() || !m_bParachuteDeployed || !m_DeployedParachute)
			return;

		ParachuteDeployedEntityExtended chuteExt = ParachuteDeployedEntityExtended.Cast(m_DeployedParachute);
		if (chuteExt && chuteExt.IsDeployInvincibilityActive())
		{
			// #region agent log
			DebugChuteLog("[DEBUG_CHUTE] H5 OnControlledEntityChanged_skipped_invincibility");
			// #endregion
			return;
		}

		// #region agent log
		DebugChuteLog("[DEBUG_CHUTE] H5 OnControlledEntityChanged_deleting_chute");
		// #endregion
		DeleteParachuteEntity(m_DeployedParachute);
		m_DeployedParachute = null;
		m_bParachuteDeployed = false;
		m_DeployedChuteId = RplId.Invalid();
		Replication.BumpMe();
	}

	void RespawnChuteForDisconnectedPilot(ParachuteDeployedEntity oldChute, IEntity pilot)
	{
		// #region agent log
		DebugChuteLog("[DEBUG_CHUTE] H3 RespawnChuteForDisconnectedPilot_entry");
		// #endregion
		if (!oldChute || !pilot)
		{
			// #region agent log
			DebugChuteLog("[DEBUG_CHUTE] H3 Respawn_early reason=!oldChute_or_!pilot");
			// #endregion
			return;
		}

		ParachuteItemComponent item = ResolveParachuteItem_Server(pilot);
		if (!item)
		{
			// #region agent log
			DebugChuteLog("[DEBUG_CHUTE] H3 Respawn_early reason=ResolveParachuteItem_Server_null");
			// #endregion
			return;
		}

		ResourceName prefab = item.GetParachutePrefab();
		if (prefab == "")
		{
			// #region agent log
			DebugChuteLog("[DEBUG_CHUTE] H3 Respawn_early reason=prefab_empty");
			// #endregion
			return;
		}

		EntitySpawnParams sp = new EntitySpawnParams;
		sp.TransformMode = ETransformMode.WORLD;
		pilot.GetWorldTransform(sp.Transform);

		IEntity spawned = GetGame().SpawnEntityPrefabEx(prefab, false, GetGame().GetWorld(), sp);
		ParachuteDeployedEntity chute = ParachuteDeployedEntity.Cast(spawned);
		if (!chute)
		{
			// #region agent log
			DebugChuteLog("[DEBUG_CHUTE] H4 Respawn_early reason=spawn_failed");
			// #endregion
			return;
		}

		BaseCompartmentManagerComponent bcm = BaseCompartmentManagerComponent.Cast(chute.FindComponent(BaseCompartmentManagerComponent));
		if (!bcm)
		{
			// #region agent log
			DebugChuteLog("[DEBUG_CHUTE] H4 Respawn_early reason=!bcm");
			// #endregion
			return;
		}

		array<BaseCompartmentSlot> slots = {};
		bcm.GetCompartments(slots);

		BaseCompartmentSlot pilotSlot = null;
		foreach (BaseCompartmentSlot s : slots)
		{
			if (!s) continue;
			if (s.GetType() == ECompartmentType.CARGO)
			{
				pilotSlot = s;
				break;
			}
		}

		if (!pilotSlot)
		{
			// #region agent log
			DebugChuteLog("[DEBUG_CHUTE] H4 Respawn_early reason=!pilotSlot");
			// #endregion
			return;
		}

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(
			pilot.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
		{
			// #region agent log
			DebugChuteLog("[DEBUG_CHUTE] H4 Respawn_early reason=!access");
			// #endregion
			return;
		}

		vector deployVel = pilot.GetPhysics().GetVelocity();
		m_vDeployVelocity = deployVel;

		m_DeployedParachute = chute;
		m_DeployedChuteId = chute.GetRplId();
		m_iChuteSlotId = pilotSlot.GetCompartmentSlotID();

		GiveChuteOwnershipToController(chute);
		chute.InitializePilot(pilot, access, deployVel);
		access.GetInVehicle(chute, pilotSlot, true, 0, ECloseDoorAfterActions.INVALID, true);

		// #region agent log
		DebugChuteLog("[DEBUG_CHUTE] H4 RespawnChuteForDisconnectedPilot_success");
		// #endregion
		Replication.BumpMe();
		GetGame().GetCallqueue().CallLater(Do_SetupDeployedChute_Owner, 50, false, m_DeployedChuteId, m_iChuteSlotId, deployVel);
		GetGame().GetCallqueue().CallLater(SCR_EntityHelper.DeleteEntityAndChildren, 100, false, oldChute);
	}

	override void Rpc_ServerExitParachute(RplId chuteId, float velocityAtExit)
	{
		// If chute was destroyed during deploy invincibility (e.g. pilot collision), skip landing damage
		ParachuteDeployedEntityExtended chuteExt = ParachuteDeployedEntityExtended.Cast(m_DeployedParachute);
		if (chuteExt && chuteExt.IsDeployInvincibilityActive())
		{
			super.Rpc_ServerExitParachute(chuteId, 0.0);
			return;
		}

		if (velocityAtExit >= m_fHardLandingVelocity && m_PlayerController)
		{
			IEntity pilot = SCR_ChimeraCharacter.Cast(m_PlayerController.GetMainEntity());
			if (pilot)
				RestoreDeployInvincibility(pilot);
		}

		super.Rpc_ServerExitParachute(chuteId, velocityAtExit);
	}
}
