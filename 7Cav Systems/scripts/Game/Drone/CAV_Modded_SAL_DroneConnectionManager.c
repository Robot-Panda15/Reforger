//------------------------------------------------------------------------------------------------
//! Extension only: base SAL_DroneConnectionManager is not edited.
//! On server, after a successful connect, set the drone's FactionAffiliationComponent to the player's faction.
//------------------------------------------------------------------------------------------------
class CAV_SALDroneFactionConnectHelper
{
	//------------------------------------------------------------------------------------------------
	static IEntity EntityFromRplId(RplId id)
	{
		if (id == RplId.Invalid())
			return null;
		if (!Replication.FindItem(id))
			return null;
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if (!rpl)
			return null;
		return rpl.GetEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! Root, parents, direct children — affiliation is not always on the replicated drone root.
	static FactionAffiliationComponent FindAffiliationInHierarchy(IEntity start)
	{
		if (!start)
			return null;

		FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(start.FindComponent(FactionAffiliationComponent));
		if (aff)
			return aff;

		IEntity p = start.GetParent();
		while (p)
		{
			aff = FactionAffiliationComponent.Cast(p.FindComponent(FactionAffiliationComponent));
			if (aff)
				return aff;
			p = p.GetParent();
		}

		IEntity c = start.GetChildren();
		while (c)
		{
			aff = FactionAffiliationComponent.Cast(c.FindComponent(FactionAffiliationComponent));
			if (aff)
				return aff;
			c = c.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static void ApplyPlayerFactionToDrone(IEntity drone, int playerId)
	{
		if (!drone || playerId < 0)
			return;
		if (!Replication.IsServer())
			return;
		if (!GetGame() || !GetGame().InPlayMode())
			return;

		Faction playerFaction = SCR_FactionManager.SGetPlayerFaction(playerId);
		if (!playerFaction)
			return;

		FactionAffiliationComponent aff = FindAffiliationInHierarchy(drone);
		if (!aff)
			return;

		aff.SetAffiliatedFaction(playerFaction);
	}
}

//------------------------------------------------------------------------------------------------
modded class SAL_DroneConnectionManager
{
	//------------------------------------------------------------------------------------------------
	override void ConnectToDrone(RplId droneId, int playerId)
	{
		super.ConnectToDrone(droneId, playerId);

		if (!Replication.IsServer())
			return;

		IEntity drone = CAV_SALDroneFactionConnectHelper.EntityFromRplId(droneId);
		if (!drone)
			return;

		CAV_SALDroneFactionConnectHelper.ApplyPlayerFactionToDrone(drone, playerId);
	}
}
