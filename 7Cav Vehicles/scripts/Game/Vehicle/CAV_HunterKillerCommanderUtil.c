//------------------------------------------------------------------------------------------------
//! Hunter-Killer: local player seat gates and consciousness (shared by component + HUD hint pattern).
//! CAV_M2A3_HunterKillerComponent must live on the compartment controller entity only (commander CITV).
//! Do not search child turrets: gunner seat owner is often the main turret parent, whose child CITV has HK — that falsely matched gunners.
class CAV_HunterKillerCommanderUtil
{
	//------------------------------------------------------------------------------------------------
	//! True only if the given entity directly hosts CAV_M2A3_HunterKillerComponent (commander CITV prefab).
	static bool EntityHasHunterKillerComponent(IEntity root)
	{
		if (!root)
			return false;
		return CAV_M2A3_HunterKillerComponent.Cast(root.FindComponent(CAV_M2A3_HunterKillerComponent)) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Local player in a turret seat: 0=ok with outPlayerTurretEnt set; else 1..5.
	static int TurretSeatBaseFailReason(out IEntity outPlayerTurretEnt)
	{
		outPlayerTurretEnt = null;
		IEntity controlled = SCR_PlayerController.GetLocalControlledEntity();
		if (!controlled)
			return 1;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(controlled.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return 2;

		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot)
			return 3;

		if (slot.GetType() != ECompartmentType.TURRET)
			return 4;

		BaseControllerComponent slotCtrl = slot.GetController();
		if (!slotCtrl)
			return 5;

		outPlayerTurretEnt = slotCtrl.GetOwner();
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! 0=ok 1=no_controlled 2=no_access 3=no_slot 4=not_turret 5=no_slot_controller 6=no_hunter_killer_on_turret
	static int CommanderGateFailReason()
	{
		IEntity playerTurretEnt;
		int code = TurretSeatBaseFailReason(playerTurretEnt);
		if (code != 0)
			return code;
		if (!EntityHasHunterKillerComponent(playerTurretEnt))
			return 6;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Gunner main turret for vehicle whose commander CITV is commanderTurretEnt (parent of main turret).
	//! 0=ok 1..5 base 6=no_turret_owner 7=no_cmd_ent 8=not_main_parent_of_cmd
	static int GunnerGateFailReason(IEntity commanderTurretEnt)
	{
		IEntity playerTurretEnt;
		int code = TurretSeatBaseFailReason(playerTurretEnt);
		if (code != 0)
			return code;
		if (!playerTurretEnt)
			return 6;
		if (!commanderTurretEnt)
			return 7;
		if (playerTurretEnt != commanderTurretEnt.GetParent())
			return 8;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Local controlled pawn: alive and not unconscious (IsUnconscious can stay true during wake animation).
	static bool IsLocalCommanderConsciousAndAlive()
	{
		IEntity controlled = SCR_PlayerController.GetLocalControlledEntity();
		if (!controlled)
			return false;
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(controlled);
		if (!ch)
			return true;
		CharacterControllerComponent ccc = ch.GetCharacterController();
		if (!ccc)
			return true;
		if (ccc.IsDead())
			return false;
		if (ccc.IsUnconscious())
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Commander HK seat + conscious + alive.
	static bool IsLocalControlledCommanderFitForHunterKiller()
	{
		if (CommanderGateFailReason() != 0)
			return false;
		return IsLocalCommanderConsciousAndAlive();
	}
}
