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

	//------------------------------------------------------------------------------------------------
	//! World/layer overrides are merged into the component source container; runtime fields can stay at class defaults until read from `BaseContainer` (same pattern as HUDMarkerSystem + HMD_PlacedDesignationComponent).
	float GetLifetimeSeconds()
	{
		float outVal = m_fLifetimeSeconds;
		IEntity owner = GetOwner();
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
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		IEntity ent = GetOwner();
		ChimeraWorld world = GetGame().GetWorld();
		if (!GetGame().InPlayMode())
			return;
		if (!ent)
			return;
		if (world)
		{
			HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
			if (sys)
			{
				sys.Register(ent, GetLifetimeSeconds(), GetMarkerName(), GetMarkerColor().PackToInt(), GetLabelColor().PackToInt());
			}
			else
			{
				HUDMarkerSystem.EnqueuePending(ent);
			}
		}
		else
		{
			HUDMarkerSystem.EnqueuePending(ent);
		}
		//! Frame ticks for optional timed delete; EOnFrame returns immediately when GetLifetimeSeconds() <= 0.
		//! Always enable so prefab/layer lifetime applies even if attributes resolve after OnPostInit.
		SetEventMask(ent, EntityEvent.FRAME | ent.GetEventMask());
	}

	//------------------------------------------------------------------------------------------------
	//! Finite lifetime: authority deletes owner entity after GetLifetimeSeconds() (HUD pool entry goes stale via existing Unregister path).
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		if (!GetGame().InPlayMode() || !owner)
			return;
		float lifetimeSec = GetLifetimeSeconds();
		if (lifetimeSec <= 0)
			return;
		if (!HMD_MarkerLifetimeAuthority.ShouldRunTimedEntityDeleteAuthority(owner))
			return;
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		WorldTimestamp now = world.GetServerTimestamp();
		if (!m_bServerTimeStartSet)
		{
			m_ServerTimeStart = now;
			m_bServerTimeStartSet = true;
		}
		float elapsed = now.DiffMilliseconds(m_ServerTimeStart) * 0.001;
		if (elapsed < lifetimeSec)
			return;
		SCR_EntityHelper.DeleteEntityAndChildren(owner);
	}

	//------------------------------------------------------------------------------------------------
	void ~HUDMarkerComponent()
	{
		if (!GetGame().InPlayMode() || Replication.IsServer())
			return;
		IEntity owner = GetOwner();
		if (!owner)
			return;

		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (sys)
			sys.Unregister(owner);
	}
}
