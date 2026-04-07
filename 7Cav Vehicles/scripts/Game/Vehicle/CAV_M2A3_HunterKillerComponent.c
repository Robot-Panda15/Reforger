//------------------------------------------------------------------------------------------------
//! Hunter-Killer (T): **gunner** main turret slews toward optic LOS (commander TurretComponent / pivot). Hull-relative lock when possible.
//! Main slew: **yaw first**, then **elevation (pitch) only** with yaw held at target. Slew rate **deg/s** per axis (frame dt or tick).
//! Attached to commander (CITV) turret entity; parent must carry the main TurretComponent.
//! Input: CAV_HunterKillerSlew (Keypad 2, commander), CAV_HunterKillerCancel (Keypad 3, commander + gunner) in chimeraInputCommon.conf - merge into vanilla file if controls break.
//------------------------------------------------------------------------------------------------

class CAV_M2A3_HunterKillerComponentClass : ScriptComponentClass
{
}

[ComponentEditorProps(category: "Vehicle", description: "M2A3 II commander slew-to-optic (hunter-killer)")]
class CAV_M2A3_HunterKillerComponent : ScriptComponent
{
	[Attribute(defvalue: "v_upgrade_optic", uiwidget: UIWidgets.EditBox, desc: "Pivot name for optic aim fallback (when not using commander TurretComponent)")]
	protected string m_sOpticPivotName;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Use commander TurretComponent aim as optic direction (recommended)")]
	protected bool m_bPreferCommanderTurretAim;

	[Attribute(defvalue: "-1", uiwidget: UIWidgets.EditBox, desc: "Hull lock after T: 0 = one-shot slew only (no lock). <0 = lock until cancel/align (no time limit). >0 = lock for this many ms.")]
	protected int m_iCommanderLockDurationMs;

	[Attribute(defvalue: "0.01", uiwidget: UIWidgets.EditBox, desc: "End lock early when main |yaw| and |pitch| error vs lock LOS both <= this (degrees).")]
	protected float m_fHkAlignReleaseDeg;

	[Attribute(defvalue: "20", uiwidget: UIWidgets.EditBox, desc: "Main turret HK slew rate (deg/s) for yaw and pitch toward lock LOS.")]
	protected float m_fHkMainSlewYawDegPerSec;

	[Attribute(defvalue: "0.05", uiwidget: UIWidgets.EditBox, desc: "Yaw-first: slew yaw until |err| <= this, then elevation only (pitch; yaw held at target).")]
	protected float m_fHkYawBeforePitchEpsilonDeg;

	[Attribute(defvalue: "20", uiwidget: UIWidgets.EditBox, desc: "Commander CITV lock: max yaw+pitch rate toward lock LOS (deg/s per axis). 0 = instant snap each frame.")]
	protected float m_fHkCommanderLockTrackDegPerSec;

	[Attribute(defvalue: "400", uiwidget: UIWidgets.EditBox, desc: "After main turret release (cancel/end), server re-snaps wanted+actual to release angles every frame for this many ms to kill residual slew. 0 = off.")]
	protected int m_iPostCancelMainFreezeMs;

	protected RplComponent m_RplComponent;

	protected bool m_bLockCommander;
	protected int m_iLockCmdUntilTick;
	//! Hull-relative lock: store optic dir in vehicle hull local space; false = fixed commander local snapshot (no hull).
	protected bool m_bLockHullRelative;
	protected vector m_vLockedDirHullLocal;
	//! Normalized world LOS at T; main turret must refresh SetAimingRotationWanted every frame (stale local target overshoots).
	protected vector m_vLockedWorldDirNorm;
	protected vector m_vLockedCmdRotDeg; //! Desired yaw/pitch (deg); updated each frame when hull lock.

	//! For deg/s main yaw slew: elapsed time between ApplySlewMainTurret calls (tick clock) when dt not passed.
	protected int m_iLastHkMainSlewApplyTick;

	//! True only when m_iCommanderLockDurationMs > 0 (timed lock). Else lock ends only on cancel/align.
	protected bool m_bLockCommanderUseTimer;

	//! GetActionTriggered stays true while key is held; slew only on rising edge (one Hunter-Killer per press).
	protected bool m_bPrevHkSlewHeld;

	//! Rising edge for Y cancel (not cleared by lock-end transient reset).
	protected bool m_bPrevHkCancelHeld;

	//! Previous frame: local player was in commander seat (gunner->commander seat-enter handling).
	protected bool m_bPrevFrameWasLocalCommander;

