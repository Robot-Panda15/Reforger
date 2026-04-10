//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Static spot: HUDMarkerSystem dot + optional WCS designator for lock. Visual kind 0 (IFF circle) = HUD only, no WCS lase spot.")]
class HMD_PlacedDesignationComponentClass : WCS_Armament_HandheldLaserDesignatorComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Extends WCS handheld designator when visual kind is not IFF (0): ALL_DESIGNATORS / weapon lock see a lasing spot at this origin.
//! Visual kind 0 registers only in HUDMarkerSystem (IFF circle); WCS designating flags stay false.
//! Same entity + HMD_IffBeaconComponent: HUD row and (non-IFF kinds) WCS spot follow ShouldShowIffOnHud(); attachable uses dynamic label from beacon.
class HMD_PlacedDesignationComponent : WCS_Armament_HandheldLaserDesignatorComponent
{
	[Attribute("1688", UIWidgets.EditBox, "Label under dot (laser code style).", category: "HUD")]
	protected string m_sLabel;

	[Attribute("1 0 0 1", UIWidgets.ColorPicker, "Dot color (RGBA 0-1)", category: "HUD")]
	protected ref Color m_MarkerColor;

	[Attribute("1 1 1 1", UIWidgets.ColorPicker, "Label color (RGBA 0-1)", category: "HUD")]
	protected ref Color m_LabelColor;

	[Attribute("-1", UIWidgets.Slider, "Max visibility distance (m). -1 = no limit.", "-1 10000 100", category: "HUD")]
	protected float m_fVisibilityDistance;

	[Attribute("-1", UIWidgets.Slider, "Visual: -1 default (own lase); 0 IFF circle; 1 own; 2 foreign Lase square + label.", "-1 2 1", category: "HUD")]
	protected int m_iVisualKind;

	[Attribute("-1", UIWidgets.Slider, "Lifetime (s). -1 or 0 = no auto-delete. >0 = delete parent entity after that many seconds (server / offline authority).", "-1 3600 1", category: "HUD")]
	protected float m_fLifetimeSeconds;

	//! Server time at first lifetime tick (GetServerTimestamp).
	protected WorldTimestamp m_ServerTimeStart;
	protected bool m_bServerTimeStartSet;

	protected int m_iDesignationId = -1;

	protected int m_iLastRegisteredAttrKind = -999;

	//! Last label string passed to HUDMarkerSystem (UpdateDesignationName when prefab/runtime label changes).
	protected string m_sLastRegisteredHudLabel = "";

	//! Editor-child HUD proxy: HMD_IffBeaconComponent drives HUD on/off (no SetEnabled on script components in Enforce).
	protected bool m_bIffBeaconParentHudSuppressed;

	//------------------------------------------------------------------------------------------------
	protected Color HMD_GetMarkerColor()
	{
		if (m_MarkerColor)
			return m_MarkerColor;
		return Color.FromRGBA(255, 0, 0, 255);
	}

