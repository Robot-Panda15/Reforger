class ParachuteDeployedEntityExtendedClass : ParachuteDeployedEntityClass {}
class ParachuteDeployedEntityExtended : ParachuteDeployedEntity
{
	protected bool m_bDeployInvincibilityActive;
	protected PhysicsBlock m_InvincibilityPhysicsBlock;
	protected float m_fEmptyCompartmentAccumulator = -1;

	bool IsDeployInvincibilityActive()
	{
		return m_bDeployInvincibilityActive;
	}

	void StartDeployInvincibility(float durationSeconds)
	{
		m_bDeployInvincibilityActive = true;

		DamageManagerComponent dmgChute = DamageManagerComponent.Cast(m_DamageManager);
		if (dmgChute)
			dmgChute.EnableDamageHandling(false);

		if (m_Pilot)
		{
			m_InvincibilityPhysicsBlock = PhysicsBlock.Create(this, m_Pilot);
		}

		GetGame().GetCallqueue().CallLater(EndDeployInvincibility, (int)(durationSeconds * 1000), false);
	}

	protected void EndDeployInvincibility()
	{
		m_bDeployInvincibilityActive = false;

		if (m_InvincibilityPhysicsBlock)
		{
			m_InvincibilityPhysicsBlock.Remove(this);
			m_InvincibilityPhysicsBlock = null;
		}

		DamageManagerComponent dmgChute = DamageManagerComponent.Cast(m_DamageManager);
		if (dmgChute)
			dmgChute.EnableDamageHandling(true);

		ReattachPilotIfDisconnected();
	}

	protected void ReattachPilotIfDisconnected()
	{
		if (!IsAuthority())
			return;

		if (!m_Compartment || !m_Pilot)
			return;

		if (m_Compartment.IsOccupied())
			return;

		if (m_bHasLanded)
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		int playerId = pm.GetPlayerIdFromControlledEntity(m_Pilot);
		if (playerId == 0)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(pm.GetPlayerController(playerId));
		if (!pc)
			return;

		ParachuteComponentExtended parachuteComp = ParachuteComponentExtended.Cast(pc.FindComponent(ParachuteComponent));
		if (!parachuteComp)
			return;

		parachuteComp.RespawnChuteForDisconnectedPilot(this, m_Pilot);
	}

	protected bool IsPilotOrPilotChild(IEntity other)
	{
		if (!other)
			return false;

		IEntity pilot = null;
		if (m_Compartment)
			pilot = m_Compartment.GetOccupant();
		if (!pilot)
			pilot = m_Pilot;
		if (!pilot)
			return false;

		if (other == pilot)
			return true;

		IEntity p = other.GetParent();
		while (p)
		{
			if (p == pilot)
				return true;
			p = p.GetParent();
		}

		return false;
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);

		if (!IsAuthority() || !m_Compartment || m_bDeployInvincibilityActive || m_bIsDestroyed)
			return;

		if (m_Compartment.IsOccupied())
		{
			m_fEmptyCompartmentAccumulator = -1;
			return;
		}

		if (m_fEmptyCompartmentAccumulator < 0)
			m_fEmptyCompartmentAccumulator = 0;

		m_fEmptyCompartmentAccumulator = m_fEmptyCompartmentAccumulator + timeSlice;

		if (m_fEmptyCompartmentAccumulator >= 5)
			DestroyParachute();
	}

	override void EOnContact(IEntity owner, IEntity other, Contact contact)
	{
		// Contact no longer triggers exit - player must manually get out
	}

	override void SetPitch(float value = 0.0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_bDeployInvincibilityActive)
			return;
		super.SetPitch(value, reason, actionName);
	}

	override void SetRoll(float value = 0.0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_bDeployInvincibilityActive)
			return;
		super.SetRoll(value, reason, actionName);
	}

	override void DestroyParachute()
	{
		if (m_bIsDestroyed)
			return;

		m_bIsDestroyed = true;

		if (m_InvincibilityPhysicsBlock)
		{
			m_InvincibilityPhysicsBlock.Remove(this);
			m_InvincibilityPhysicsBlock = null;
		}

		if (m_Compartment && m_Compartment.IsOccupied())
		{
			AskServerExit();
			// Do NOT delete here - player is parented to chute. Rpc_ServerExitParachute will
			// unparent the player via AskOwnerToGetOutFromVehicle, then delete the chute.
		}
		else
		{
			GetGame().GetCallqueue().CallLater(SCR_EntityHelper.DeleteEntityAndChildren, 100, false, this);
		}
	}
}