	//! Rising edge into commander: release deferred one frame so compartment/turret controller is ready.
	protected bool m_bPendingCommanderSeatRelease;

	//! Previous frame: local commander was alive and not unconscious (HK cancel edge).
	protected bool m_bPrevCommanderFitForHk;

	//! Server only: apply RpcRefreshMainSlewLock only while HK lock is active (avoids cancel then same-frame/next RPC re-slewing main).
	protected bool m_bServerHkMainRefreshActive;

	//! Server only: after ReleaseMainTurretScriptControl, hold fixed yaw/pitch snapshot to stop residual turret integration (see m_iPostCancelMainFreezeMs).
	protected bool m_bServerMainPostReleaseFreezeActive;
	protected int m_iServerMainPostReleaseFreezeUntilTick;
	protected vector m_vServerMainPostReleaseFreezeRotDeg;

	//------------------------------------------------------------------------------------------------
	protected static float DotVec(vector a, vector b)
	{
		return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	}

	//------------------------------------------------------------------------------------------------
	protected static vector GetNormalized(vector v)
	{
		float len = Math.Sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
		if (len < 0.0001)
			return Vector(0, 0, 1);
		return Vector(v[0] / len, v[1] / len, v[2] / len);
	}

	//------------------------------------------------------------------------------------------------
	protected static float RadToDeg(float rad)
	{
		return rad * 180.0 / Math.PI;
	}

	//------------------------------------------------------------------------------------------------
	//! Shortest signed delta from angle a to b (degrees), for rate/wrap.
	protected static float AngleDeltaDeg(float aDeg, float bDeg)
	{
		float d = bDeg - aDeg;
		while (d > 180.0)
			d -= 360.0;
		while (d < -180.0)
			d += 360.0;
		return d;
	}

	//! GetAimingRotation can return multi-turn yaw; normalize for stable lock + SetAimingRotationWanted.
	protected static float WrapYawDeg180(float deg)
	{
		float y = deg;
		while (y > 180.0)
			y -= 360.0;
		while (y < -180.0)
			y += 360.0;
		return y;
	}

	protected static float ClampPitchDeg(float deg)
	{
		if (deg > 89.0)
			return 89.0;
		if (deg < -89.0)
			return -89.0;
		return deg;
	}

