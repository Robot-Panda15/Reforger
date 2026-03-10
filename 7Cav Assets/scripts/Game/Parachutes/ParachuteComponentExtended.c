class ParachuteComponentExtendedClass : ParachuteComponentClass {}
class ParachuteComponentExtended : ParachuteComponent
{
	[Attribute("3.0", UIWidgets.Slider, "Deploy invincibility duration (s)", "0.5 10 0.5", category : "Landing")]
	protected float m_fDeployInvincibilityDuration;

	protected bool IsChuteCompartmentEmpty(IEntity chute)
	{
		if (!chute)
			return true;
		BaseCompartmentManagerComponent bcm = BaseCompartmentManagerComponent.Cast(chute.FindComponent(BaseCompartmentManagerComponent));
		if (!bcm)
			return true;
		array<BaseCompartmentSlot> slots = {};
		bcm.GetCompartments(slots);
		foreach (BaseCompartmentSlot s : slots)
		{
			if (s && s.GetType() == ECompartmentType.CARGO)
				return !s.IsOccupied();
		}
		return true;
	}

	void DeleteParachuteEntityWhenEmpty(IEntity chute, int retryCount)
	{
		if (!chute)
		{
			ClearExitState();
			return;
		}
		if (retryCount >= 40)
		{
			DeleteParachuteEntity(chute);
			ClearExitState();
			return;
		}
		if (IsChuteCompartmentEmpty(chute))
		{
			DeleteParachuteEntity(chute);
			ClearExitState();
			return;
		}
		GetGame().GetCallqueue().CallLater(DeleteParachuteEntityWhenEmpty, 50, true, chute, retryCount + 1);
	}

	void ClearExitState()
	{
		m_DeployedParachute = null;
		m_bParachuteDeployed = false;
		m_DeployedChuteId = RplId.Invalid();
		Replication.BumpMe();
	}

	override void RpcAskDeployParachute()
	{
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
			return;

		DeleteParachuteEntity(m_DeployedParachute);
		m_DeployedParachute = null;
		m_bParachuteDeployed = false;
		m_DeployedChuteId = RplId.Invalid();
		Replication.BumpMe();
	}

	void RespawnChuteForDisconnectedPilot(ParachuteDeployedEntity oldChute, IEntity pilot)
	{
		if (!oldChute || !pilot)
			return;

		ParachuteItemComponent item = ResolveParachuteItem_Server(pilot);
		if (!item)
			return;

		ResourceName prefab = item.GetParachutePrefab();
		if (prefab == "")
			return;

		EntitySpawnParams sp = new EntitySpawnParams;
		sp.TransformMode = ETransformMode.WORLD;
		pilot.GetWorldTransform(sp.Transform);

		IEntity spawned = GetGame().SpawnEntityPrefabEx(prefab, false, GetGame().GetWorld(), sp);
		ParachuteDeployedEntity chute = ParachuteDeployedEntity.Cast(spawned);
		if (!chute)
			return;

		BaseCompartmentManagerComponent bcm = BaseCompartmentManagerComponent.Cast(chute.FindComponent(BaseCompartmentManagerComponent));
		if (!bcm)
			return;

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
			return;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(
			pilot.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return;

		vector deployVel = pilot.GetPhysics().GetVelocity();
		m_vDeployVelocity = deployVel;

		m_DeployedParachute = chute;
		m_DeployedChuteId = chute.GetRplId();
		m_iChuteSlotId = pilotSlot.GetCompartmentSlotID();

		GiveChuteOwnershipToController(chute);
		chute.InitializePilot(pilot, access, deployVel);
		access.GetInVehicle(chute, pilotSlot, true, 0, ECloseDoorAfterActions.INVALID, true);

		Replication.BumpMe();
		GetGame().GetCallqueue().CallLater(Do_SetupDeployedChute_Owner, 50, false, m_DeployedChuteId, m_iChuteSlotId, deployVel);
		GetGame().GetCallqueue().CallLater(SCR_EntityHelper.DeleteEntityAndChildren, 100, false, oldChute);
	}

	override void Rpc_ServerExitParachute(RplId chuteId, float velocityAtExit)
	{
		if (!IsAuthority())
			return;

		if (!m_bParachuteDeployed)
			return;

		if (chuteId != m_DeployedChuteId)
			return;

		ParachuteDeployedEntityExtended chuteExt = ParachuteDeployedEntityExtended.Cast(m_DeployedParachute);
		bool skipDamage = chuteExt && chuteExt.IsDeployInvincibilityActive();

		if (!skipDamage)
		{
			if (velocityAtExit >= m_fHardLandingVelocity && m_PlayerController)
			{
				IEntity pilot = SCR_ChimeraCharacter.Cast(m_PlayerController.GetMainEntity());
				if (pilot)
					RestoreDeployInvincibility(pilot);
			}
			if (velocityAtExit >= m_fHardLandingVelocity && velocityAtExit < m_fDeathLandingVelocity)
				BreakLegs_Server();
			else if (velocityAtExit >= m_fDeathLandingVelocity)
				KillPlayer_Server();
		}

		if (m_CompartmentAccess)
			m_CompartmentAccess.AskOwnerToGetOutFromVehicle(EGetOutType.TELEPORT, 0, ECloseDoorAfterActions.LEAVE_OPEN, true, true);

		IEntity chuteToDelete = m_DeployedParachute;
		ClearExitState();
		Rpc(RpcDo_OnParachuteCleared);
		GetGame().GetCallqueue().CallLater(DeleteParachuteEntityWhenEmpty, 50, true, chuteToDelete, 0);
	}
}
