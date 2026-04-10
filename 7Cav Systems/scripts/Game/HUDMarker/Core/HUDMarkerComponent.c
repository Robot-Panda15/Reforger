//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "IFF marker: shows entity world position as a colored dot on the player HUD (visibility). Dot and label colors configurable.")]
class HUDMarkerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class HUDMarkerComponent : ScriptComponent
{
	[Attribute("-1", UIWidgets.Slider, "Marker lifetime (s). -1 = infinite. >0 = delete this entity after that many seconds (server), and seconds to keep cached HUD dot after stream-out.", "-1 360 1", category: "HUD")]
	protected float m_fLifetimeSeconds;

	//! Server timestamp at first expiry tick (GetServerTimestamp; not affected by mission time scale).
	protected WorldTimestamp m_ServerTimeStart;
	protected bool m_bServerTimeStartSet;

	[Attribute("", UIWidgets.EditBox, "Optional marker name shown under the dot on HUD. Leave empty for no label.", category: "HUD")]
	protected string m_sMarkerName;

	[Attribute("0 1 0 1", UIWidgets.ColorPicker, "Marker dot color (RGBA 0-1)", category: "HUD")]
	protected ref Color m_MarkerColor;

	[Attribute("1 1 1 1", UIWidgets.ColorPicker, "Marker label color (RGBA 0-1)", category: "HUD")]
	protected ref Color m_LabelColor;

	[Attribute("-1", UIWidgets.Slider, "Max visibility distance (m) for HUD culling/fade. -1 = no limit.", "-1 10000 100", category: "HUD")]
	protected float m_fVisibilityDistance;

	//! Retries when GetGame() was missing at OnPostInit; otherwise registration uses EnqueuePending when not yet InPlayMode.
	protected bool m_bPendingHudRegistration;

	//------------------------------------------------------------------------------------------------
	//! Same resolution pattern as HMD_PlacedDesignationComponent.HMD_GetLifetimeSeconds (prefab / layer container).
	protected float HMD_ResolveLifetimeSeconds(IEntity owner)
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
	float GetLifetimeSeconds()
	{
		return HMD_ResolveLifetimeSeconds(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	string GetMarkerName()
	{
		string n = m_sMarkerName;
		IEntity owner = GetOwner();
		if (!owner)
			return n;
		if (n && n.Length() > 0)
			return n;
		BaseContainer src = GetComponentSource(owner);
		if (src)
			src.Get("m_sMarkerName", n);
		return n;
	}

	//------------------------------------------------------------------------------------------------
	Color GetMarkerColor()
	{
		if (m_MarkerColor)
			return m_MarkerColor;
		return Color.FromRGBA(0, 255, 0, 255);
	}

	//------------------------------------------------------------------------------------------------
	Color GetLabelColor()
	{
		if (m_LabelColor)
			return m_LabelColor;
		return Color.FromRGBA(255, 255, 255, 255);
	}

	//------------------------------------------------------------------------------------------------
	float GetVisibilityDistance()
	{
		return m_fVisibilityDistance;
	}

	//------------------------------------------------------------------------------------------------
	//! Iff beacon/lifetime use RegisterIffMarker (not entity Register). When this component is on the same entity, pooled row uses its colors and visibility; non-empty marker name overrides the dynamic beacon label.
	static void HMD_ResolveIffPoolPresentation(IEntity owner, string dynamicLabel, out string outLabel, out int outMarkerARGB, out int outLabelARGB, out float outVisibilityDistance)
	{
		outLabel = dynamicLabel;
		outMarkerARGB = Color.FromRGBA(0, 255, 0, 255).PackToInt();
		outLabelARGB = Color.FromRGBA(255, 255, 255, 255).PackToInt();
		outVisibilityDistance = -1;
		if (!owner)
			return;
		HUDMarkerComponent hud = HUDMarkerComponent.Cast(owner.FindComponent(HUDMarkerComponent));
		if (!hud)
			return;
		string mn = hud.GetMarkerName();
		if (mn && mn.Length() > 0)
			outLabel = mn;
		outMarkerARGB = hud.GetMarkerColor().PackToInt();
		outLabelARGB = hud.GetLabelColor().PackToInt();
		outVisibilityDistance = hud.GetVisibilityDistance();
	}

	//------------------------------------------------------------------------------------------------
	//! Register with HUDMarkerSystem when possible; if not InPlayMode yet, enqueue for frame flush until play/world/system exist.
	protected void HMD_TryRegisterWithHudMarkerSystem(IEntity ent)
	{
		if (!ent || !GetGame())
			return;

		m_bPendingHudRegistration = false;

		if (HMD_IffBeaconComponent.Cast(ent.FindComponent(HMD_IffBeaconComponent)) != null
			|| HMD_IffLifetimeComponent.Cast(ent.FindComponent(HMD_IffLifetimeComponent)) != null)
			return;

		if (!GetGame().InPlayMode())
		{
			HUDMarkerSystem.EnqueuePending(ent);
			return;
		}

		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
		{
			HUDMarkerSystem.EnqueuePending(ent);
			return;
		}
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
		{
			HUDMarkerSystem.EnqueuePending(ent);
			return;
		}
		sys.Register(ent, GetLifetimeSeconds(), GetMarkerName(), GetMarkerColor().PackToInt(), GetLabelColor().PackToInt());
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		IEntity ent = GetOwner();
		if (!ent)
			return;

		//! Always enable FRAME: lifetime ticks + deferred HUD registration when spawn precedes InPlayMode or system init.
		SetEventMask(ent, EntityEvent.FRAME | ent.GetEventMask());

		if (HMD_IffBeaconComponent.Cast(ent.FindComponent(HMD_IffBeaconComponent)) != null
			|| HMD_IffLifetimeComponent.Cast(ent.FindComponent(HMD_IffLifetimeComponent)) != null)
			return;

		if (!GetGame())
		{
			m_bPendingHudRegistration = true;
			return;
		}
		HMD_TryRegisterWithHudMarkerSystem(ent);
	}

	//------------------------------------------------------------------------------------------------
	//! Match HMD_PlacedDesignationComponent.EOnFrame lifetime block (server time + same authority gate). ScriptComponent has no Update(); Placed uses WCS base empty Update.
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		if (!owner)
			return;

		if (m_bPendingHudRegistration)
			HMD_TryRegisterWithHudMarkerSystem(owner);

		if (!GetGame().InPlayMode())
			return;

		float lifetimeSec = HMD_ResolveLifetimeSeconds(owner);
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
	}

	//------------------------------------------------------------------------------------------------
	//! Entity teardown: drop IFF row immediately. Unregister() is for stream-out (keeps cached dot); deletion needs RemoveMarkerEntry.
	override void OnDelete(IEntity owner)
	{
		if (owner && GetGame())
		{
			ChimeraWorld world = GetGame().GetWorld();
			if (world)
			{
				HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
				if (sys)
					sys.RemoveMarkerEntry(owner);
			}
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	void ~HUDMarkerComponent()
	{
		if (!GetGame())
			return;
		IEntity owner = GetOwner();
		if (!owner)
			return;

		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (sys)
			sys.RemoveMarkerEntry(owner);
	}
}
