//------------------------------------------------------------------------------------------------
//! Server-only world system: authoritative IFF beacon table for purge (no Rpc HUD delivery).
//! Clients draw IFF from replicated entities via local HUDMarkerSystem / RegisterIffMarker.
class HMD_HudMarkerAuthoritySystem : GameSystem
{
	protected static const float AUTH_POS_RESYNC = 0.5;
	protected float m_fAuthPosAccum;

	protected ref array<RplId> m_AuthRplIds = {};
	protected ref array<IEntity> m_AuthEntities = {};
	protected ref array<vector> m_AuthPositions = {};
	protected ref array<string> m_AuthLabels = {};
	protected ref array<float> m_AuthLifeSec = {};
	protected ref array<int> m_AuthDot = {};
	protected ref array<int> m_AuthLbl = {};
	protected ref array<float> m_AuthVis = {};

	//------------------------------------------------------------------------------------------------
	static HMD_HudMarkerAuthoritySystem GetInstance(ChimeraWorld world)
	{
		if (!world)
			return null;
		return HMD_HudMarkerAuthoritySystem.Cast(world.FindSystem(HMD_HudMarkerAuthoritySystem));
	}

	//------------------------------------------------------------------------------------------------
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		outInfo.SetAbstract(false)
			.SetUnique(true)
			.SetLocation(WorldSystemLocation.Server)
			.AddPoint(WorldSystemPoint.Frame);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnInit()
	{
		super.OnInit();
		Enable(true);
	}

	//------------------------------------------------------------------------------------------------
	protected static int FindAuthIdxByRplId(array<RplId> ids, RplId want)
	{
		for (int i = 0; i < ids.Count(); i++)
		{
			if (ids[i] == want)
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool PosNearlyEqual(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dy = a[1] - b[1];
		float dz = a[2] - b[2];
		return dx * dx + dy * dy + dz * dz < 0.000001;
	}

	//------------------------------------------------------------------------------------------------
	protected void AuthorityRemoveAt(int idx)
	{
		m_AuthRplIds.RemoveOrdered(idx);
		m_AuthEntities.RemoveOrdered(idx);
		m_AuthPositions.RemoveOrdered(idx);
		m_AuthLabels.RemoveOrdered(idx);
		m_AuthLifeSec.RemoveOrdered(idx);
		m_AuthDot.RemoveOrdered(idx);
		m_AuthLbl.RemoveOrdered(idx);
		m_AuthVis.RemoveOrdered(idx);
	}

	//------------------------------------------------------------------------------------------------
	protected void AuthorityPurgeOrphanEntityRows()
	{
		ChimeraWorld w = GetWorld();
		if (!w)
			return;
		for (int i = m_AuthRplIds.Count() - 1; i >= 0; i--)
		{
			IEntity e = m_AuthEntities[i];
			if (e && e.GetWorld())
				continue;
			AuthorityRemoveAt(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AuthorityTickPositions()
	{
		ChimeraWorld w = GetWorld();
		if (!w)
			return;
		AuthorityPurgeOrphanEntityRows();
		for (int i = 0; i < m_AuthRplIds.Count(); i++)
		{
			IEntity e = m_AuthEntities[i];
			if (!e)
				continue;
			vector p = e.GetOrigin();
			if (PosNearlyEqual(p, m_AuthPositions[i]))
				continue;
			m_AuthPositions[i] = p;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AuthorityReplayAllIffRowsToClients()
	{
		ChimeraWorld w = GetWorld();
		if (!w)
			return;
		AuthorityPurgeOrphanEntityRows();
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only: track IFF beacon row for purge (HUD uses local Register / Iff pool on each client).
	void AuthorityRegisterOrUpdateIff(IEntity ent, RplId rid, string label, float lifeSec, int dotARGB, int labelARGB, float visDist)
	{
		if (!Replication.IsRunning() || !Replication.IsServer())
			return;
		ChimeraWorld w = GetWorld();
		if (!w)
			return;
		if (rid == RplId.Invalid())
			return;
		vector pos = vector.Zero;
		if (ent && ent.GetWorld())
			pos = ent.GetOrigin();
		int idx = FindAuthIdxByRplId(m_AuthRplIds, rid);
		if (idx < 0)
		{
			m_AuthRplIds.Insert(rid);
			m_AuthEntities.Insert(ent);
			m_AuthPositions.Insert(pos);
			m_AuthLabels.Insert(label);
			m_AuthLifeSec.Insert(lifeSec);
			m_AuthDot.Insert(dotARGB);
			m_AuthLbl.Insert(labelARGB);
			m_AuthVis.Insert(visDist);
		}
		else
		{
			m_AuthEntities[idx] = ent;
			m_AuthPositions[idx] = pos;
			m_AuthLabels[idx] = label;
			m_AuthLifeSec[idx] = lifeSec;
			m_AuthDot[idx] = dotARGB;
			m_AuthLbl[idx] = labelARGB;
			m_AuthVis[idx] = visDist;
		}
	}

	//------------------------------------------------------------------------------------------------
	void AuthorityRemoveIff(IEntity ent, RplId rid)
	{
		if (!Replication.IsRunning() || !Replication.IsServer())
			return;
		ChimeraWorld w = GetWorld();
		if (!w)
			return;
		if (rid == RplId.Invalid())
			return;
		int idx = FindAuthIdxByRplId(m_AuthRplIds, rid);
		if (idx >= 0)
			AuthorityRemoveAt(idx);
	}

	//------------------------------------------------------------------------------------------------
	void HandleRpcAsk_DesignatorViewportSync()
	{
		if (!Replication.IsServer())
			return;
		AuthorityReplayAllIffRowsToClients();
	}

	//------------------------------------------------------------------------------------------------
	void HandleRpcAsk_RequestFullIffSnapshot()
	{
		if (!Replication.IsServer())
			return;
		AuthorityPurgeOrphanEntityRows();
		AuthorityReplayAllIffRowsToClients();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnUpdate(ESystemPoint point)
	{
		super.OnUpdate(point);
		if (point != ESystemPoint.Frame)
			return;
		if (!GetGame() || !GetGame().InPlayMode())
			return;
		if (!Replication.IsRunning() || !Replication.IsServer())
			return;
		ChimeraWorld w = GetWorld();
		if (!w)
			return;
		m_fAuthPosAccum += w.GetTimeSlice();
		if (m_fAuthPosAccum < AUTH_POS_RESYNC)
			return;
		m_fAuthPosAccum = 0;
		AuthorityTickPositions();
	}
}
