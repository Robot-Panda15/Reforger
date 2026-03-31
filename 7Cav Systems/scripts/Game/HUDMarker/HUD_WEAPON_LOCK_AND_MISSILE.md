# HUD weapon lock vs Hellfire / AGM114 bridge

Two different concepts share the word ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œlockÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¿Ãƒâ€šÃ‚Â½:

## 1. `HMD_LaserLockState` (client only)

- **What:** LSHIFT cycle, locked designation highlight, rangefinder lock readout.
- **Where:** Static client state; may reference `HUDMarkerSystem` pooled ids, WCS designators, or a world snapshot.
- **Does not:** Automatically replicate to the server or to `HUDLaserMarkingComponent` weapon-lock fields.

## 2. Server weapon lock for missiles (`m_bWeaponLaserLockActive`, `m_vWeaponLaserLockWorld`)

- **What:** The world point `HMD_WcsLaserVehicleDesignatorBridge.TryGetHmdLaserTargetWorld` reads **on the server** for AGM114 / WCS resolve.
- **How:** `SCR_HUDManagerComponent.OnUpdate` calls `HUDLaserMarkingComponent.ClientSyncLockedWorldFromHud` every frame. When `HMD_LaserLockState.IsLocked()` and `TryGetLockedTargetWorldPosition` succeeds, the client sends `RpcAsk_SetWeaponLaserLockState(active, worldPos)`. The server mirrors that onto **every** `HUDLaserMarkingComponent` under the vehicle root.

## Self-lase (own marking laser)

If WCS cone resolve finds no designator (weapon-station aim often does not include your own ground spot), `TryGetHmdLaserTargetWorld` falls back to the first `HUDLaserMarkingComponent` on the vehicle with valid **replicated** laser hit (`phase` `self_marking` in `H10` `out_world`). The AGM114 seeker then tracks that point without an extra boresight FOV check.

## Replication gate (listen server + Workbench)

`ClientSyncLockedWorldFromHud` must **not** use `Replication.IsServer()` alone as a skip condition. Listen-server hosts are **both** server and client; they still need to run this path so the RPC reaches the server simulation where the missile runs.

Correct pattern:

- Skip **only** when replication says dedicated **and** there is **no** local controlled entity (`GetLocalControlledEntity` null). Some listen / Workbench sessions report `!IsClient()` even with a local pilot; those **do not** skip so HUD lock can still RPC.
- If `!Replication.IsRunning()` (common in **Workbench** / local play), **RPC does not run**, but `ClientPushLockState` still calls `HMD_ApplyWeaponLaserLockStateToVehicleHierarchy` directly so the same-process missile seeker sees `m_bWeaponLaserLockActive` / `m_vWeaponLaserLockWorld`. Logged once as `H12` `no_replication_local_mode`.

## Debug log tags (session `89ea13`, `$profile:debug-89ea13.log`)

| Tag | Meaning |
|-----|---------|
| `H12` `gate_passed_client_sync` | Client sync is allowed (listen or remote client). Once per session. |
| `H12` `dedicated_server_skip` | `IsServer && !IsClient` and **no** `GetLocalControlledEntity`; headless. Once per session. |
| `H12` `host_override_isclient_false` | Same replication flags but **local pilot exists** â€” ClientSync continues (listen / Workbench). Once per session. |
| `H12` `no_replication_local_mode` | `Replication.IsRunning()` false; lock uses **local Apply** not RPC. Once per session. |
| `H12` `local_lock_apply` | Direct hierarchy apply with `w` (no replication). |
| `H12` `push_lock_world` | Exact world vector sent with lock push (throttled with other H12 verbose lines). |
| `H2` `lock_rpc` | Server received `RpcAsk_SetWeaponLaserLockState` (includes `w` xyz). |
| `H10` `marker_row` | Server snapshot of each `HUDLaserMarkingComponent` (`lockAct`, `lockW`, ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¦). |

If the HUD shows lock but `H10` shows `lockAct:0` and `lockW:0,0,0`, the client pipeline above did not populate server stateÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¯Ã‚Â¿Ã‚Â½check `H12`/`H7`/`H8` and whether you are on a listen host (should see `gate_passed` and `H2` with non-zero `w` when locking).