	protected vector NormalizeLockRotationDeg(vector rotDeg)
	{
		return Vector(WrapYawDeg180(rotDeg[0]), ClampPitchDeg(rotDeg[1]), rotDeg[2]);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLocalPlayerCommanderInThisVehicle()
	{
		return CAV_HunterKillerCommanderUtil.CommanderGateFailReason() == 0;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLocalPlayerGunnerInThisVehicle()
	{
		return CAV_HunterKillerCommanderUtil.GunnerGateFailReason(GetOwner()) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Commander seat + conscious + alive: HK slew/lock allowed and lock/stabilization runs.
	protected bool IsLocalControlledCommanderFitForHunterKiller()
	{
		return CAV_HunterKillerCommanderUtil.IsLocalControlledCommanderFitForHunterKiller();
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetOpticDirectionWorld(IEntity commanderTurretEntity)
	{
		if (m_bPreferCommanderTurretAim)
		{
			TurretComponent cmdTurret = TurretComponent.Cast(commanderTurretEntity.FindComponent(TurretComponent));
			if (cmdTurret)
			{
				vector dir = cmdTurret.GetAimingDirectionWorld();
				return GetNormalized(dir);
			}
		}

		// Fallback: pivot forward in world space (pivot name from attribute)
		vector mat[4];
		commanderTurretEntity.GetWorldTransform(mat);
		// First column = local X basis in world space; used when TurretComponent is unavailable.
		vector col0 = mat[0];
		vector dirW = Vector(col0[0], col0[1], col0[2]);
		return GetNormalized(dirW);
	}

	//------------------------------------------------------------------------------------------------
	//! World aim direction -> **this** turret entity's local direction (basis = entity world transform axes).
	protected vector WorldDirToTurretLocalDir(IEntity turretEntity, vector dirWorldNorm)
	{
		vector mat[4];
		turretEntity.GetWorldTransform(mat);
		vector dirLocal;
		dirLocal[0] = DotVec(dirWorldNorm, mat[0]);
		dirLocal[1] = DotVec(dirWorldNorm, mat[1]);
		dirLocal[2] = DotVec(dirWorldNorm, mat[2]);
		return GetNormalized(dirLocal);
	}

	//------------------------------------------------------------------------------------------------
	//! Inverse of WorldDirToTurretLocalDir: entity-local direction -> world (rotation basis from GetWorldTransform).
	protected vector EntityLocalDirToWorldDir(IEntity ent, vector dirLocalNorm)
	{
		vector mat[4];
		ent.GetWorldTransform(mat);
		vector dirW;
		dirW[0] = mat[0][0] * dirLocalNorm[0] + mat[1][0] * dirLocalNorm[1] + mat[2][0] * dirLocalNorm[2];
		dirW[1] = mat[0][1] * dirLocalNorm[0] + mat[1][1] * dirLocalNorm[1] + mat[2][1] * dirLocalNorm[2];
		dirW[2] = mat[0][2] * dirLocalNorm[0] + mat[1][2] * dirLocalNorm[1] + mat[2][2] * dirLocalNorm[2];
		return GetNormalized(dirW);
	}

	//------------------------------------------------------------------------------------------------
	//! Vehicle hull entity: commander -> gunner turret -> hull (parent of main turret).
	protected IEntity GetVehicleHullEntity(IEntity commanderEnt)
	{
		if (!commanderEnt)
			return null;
		IEntity mainEnt = commanderEnt.GetParent();
		if (!mainEnt)
			return null;
		return mainEnt.GetParent();
	}

	//------------------------------------------------------------------------------------------------
	//! Yaw/pitch in degrees from local dir; pass * Math.DEG2RAD for SetAimingRotationWanted (API: radians).
	protected vector LocalDirToAimingRotationWantedDeg(vector dirLocalNorm)
	{
		float x = dirLocalNorm[0];
		float y = dirLocalNorm[1];
		float z = dirLocalNorm[2];
		float yawRad = Math.Atan2(x, z);
		float horiz = Math.Sqrt(x * x + z * z);
		float pitchRad = Math.Atan2(y, horiz);
		return Vector(RadToDeg(yawRad), RadToDeg(pitchRad), 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Commander lock target: world LOS -> commander turret local dir -> yaw/pitch deg (same basis as main slew math).
	protected vector HK_ComputeCommanderLockRotationDegFromWorldDir(IEntity commanderEnt, vector dirWorldNorm)
	{
		vector dirLocal = GetNormalized(WorldDirToTurretLocalDir(commanderEnt, GetNormalized(dirWorldNorm)));
		return NormalizeLockRotationDeg(LocalDirToAimingRotationWantedDeg(dirLocal));
	}

	//------------------------------------------------------------------------------------------------
	//! Main gun target yaw/pitch (deg) from world LOS in main turret local basis (shared by slew + align release).
	protected vector HK_MainTargetRotationDegFromWorldDir(IEntity mainTurretEnt, vector dirWorldNorm)
	{
		vector dirLocal = WorldDirToTurretLocalDir(mainTurretEnt, GetNormalized(dirWorldNorm));
		return NormalizeLockRotationDeg(LocalDirToAimingRotationWantedDeg(dirLocal));
	}

	//------------------------------------------------------------------------------------------------
	protected static float ClampFrameDtSec(float dtSec)
	{
		if (dtSec < 0.0001)
			dtSec = 0.0001;
		if (dtSec > 0.5)
			dtSec = 0.5;
		return dtSec;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetTurretAimingFromDeg(TurretComponent t, vector rotDeg)
	{
		if (!t)
			return;
		vector rotRad = Vector(rotDeg[0] * Math.DEG2RAD, rotDeg[1] * Math.DEG2RAD, rotDeg[2] * Math.DEG2RAD);
		t.SetAimingRotationWanted(rotRad);
		t.SetAimingRotation(rotRad);
	}

	//------------------------------------------------------------------------------------------------
	//! dtSecOverride >= 0: use frame/wall dt for deg/s (smooth). < 0: derive dt from tick delta (one-shot RPCs).
	protected void ApplySlewMainTurret(vector dirWorldNorm, float dtSecOverride)
	{
		IEntity commanderEnt = GetOwner();
		if (!commanderEnt)
			return;

		IEntity mainTurretEnt = commanderEnt.GetParent();
		if (!mainTurretEnt)
			return;

		TurretComponent mainTurret = TurretComponent.Cast(mainTurretEnt.FindComponent(TurretComponent));
		if (!mainTurret)
			return;

		vector targetRotDeg = HK_MainTargetRotationDegFromWorldDir(mainTurretEnt, dirWorldNorm);

		int nowTick = System.GetTickCount();
		float dtSec = 1.0 / 60.0;
		if (dtSecOverride >= 0)
			dtSec = ClampFrameDtSec(dtSecOverride);
		else if (m_iLastHkMainSlewApplyTick > 0)
		{
			int dtMs = nowTick - m_iLastHkMainSlewApplyTick;
			if (dtMs < 1)
				dtMs = 1;
			if (dtMs > 100)
				dtMs = 100;
			dtSec = dtMs / 1000.0;
		}
		m_iLastHkMainSlewApplyTick = nowTick;

		vector actualDeg = mainTurret.GetAimingRotation();
		float yawErr = AngleDeltaDeg(actualDeg[0], targetRotDeg[0]);
		float pitchErr = AngleDeltaDeg(actualDeg[1], targetRotDeg[1]);
		float rate = m_fHkMainSlewYawDegPerSec;
		if (rate < 0)
			rate = 0;
		float maxStepDeg = rate * dtSec;
		float yawEps = m_fHkYawBeforePitchEpsilonDeg;
		if (yawEps < 0.001)
			yawEps = 0.001;
		bool yawPhase = false;
		if (Math.AbsFloat(yawErr) > yawEps)
			yawPhase = true;
		vector newRotDeg;
		if (yawPhase)
		{
			float stepY = yawErr;
			if (stepY > maxStepDeg)
				stepY = maxStepDeg;
			if (stepY < -maxStepDeg)
				stepY = -maxStepDeg;
			newRotDeg = NormalizeLockRotationDeg(Vector(actualDeg[0] + stepY, actualDeg[1], 0));
		}
		else
		{
			float stepP = pitchErr;
			if (stepP > maxStepDeg)
				stepP = maxStepDeg;
			if (stepP < -maxStepDeg)
				stepP = -maxStepDeg;
			newRotDeg = NormalizeLockRotationDeg(Vector(targetRotDeg[0], actualDeg[1] + stepP, 0));
		}
		SetTurretAimingFromDeg(mainTurret, newRotDeg);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SlewToDirection(vector dirWorldNorm)
	{
		m_bServerMainPostReleaseFreezeActive = false;
		dirWorldNorm = GetNormalized(dirWorldNorm);
		ApplySlewMainTurret(dirWorldNorm, -1);
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	protected void ApplyServerHkMainRefreshFlag(bool active)
	{
		m_bServerHkMainRefreshActive = active;
		if (active)
			m_bServerMainPostReleaseFreezeActive = false;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcSetServerHkMainRefreshActive(bool active)
	{
		ApplyServerHkMainRefreshFlag(active);
	}

	//------------------------------------------------------------------------------------------------
	//! Server authority: must match commander lock begin/end or cancel overwrites RpcRefresh spam.
	//! Do not require Rpl owner: gunner cancel must reach server even when commander turret is owned by another peer.
	protected void NotifyServerHkMainRefreshActive(bool active)
	{
		if (Replication.IsServer())
		{
			ApplyServerHkMainRefreshFlag(active);
			return;
		}
		if (!m_RplComponent)
			return;
		Rpc(RpcSetServerHkMainRefreshActive, active);
	}

	//------------------------------------------------------------------------------------------------
	//! Per-frame lock refresh on server (dt = commander frame timeSlice for consistent deg/s, less stutter than tick-only).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcRefreshMainSlewLock(vector dirWorldNorm, float dtSec)
	{
		if (m_bServerMainPostReleaseFreezeActive && System.GetTickCount() < m_iServerMainPostReleaseFreezeUntilTick)
			return;
		if (!m_bServerHkMainRefreshActive)
			return;
		dirWorldNorm = GetNormalized(dirWorldNorm);
		dtSec = ClampFrameDtSec(dtSec);
		ApplySlewMainTurret(dirWorldNorm, dtSec);
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestSlew(vector dirWorldNorm)
	{
		dirWorldNorm = GetNormalized(dirWorldNorm);

		if (Replication.IsServer())
		{
			m_bServerMainPostReleaseFreezeActive = false;
			ApplySlewMainTurret(dirWorldNorm, -1);
			return;
		}

		//! Dedicated-server commander is often not Rpl owner of the CITV entity; Rpc still routes to server (same as RequestReleaseMainTurret / RpcAsk_ReleaseMainTurret).
		if (!m_RplComponent)
			return;

		Rpc(RpcAsk_SlewToDirection, dirWorldNorm);
	}

	//------------------------------------------------------------------------------------------------
	//! Clears lock snapshot fields only (caller releases turrets if needed).
	protected void ClearHunterKillerLockFields()
	{
		m_bLockCommander = false;
		m_bLockCommanderUseTimer = false;
		m_iLockCmdUntilTick = 0;
		m_bLockHullRelative = false;
		m_vLockedWorldDirNorm = Vector(0, 0, 1);
		m_vLockedDirHullLocal = Vector(0, 0, 1);
		m_vLockedCmdRotDeg = Vector(0, 0, 0);
		NotifyServerHkMainRefreshActive(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Cooldown, slew edge; resets stale state between slew/lock cycles.
	protected void ResetHunterKillerLocalTransientState()
	{
		m_bPrevHkSlewHeld = false;
		m_iLastHkMainSlewApplyTick = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearHunterKillerLockAndResetLocal()
	{
		ClearHunterKillerLockFields();
		ResetHunterKillerLocalTransientState();
	}

	//------------------------------------------------------------------------------------------------
	//! Mirror ReleaseCommanderTurretScriptControl on the gunner (main) turret so Wanted matches Actual.
	protected void ReleaseMainTurretScriptControl(IEntity mainTurretEnt, string reason)
	{
		if (!mainTurretEnt)
			return;
		TurretComponent mainT = TurretComponent.Cast(mainTurretEnt.FindComponent(TurretComponent));
		if (!mainT)
			return;
		vector actDeg = mainT.GetAimingRotation();
		SetTurretAimingFromDeg(mainT, actDeg);
		if (Replication.IsServer() && m_iPostCancelMainFreezeMs > 0)
		{
			m_vServerMainPostReleaseFreezeRotDeg = NormalizeLockRotationDeg(actDeg);
			m_iServerMainPostReleaseFreezeUntilTick = System.GetTickCount() + m_iPostCancelMainFreezeMs;
			m_bServerMainPostReleaseFreezeActive = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: each frame snap main to fixed release snapshot so sim cannot keep integrating residual slew (log: wanted tracked actual but yaw rate stayed high).
	protected void ApplyServerMainPostReleaseFreezeIfActive()
	{
		if (!Replication.IsServer())
			return;
		if (!m_bServerMainPostReleaseFreezeActive)
			return;
		int now = System.GetTickCount();
		if (now >= m_iServerMainPostReleaseFreezeUntilTick)
		{
			m_bServerMainPostReleaseFreezeActive = false;
			return;
		}
		IEntity cmdEnt = GetOwner();
		if (!cmdEnt)
			return;
		IEntity mainEnt = cmdEnt.GetParent();
		if (!mainEnt)
			return;
		TurretComponent mainT = TurretComponent.Cast(mainEnt.FindComponent(TurretComponent));
		if (!mainT)
			return;
		SetTurretAimingFromDeg(mainT, m_vServerMainPostReleaseFreezeRotDeg);
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerNotifyRefreshOffAndReleaseMainTurret(string reason)
	{
		NotifyServerHkMainRefreshActive(false);
		IEntity cmdEnt = GetOwner();
		if (cmdEnt)
			ReleaseMainTurretScriptControl(cmdEnt.GetParent(), reason);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_ReleaseMainTurret()
	{
		ServerNotifyRefreshOffAndReleaseMainTurret("rpc_release_main");
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestReleaseMainTurret()
	{
		if (Replication.IsServer())
		{
			ServerNotifyRefreshOffAndReleaseMainTurret("server_direct_release_main");
			return;
		}

		if (!m_RplComponent)
			return;

		Rpc(RpcAsk_ReleaseMainTurret);
	}

	//------------------------------------------------------------------------------------------------
	//! Commander Y: stop HK slew/lock and return both turrets to player control (local commander + server main).
	protected void CancelHunterKillerAll(string reason)
	{
		IEntity cmdEnt = GetOwner();
		if (cmdEnt)
		{
			TurretComponent cmdT = TurretComponent.Cast(cmdEnt.FindComponent(TurretComponent));
			ReleaseCommanderTurretScriptControl(cmdT, "release_cancel_" + reason);
		}
		RequestReleaseMainTurret();
		ClearHunterKillerLockAndResetLocal();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		SetEventMask(owner, EntityEvent.FRAME);
		m_bPrevFrameWasLocalCommander = false;
		m_bPendingCommanderSeatRelease = false;
		m_bPrevHkCancelHeld = false;
		m_bServerHkMainRefreshActive = false;
		m_bServerMainPostReleaseFreezeActive = false;
		m_iLastHkMainSlewApplyTick = 0;
		m_bLockCommanderUseTimer = false;
		m_bPrevCommanderFitForHk = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Entity removed (e.g. vehicle destroyed): stop server slew/refresh/freeze and clear lock like Y cancel.
	override protected void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			ApplyServerHkMainRefreshFlag(false);
			m_bServerMainPostReleaseFreezeActive = false;
		}
		CancelHunterKillerAll("vehicle_destroyed");
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Drop hull lock when local player is not in commander seat (gunner/dismount): timer expiry never ran, so state would stick.
	protected void ClearHunterKillerLockIfNotCommander(string reason)
	{
		if (!m_bLockCommander)
			return;
		IEntity cmdEnt = GetOwner();
		if (cmdEnt)
		{
			TurretComponent cmdT = TurretComponent.Cast(cmdEnt.FindComponent(TurretComponent));
			ReleaseCommanderTurretScriptControl(cmdT, "release_" + reason);
		}
		ClearHunterKillerLockAndResetLocal();
	}

	//------------------------------------------------------------------------------------------------
	//! Step commander CITV toward lock target (deg/s per axis). rate<=0: snap wanted+actual to target (instant hold).
	protected void ApplyCommanderLockTrackTowardTarget(TurretComponent cmdT, vector targetRotDeg, float dtSec)
	{
		if (!cmdT)
			return;
		dtSec = ClampFrameDtSec(dtSec);
		float rate = m_fHkCommanderLockTrackDegPerSec;
		if (rate <= 0)
		{
			SetTurretAimingFromDeg(cmdT, targetRotDeg);
			return;
		}
		vector actualDeg = cmdT.GetAimingRotation();
		float yawErr = AngleDeltaDeg(actualDeg[0], targetRotDeg[0]);
		float pitchErr = AngleDeltaDeg(actualDeg[1], targetRotDeg[1]);
		float maxStep = rate * dtSec;
		float stepY = yawErr;
		if (stepY > maxStep)
			stepY = maxStep;
		if (stepY < -maxStep)
			stepY = -maxStep;
		float stepP = pitchErr;
		if (stepP > maxStep)
			stepP = maxStep;
		if (stepP < -maxStep)
			stepP = -maxStep;
		vector newDeg = NormalizeLockRotationDeg(Vector(actualDeg[0] + stepY, actualDeg[1] + stepP, 0));
		SetTurretAimingFromDeg(cmdT, newDeg);
	}

	//------------------------------------------------------------------------------------------------
	//! After hull lock, wanted+actual were overwritten every frame; snap both so input regains both channels.
	protected void ReleaseCommanderTurretScriptControl(TurretComponent cmdT, string reason)
	{
		if (!cmdT)
			return;
		SetTurretAimingFromDeg(cmdT, cmdT.GetAimingRotation());
	}

	//------------------------------------------------------------------------------------------------
	//! At T: store optic direction in vehicle hull local space when hierarchy allows; else fixed commander local snapshot.
	protected void BeginCommanderLocalLock(IEntity commanderEnt, vector dirWorldNorm)
	{
		if (m_iCommanderLockDurationMs == 0)
			return;
		if (!commanderEnt)
			return;
		dirWorldNorm = GetNormalized(dirWorldNorm);
		IEntity hullEnt = GetVehicleHullEntity(commanderEnt);
		if (hullEnt)
		{
			m_vLockedDirHullLocal = GetNormalized(WorldDirToTurretLocalDir(hullEnt, dirWorldNorm));
			m_bLockHullRelative = true;
		}
		else
		{
			m_bLockHullRelative = false;
			TurretComponent cmdT = TurretComponent.Cast(commanderEnt.FindComponent(TurretComponent));
			if (!cmdT)
				return;
			m_vLockedCmdRotDeg = NormalizeLockRotationDeg(cmdT.GetAimingRotation());
		}
		m_vLockedWorldDirNorm = dirWorldNorm;
		m_bLockCommander = true;
		m_bLockCommanderUseTimer = (m_iCommanderLockDurationMs > 0);
		if (m_bLockCommanderUseTimer)
			m_iLockCmdUntilTick = System.GetTickCount() + m_iCommanderLockDurationMs;
		else
			m_iLockCmdUntilTick = 0;
		m_iLastHkMainSlewApplyTick = 0;
		NotifyServerHkMainRefreshActive(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Hull-relative lock: recompute world LOS from hull-local snapshot every frame so main slew + commander aim stay aligned while hull or main turret moves.
	protected void RefreshLockedWorldDirFromHullIfHullLock(IEntity commanderEnt)
	{
		if (!m_bLockCommander || !m_bLockHullRelative)
			return;
		if (!commanderEnt)
			return;
		IEntity hullEnt = GetVehicleHullEntity(commanderEnt);
		if (!hullEnt)
			return;
		m_vLockedWorldDirNorm = GetNormalized(EntityLocalDirToWorldDir(hullEnt, m_vLockedDirHullLocal));
	}

	//------------------------------------------------------------------------------------------------
	//! Re-apply main slew toward fixed world LOS every frame while lock active (one-shot Wanted goes stale as main rotates).
	//! Remote gunner owns local turret wanted unless we apply on server (same as RequestSlew / RpcAsk_SlewToDirection).
	protected void ApplyMainTurretSlewFromLockIfActive(float frameDtSec)
	{
		if (!m_bLockCommander)
			return;
		if (Replication.IsServer() && m_bServerMainPostReleaseFreezeActive && System.GetTickCount() < m_iServerMainPostReleaseFreezeUntilTick)
			return;
		int now = System.GetTickCount();
		if (m_bLockCommanderUseTimer && now >= m_iLockCmdUntilTick)
			return;
		if (!IsLocalControlledCommanderFitForHunterKiller())
			return;

		IEntity cmdForMain = GetOwner();
		if (!cmdForMain || !cmdForMain.GetParent())
			return;

		vector dirWorldNorm = m_vLockedWorldDirNorm;
		if (Replication.IsServer())
		{
			if (!m_bServerHkMainRefreshActive)
				return;
			ApplySlewMainTurret(dirWorldNorm, frameDtSec);
			return;
		}

		if (!m_RplComponent)
			return;

		Rpc(RpcRefreshMainSlewLock, dirWorldNorm, frameDtSec);
	}

	//------------------------------------------------------------------------------------------------
	//! Hull lock: each frame recompute commander target from m_vLockedWorldDirNorm via HK_ComputeCommanderLockRotationDegFromWorldDir, then track. Else fixed local snapshot at lock T.
	protected void ApplyCommanderLocalStabilizationIfActive(float dtSec)
	{
		if (!m_bLockCommander)
			return;
		if (!IsLocalControlledCommanderFitForHunterKiller())
			return;
		int now = System.GetTickCount();
		if (m_bLockCommanderUseTimer && now >= m_iLockCmdUntilTick)
		{
			IEntity cmdEntExpire = GetOwner();
			if (cmdEntExpire)
			{
				TurretComponent cmdTExpire = TurretComponent.Cast(cmdEntExpire.FindComponent(TurretComponent));
				ReleaseCommanderTurretScriptControl(cmdTExpire, "release_timer_expiry");
			}
			ClearHunterKillerLockAndResetLocal();
			return;
		}
		IEntity cmdEnt = GetOwner();
		if (!cmdEnt)
			return;
		TurretComponent cmdT = TurretComponent.Cast(cmdEnt.FindComponent(TurretComponent));
		if (!cmdT)
			return;
		if (m_bLockHullRelative)
		{
			IEntity hullEnt = GetVehicleHullEntity(cmdEnt);
			if (!hullEnt)
			{
				ReleaseCommanderTurretScriptControl(cmdT, "release_no_hull_ent");
				ClearHunterKillerLockAndResetLocal();
				return;
			}
			//! Same-frame m_vLockedWorldDirNorm as main slew (refreshed in EOnFrame before ApplyMain/this).
			vector rotDeg = HK_ComputeCommanderLockRotationDegFromWorldDir(cmdEnt, m_vLockedWorldDirNorm);
			m_vLockedCmdRotDeg = rotDeg;
			ApplyCommanderLockTrackTowardTarget(cmdT, rotDeg, dtSec);
		}
		else
		{
			ApplyCommanderLockTrackTowardTarget(cmdT, m_vLockedCmdRotDeg, dtSec);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! When gunner main yaw+pitch match lock LOS within m_fHkAlignReleaseDeg, release lock.
	protected void TryReleaseHunterKillerLockIfGunnerAligned(IEntity commanderEnt)
	{
		if (!m_bLockCommander)
			return;
		if (!IsLocalControlledCommanderFitForHunterKiller())
			return;
		if (!commanderEnt)
			return;
		IEntity mainEnt = commanderEnt.GetParent();
		if (!mainEnt)
			return;
		TurretComponent mainT = TurretComponent.Cast(mainEnt.FindComponent(TurretComponent));
		if (!mainT)
			return;
		vector targetRot = HK_MainTargetRotationDegFromWorldDir(mainEnt, m_vLockedWorldDirNorm);
		vector mainAct = mainT.GetAimingRotation();
		float yawErr = Math.AbsFloat(AngleDeltaDeg(mainAct[0], targetRot[0]));
		float pitchErr = Math.AbsFloat(AngleDeltaDeg(mainAct[1], targetRot[1]));
		if (yawErr <= m_fHkAlignReleaseDeg && pitchErr <= m_fHkAlignReleaseDeg)
		{
			TurretComponent cmdTAlign = TurretComponent.Cast(commanderEnt.FindComponent(TurretComponent));
			ReleaseCommanderTurretScriptControl(cmdTAlign, "release_align_gunner");
			ClearHunterKillerLockAndResetLocal();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		bool cmdLocalNow = IsLocalPlayerCommanderInThisVehicle();
		bool cmdFitHk = IsLocalControlledCommanderFitForHunterKiller();

		if (cmdLocalNow && !cmdFitHk && m_bPrevCommanderFitForHk)
			CancelHunterKillerAll("commander_unconscious_or_dead");

		if (cmdLocalNow)
			m_bPrevCommanderFitForHk = cmdFitHk;
		else
			m_bPrevCommanderFitForHk = true;

		if (!cmdLocalNow)
		{
			ClearHunterKillerLockIfNotCommander("lock_clear_not_commander");
			m_bPendingCommanderSeatRelease = false;
		}
		else
		{
			if (m_bPendingCommanderSeatRelease)
			{
				IEntity ceEnter = GetOwner();
				if (ceEnter)
				{
					TurretComponent cmdTEnter = TurretComponent.Cast(ceEnter.FindComponent(TurretComponent));
					ReleaseCommanderTurretScriptControl(cmdTEnter, "release_commander_seat_enter_deferred");
				}
				m_bPendingCommanderSeatRelease = false;
			}
			else if (!m_bPrevFrameWasLocalCommander)
			{
				//! Next frame: compartment controller + turret sim ready; avoids release with stale reads on seat-enter frame.
				m_bPendingCommanderSeatRelease = true;
			}
		}
		m_bPrevFrameWasLocalCommander = cmdLocalNow;

		if (GetGame() && GetGame().GetInputManager())
		{
			InputManager im = GetGame().GetInputManager();
			bool cancelHeld = im.GetActionTriggered("CAV_HunterKillerCancel");
			bool cancelRising = cancelHeld && !m_bPrevHkCancelHeld;
			m_bPrevHkCancelHeld = cancelHeld;
			if (cancelRising)
			{
				if (cmdFitHk)
					CancelHunterKillerAll("y_key");
				else if (IsLocalPlayerGunnerInThisVehicle())
					CancelHunterKillerAll("y_key_gunner");
			}

			bool hkHeld = im.GetActionTriggered("CAV_HunterKillerSlew");
			bool hkRisingEdge = hkHeld && !m_bPrevHkSlewHeld;
			m_bPrevHkSlewHeld = hkHeld;

			if (hkRisingEdge)
			{
				if (cmdFitHk)
				{
					vector dirW = GetOpticDirectionWorld(owner);
					if (m_iCommanderLockDurationMs != 0)
					{
						BeginCommanderLocalLock(owner, dirW);
					}
					else
					{
						RequestSlew(dirW);
					}
				}
			}
		}

		ApplyServerMainPostReleaseFreezeIfActive();

		if (m_bLockCommander && m_bLockHullRelative)
		{
			IEntity ceRefresh = GetOwner();
			if (ceRefresh)
				RefreshLockedWorldDirFromHullIfHullLock(ceRefresh);
		}

		float frameDt = ClampFrameDtSec(timeSlice);
		ApplyMainTurretSlewFromLockIfActive(frameDt);
		ApplyCommanderLocalStabilizationIfActive(frameDt);
		TryReleaseHunterKillerLockIfGunnerAligned(owner);
	}
}