	//------------------------------------------------------------------------------------------------
	protected Color HMD_GetLabelColor()
	{
		if (m_LabelColor)
			return m_LabelColor;
		return Color.FromRGBA(255, 255, 255, 255);
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	protected float HMD_GetLifetimeSeconds(IEntity owner)
	{
		float outVal = m_fLifetimeSeconds;
		if (owner)
		{
			BaseContainer src = GetComponentSource(owner);
			if (src)
			{
				float fromSrc;
				if (src.Get("m_fLifetimeSeconds", fromSrc))
					outVal = fromSrc;
			}
		}
		return outVal;
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	protected int HMD_ResolveVisualKindAttr(IEntity owner)
	{
		int attrKind = m_iVisualKind;
		if (owner)
		{
			BaseContainer src = GetComponentSource(owner);
			if (src)
			{
				int fromPrefab;
				if (src.Get("m_iVisualKind", fromPrefab))
				{
					if (m_iVisualKind == -1)
						attrKind = fromPrefab;
					else
						attrKind = m_iVisualKind;
				}
			}
		}
		if (attrKind < -1 || attrKind > 2)
			attrKind = -1;
		return attrKind;
	}

	//------------------------------------------------------------------------------------------------
	//! Prefer serialized prefab label when BaseContainer has m_sLabel (runtime member can lag behind prefab).
	protected string HMD_ResolveLabelForHud(IEntity owner)
	{
		string label = m_sLabel;
		if (owner)
		{
			BaseContainer src = GetComponentSource(owner);
			if (src)
			{
				string fromPrefab;
				if (src.Get("m_sLabel", fromPrefab) && fromPrefab && fromPrefab.Length() > 0)
					label = fromPrefab;
			}
		}
		if (!label || label.Length() == 0)
			label = "1688";
		return label;
	}

	//------------------------------------------------------------------------------------------------
	//! Not a handheld gadget: skip WCS trace / character-bound Update (parent would clear designation).
	override void Update(float timeSlice)
	{
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (owner)
		{
			m_vDesignatedLocation = owner.GetOrigin();
			if (HMD_ShouldExposeWcsSpot(owner))
			{
				m_bIsDesignating = true;
				m_bHasValidDesignation = true;
			}
			else
			{
				m_bIsDesignating = false;
				m_bHasValidDesignation = false;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (owner)
			SetEventMask(owner, EntityEvent.FRAME | owner.GetEventMask());
	}

	//------------------------------------------------------------------------------------------------
	//! Proxy designation entities (child of attachable beacon) may need several parent hops; warheads match on self first.
	static HMD_IffBeaconComponent HMD_ResolveDrivingIffBeaconForDesignationHud(IEntity owner, out int outHopDepth)
	{
		outHopDepth = -1;
		if (!owner)
			return null;
		IEntity n = owner;
		for (int d = 0; d < 24 && n; d++)
		{
			HMD_IffBeaconComponent b = HMD_IffBeaconComponent.FindOnEntity(n);
			if (b)
			{
				outHopDepth = d;
				return b;
			}
			n = n.GetParent();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Clears HUD row immediately (used when parent beacon suppresses this child proxy).
	void HMD_ForceHudUnregisterBeforeDisable()
	{
		if (!GetGame() || !GetGame().InPlayMode())
			return;
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		HMD_UnregisterDesignationHudSlot(sys);
	}

	//------------------------------------------------------------------------------------------------
	//! Parent IFF beacon: suppress HUD registration while OFF / unplaced / no battery (replaces engine SetEnabled, which is unavailable on script components).
	void HMD_SetIffBeaconParentHudSuppressed(bool suppressed)
	{
		if (m_bIffBeaconParentHudSuppressed == suppressed)
			return;
		m_bIffBeaconParentHudSuppressed = suppressed;
		if (suppressed)
			HMD_ForceHudUnregisterBeforeDisable();
		else
			HMD_InvalidateCachedHudLabel();
	}

	//------------------------------------------------------------------------------------------------
	//! Forces next EOnFrame to push UpdateDesignationName (e.g. after beacon text/number Rpl or authority change).
	void HMD_InvalidateCachedHudLabel()
	{
		m_sLastRegisteredHudLabel = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Visual kind 0 (IFF HUD dot) never exposes WCS. Co-located HMD_IffBeaconComponent gates HUD + WCS for other kinds.
	protected bool HMD_ShouldExposeWcsSpot(IEntity owner)
	{
		if (HMD_ResolveVisualKindAttr(owner) == HMD_MarkerVisuals.KIND_IFF_MARKER)
			return false;
		HMD_IffBeaconComponent iff = HMD_IffBeaconComponent.FindOnEntity(owner);
		if (iff)
			return iff.ShouldShowIffOnHud();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HMD_ShouldShowHudDesignation(IEntity owner)
	{
		int hopIgnored;
		HMD_IffBeaconComponent drive = HMD_ResolveDrivingIffBeaconForDesignationHud(owner, hopIgnored);
		if (drive)
			return drive.ShouldShowIffOnHud();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected string HMD_GetDesignationHudLabel(IEntity owner)
	{
		int hopIgnored;
		HMD_IffBeaconComponent drive = HMD_ResolveDrivingIffBeaconForDesignationHud(owner, hopIgnored);
		if (!drive)
			return HMD_ResolveLabelForHud(owner);
		if (HMD_IffBeaconComponentAttachable.Cast(drive))
			return drive.GetMarkerLabelForHud();
		if (drive.GetOwner() && owner != drive.GetOwner())
			return drive.GetMarkerLabelForHud();
		return HMD_ResolveLabelForHud(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_SyncWcsDesignationFromOwner(IEntity owner)
	{
		if (!owner)
			return;
		if (HMD_ShouldExposeWcsSpot(owner))
		{
			m_vDesignatedLocation = owner.GetOrigin();
			m_bIsDesignating = true;
			m_bHasValidDesignation = true;
		}
		else
		{
			m_bIsDesignating = false;
			m_bHasValidDesignation = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void HMD_UnregisterDesignationHudSlot(HUDMarkerSystem sys)
	{
		if (m_iDesignationId < 0)
			return;
		if (sys && GetGame() && GetGame().InPlayMode())
		{
			vector pos;
			if (sys.TryGetDesignationWorldPositionById(m_iDesignationId, pos))
			{
				string nm = "";
				sys.TryGetDesignationNameById(m_iDesignationId, nm);
				HMD_LaserLockState.MigrateHudLockBeforeUnregisterLocalDesignation(m_iDesignationId, pos, nm, 0);
			}
			sys.UnregisterDesignation(m_iDesignationId);
		}
		m_iDesignationId = -1;
		m_iLastRegisteredAttrKind = -999;
		m_sLastRegisteredHudLabel = "";
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		if (!GetGame().InPlayMode())
			return;
		if (!owner)
			return;

		int hopIff;
		HMD_IffBeaconComponent drvIff = HMD_ResolveDrivingIffBeaconForDesignationHud(owner, hopIff);
		if (drvIff && hopIff > 0)
			HMD_SetIffBeaconParentHudSuppressed(!drvIff.ShouldShowIffOnHud());
		else
			HMD_SetIffBeaconParentHudSuppressed(false);

		float lifetimeSec = HMD_GetLifetimeSeconds(owner);
		if (lifetimeSec > 0 && HMD_MarkerLifetimeAuthority.ShouldRunTimedEntityDeleteAuthority(owner))
		{
			ChimeraWorld wLife = GetGame().GetWorld();
			if (wLife)
			{
				WorldTimestamp now = wLife.GetServerTimestamp();
				if (!m_bServerTimeStartSet)
				{
					m_ServerTimeStart = now;
					m_bServerTimeStartSet = true;
				}
				float elapsed = HMD_MarkerLifetimeAuthority.GetElapsedSecondsSinceServerTime(m_ServerTimeStart, wLife);
				if (elapsed >= lifetimeSec)
				{
					SCR_EntityHelper.DeleteEntityAndChildren(owner);
					return;
				}
			}
		}

		if (m_bIffBeaconParentHudSuppressed)
		{
			m_bIsDesignating = false;
			m_bHasValidDesignation = false;
			ChimeraWorld wSup = GetGame().GetWorld();
			if (wSup)
			{
				HUDMarkerSystem sysSup = HUDMarkerSystem.GetInstance(wSup);
				if (sysSup)
					HMD_UnregisterDesignationHudSlot(sysSup);
			}
			return;
		}

		HMD_SyncWcsDesignationFromOwner(owner);
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return;
		if (!HMD_ShouldShowHudDesignation(owner))
		{
			HMD_UnregisterDesignationHudSlot(sys);
			return;
		}
		vector pos = owner.GetOrigin();
		int attrKind = HMD_ResolveVisualKindAttr(owner);
		string hudLabel = HMD_GetDesignationHudLabel(owner);

		if (m_iDesignationId < 0)
		{
			int m = HMD_GetMarkerColor().PackToInt();
			int lbl = HMD_GetLabelColor().PackToInt();
			m_iDesignationId = sys.RegisterDesignation(pos, hudLabel, m, lbl, m_fVisibilityDistance, attrKind);
			m_iLastRegisteredAttrKind = attrKind;
			m_sLastRegisteredHudLabel = hudLabel;
		}
		else
		{
			sys.UpdateDesignation(m_iDesignationId, pos);
			if (attrKind != m_iLastRegisteredAttrKind)
			{
				sys.UpdateDesignationVisualKind(m_iDesignationId, attrKind);
				m_iLastRegisteredAttrKind = attrKind;
			}
			if (hudLabel != m_sLastRegisteredHudLabel)
			{
				sys.UpdateDesignationName(m_iDesignationId, hudLabel);
				m_sLastRegisteredHudLabel = hudLabel;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().InPlayMode())
		{
			ChimeraWorld world = GetGame().GetWorld();
			if (world)
			{
				HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
				HMD_UnregisterDesignationHudSlot(sys);
			}
			else
			{
				m_iDesignationId = -1;
				m_iLastRegisteredAttrKind = -999;
				m_sLastRegisteredHudLabel = "";
			}
		}
		else
		{
			m_iDesignationId = -1;
			m_iLastRegisteredAttrKind = -999;
			m_sLastRegisteredHudLabel = "";
		}
		HMD_LaserLockState.ClearIfLockedDesignator(this);
		super.OnDelete(owner);
	}
}
