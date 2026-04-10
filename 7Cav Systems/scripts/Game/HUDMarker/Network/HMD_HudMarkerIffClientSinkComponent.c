//------------------------------------------------------------------------------------------------
//! Player controller: designator viewport Rpc; legacy IFF Rpc handlers (no-op local pool apply). IFF uses RegisterIffMarker on clients from replicated entities.
[ComponentEditorProps(category: "HMD", description: "HMD Rpc plumbing on player controller (IFF HUD uses local pool).")]
class HMD_HudMarkerIffClientSinkComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class HMD_HudMarkerIffClientSinkComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	static HMD_HudMarkerIffClientSinkComponent FindSinkForPlayerId(PlayerManager pm, int playerId)
	{
		if (!pm)
			return null;
		PlayerController pcb = pm.GetPlayerController(playerId);
		SCR_PlayerController spc = SCR_PlayerController.Cast(pcb);
		if (!spc)
			return null;
		return HMD_HudMarkerIffClientSinkComponent.Cast(spc.FindComponent(HMD_HudMarkerIffClientSinkComponent));
	}

	//------------------------------------------------------------------------------------------------
	static HMD_HudMarkerIffClientSinkComponent GetLocalSink()
	{
		if (!GetGame())
			return null;
		PlayerController pl = GetGame().GetPlayerController();
		if (!pl)
			return null;
		SCR_PlayerController spc = SCR_PlayerController.Cast(pl);
		if (!spc)
			return null;
		return HMD_HudMarkerIffClientSinkComponent.Cast(spc.FindComponent(HMD_HudMarkerIffClientSinkComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Dedicated server: at least one player controller has this sink (transport ready).
	static bool ServerHasAnySink()
	{
		if (!GetGame() || !Replication.IsServer())
			return false;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return false;
		array<int> pids = {};
		pm.GetAllPlayers(pids);
		foreach (int pid : pids)
		{
			if (FindSinkForPlayerId(pm, pid))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: push one IFF row to every connected player's client (Owner Rpc per controller).
	static void ServerBroadcastUpsertToAll(ChimeraWorld world, RplId id, vector pos, string label, float lifeSec, int markerColor, int labelColor, float visDist)
	{
		if (!world || !Replication.IsServer() || !GetGame())
			return;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;
		array<int> pids = {};
		pm.GetAllPlayers(pids);
		foreach (int pid : pids)
		{
			HMD_HudMarkerIffClientSinkComponent sink = FindSinkForPlayerId(pm, pid);
			if (sink)
				sink.AuthorityRpc_IffUpsertFull(id, pos, label, lifeSec, markerColor, labelColor, visDist);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ServerBroadcastRemoveToAll(ChimeraWorld world, RplId id)
	{
		if (!world || !Replication.IsServer() || !GetGame())
			return;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;
		array<int> pids = {};
		pm.GetAllPlayers(pids);
		foreach (int pid : pids)
		{
			HMD_HudMarkerIffClientSinkComponent sink = FindSinkForPlayerId(pm, pid);
			if (sink)
				sink.AuthorityRpc_IffRemove(id);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ServerBroadcastPositionToAll(ChimeraWorld world, RplId id, vector pos)
	{
		if (!world || !Replication.IsServer() || !GetGame())
			return;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;
		array<int> pids = {};
		pm.GetAllPlayers(pids);
		foreach (int pid : pids)
		{
			HMD_HudMarkerIffClientSinkComponent sink = FindSinkForPlayerId(pm, pid);
			if (sink)
				sink.AuthorityRpc_IffPosition(id, pos);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only on this controller instance; dedicated clients get RpcDo_*; listen host also applies here (same process as server).
	void AuthorityRpc_IffUpsertFull(RplId id, vector pos, string label, float lifeSec, int markerColor, int labelColor, float visDist)
	{
		if (!Replication.IsServer())
			return;
		Rpc(RpcDo_IffUpsertFull, id, pos, label, lifeSec, markerColor, labelColor, visDist);
		if (RplSession.Mode() != RplMode.Dedicated && Replication.IsClient())
			ApplyServerIffUpsertToLocalHud(id, pos, label, lifeSec, markerColor, labelColor, visDist);
	}

	//------------------------------------------------------------------------------------------------
	void AuthorityRpc_IffRemove(RplId id)
	{
		if (!Replication.IsServer())
			return;
		Rpc(RpcDo_IffRemove, id);
		if (RplSession.Mode() != RplMode.Dedicated && Replication.IsClient())
			ApplyServerIffRemoveToLocalHud(id);
	}

	//------------------------------------------------------------------------------------------------
	void AuthorityRpc_IffPosition(RplId id, vector pos)
	{
		if (!Replication.IsServer())
			return;
		Rpc(RpcDo_IffPosition, id, pos);
		if (RplSession.Mode() != RplMode.Dedicated && Replication.IsClient())
			ApplyServerIffPositionToLocalHud(id, pos);
	}

	//------------------------------------------------------------------------------------------------
	void ClientRequestDesignatorViewportSync()
	{
		if (!Replication.IsRunning() || !Replication.IsClient())
			return;
		Rpc(RpcAsk_DesignatorViewportSync);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (GetGame())
			GetGame().GetCallqueue().CallLater(DelayedClientJipAsk, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void DelayedClientJipAsk()
	{
		if (!Replication.IsRunning() || !Replication.IsClient())
			return;
		Rpc(RpcAsk_RequestFullIffSnapshot);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DesignatorViewportSync()
	{
		ChimeraWorld w = GetGame().GetWorld();
		if (!w)
			return;
		HMD_HudMarkerAuthoritySystem auth = HMD_HudMarkerAuthoritySystem.GetInstance(w);
		if (auth)
			auth.HandleRpcAsk_DesignatorViewportSync();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RequestFullIffSnapshot()
	{
		ChimeraWorld w = GetGame().GetWorld();
		if (!w)
			return;
		HMD_HudMarkerAuthoritySystem auth = HMD_HudMarkerAuthoritySystem.GetInstance(w);
		if (auth)
			auth.HandleRpcAsk_RequestFullIffSnapshot();
	}

	//------------------------------------------------------------------------------------------------
	//! Legacy Rpc path (unused). If re-enabled, upsert must write the full IffMarker pool row.
	protected void ApplyServerIffUpsertToLocalHud(RplId id, vector pos, string label, float lifeSec, int markerColor, int labelColor, float visDist)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyServerIffRemoveToLocalHud(RplId id)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyServerIffPositionToLocalHud(RplId id, vector pos)
	{
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_IffUpsertFull(RplId id, vector pos, string label, float lifeSec, int markerColor, int labelColor, float visDist)
	{
		if (!Replication.IsClient())
			return;
		ApplyServerIffUpsertToLocalHud(id, pos, label, lifeSec, markerColor, labelColor, visDist);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_IffRemove(RplId id)
	{
		if (!Replication.IsClient())
			return;
		ApplyServerIffRemoveToLocalHud(id);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_IffPosition(RplId id, vector pos)
	{
		if (!Replication.IsClient())
			return;
		ApplyServerIffPositionToLocalHud(id, pos);
	}
}
