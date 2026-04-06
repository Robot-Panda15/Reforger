//------------------------------------------------------------------------------------------------
//! Shared authority checks for timed entity delete (HUD markers, placed designations, etc.).
class HMD_MarkerLifetimeAuthority
{
	//------------------------------------------------------------------------------------------------
	//! Offline / host / master: run timed delete. Pure network proxies must not delete replicated entities.
	static bool ShouldRunTimedEntityDeleteAuthority(IEntity owner)
	{
		if (!Replication.IsRunning())
			return true;
		if (Replication.IsServer())
			return true;
		if (!owner)
			return false;
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (!rpl)
			return true;
		if (rpl.IsMaster())
			return true;
		if (!rpl.IsProxy())
			return true;
		return false;
	}
}
