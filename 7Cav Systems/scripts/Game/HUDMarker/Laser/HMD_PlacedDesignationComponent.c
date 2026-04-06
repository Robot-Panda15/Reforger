//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Test / mission: static laser spot at this entity. Registers WCS HandheldLaserDesignator (weapon lock) + HUDMarkerSystem dot.")]
class HMD_PlacedDesignationComponentClass : WCS_Armament_HandheldLaserDesignatorComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Extends WCS handheld designator so ALL_DESIGNATORS / weapon lock see a valid lasing spot at this entity's origin.
//! Also registers the same position in HUDMarkerSystem for dots / foreign visual kind.
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
			m_bIsDesignating = true;
			m_bHasValidDesignation = true;
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
	protected void HMD_SyncWcsDesignationFromOwner(IEntity owner)
	{
		if (!owner)
			return;
		m_vDesignatedLocation = owner.GetOrigin();
		m_bIsDesignating = true;
		m_bHasValidDesignation = true;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		if (!GetGame().InPlayMode())
			return;
		if (!owner)
			return;

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
				float elapsed = now.DiffMilliseconds(m_ServerTimeStart) * 0.001;
				if (elapsed >= lifetimeSec)
				{
					SCR_EntityHelper.DeleteEntityAndChildren(owner);
					return;
				}
			}
		}

		HMD_SyncWcsDesignationFromOwner(owner);
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return;
		vector pos = owner.GetOrigin();
		int attrKind = HMD_ResolveVisualKindAttr(owner);
		string hudLabel = HMD_ResolveLabelForHud(owner);
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
		if (GetGame().InPlayMode())
		{
			ChimeraWorld world = GetGame().GetWorld();
			if (world)
			{
				HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
				if (sys && m_iDesignationId >= 0)
					sys.UnregisterDesignation(m_iDesignationId);
			}
		}
		m_iDesignationId = -1;
		m_iLastRegisteredAttrKind = -999;
		m_sLastRegisteredHudLabel = "";
		super.OnDelete(owner);
	}
}
