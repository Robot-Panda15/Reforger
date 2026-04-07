//------------------------------------------------------------------------------------------------
//! Shared authority + ChimeraWorld.GetServerTimestamp elapsed for timed delete (HUD markers, placed designations, etc.).
class HMD_MarkerLifetimeAuthority
{
	//------------------------------------------------------------------------------------------------
	//! Seconds since start using server time (not mission time / not frame timeSlice). Clamps negative diffs to 0.
	static float GetElapsedSecondsSinceServerTime(WorldTimestamp start, ChimeraWorld world)
	{
		if (!world)
			return 0;
		WorldTimestamp now = world.GetServerTimestamp();
		float ms = now.DiffMilliseconds(start);
		if (ms < 0)
			ms = 0;
		return ms * 0.001;
	}

	//------------------------------------------------------------------------------------------------
	//! Offline: run timed delete. Online: server only for replicated entities; local non-replicated may delete on client.
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
		return false;
	}
}
