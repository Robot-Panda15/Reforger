//------------------------------------------------------------------------------------------------
//! Client-side system holding registered HUD marker entities for display
//! Caches positions so markers remain visible when entities are streamed out
//! Lifetime: -1 = forever, 1-360 = seconds after stream-out before removal
//! Designations (laser designator aim / pooled HUD dots) are listed first in GetMarkerData
class HUDMarkerSystem : GameSystem
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Helmet prefabs with HMD (full marker HUD in vehicle + free look). Empty = no camera-only restriction for crew without HMD (unless Enforce below).", "et", category: "HMD")]
	protected ref array<ResourceName> m_aHmdHelmetPrefabs;
	[Attribute("0", UIWidgets.CheckBox, "When set, require HMD helmet (capability tag or prefab list match) in vehicles even if the helmet prefab list above is empty.", category: "HMD")]
	protected bool m_bEnforceHmdHelmetInVehicles;
	protected ref array<IEntity> m_Markers = {};
	protected ref array<vector> m_CachedPositions = {};
	protected ref array<string> m_Names = {};
	protected ref array<int> m_MarkerColors = {};
	protected ref array<int> m_LabelColors = {};
	protected ref array<RplId> m_RplIds = {};
	protected ref array<float> m_LifetimeSeconds = {};
	//! Stream-out stale retention: elapsed uses ChimeraWorld.GetServerTimestamp (not mission/world time scale).
	protected ref array<bool> m_bStaleStartSet = {};
	protected ref array<WorldTimestamp> m_StaleStartServerTime = {};
	protected static ref array<IEntity> s_aPendingMarkers = {};

	protected ref array<vector> m_DesignationPositions = {};
	protected ref array<string> m_DesignationNames = {};
	protected ref array<int> m_DesignationMarkerColors = {};
	protected ref array<int> m_DesignationLabelColors = {};
	protected ref array<float> m_DesignationVisibilityDistances = {};
	protected ref array<int> m_DesignationIds = {};
	protected ref array<int> m_DesignationVisualKinds = {};
	protected int m_iNextDesignationId = 1;
	protected ref map<int, int> m_DesignationIdToIndex = new map<int, int>();

	//------------------------------------------------------------------------------------------------
	static HUDMarkerSystem GetInstance(ChimeraWorld world)
	{
		if (!world)
			return null;
		return HUDMarkerSystem.Cast(world.FindSystem(HUDMarkerSystem));
	}

	//------------------------------------------------------------------------------------------------
	static void EnqueuePending(IEntity entity)
	{
		if (!entity)
			return;
		if (s_aPendingMarkers.Find(entity) >= 0)
			return;
		s_aPendingMarkers.Insert(entity);
	}

	//------------------------------------------------------------------------------------------------
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		outInfo.SetAbstract(false)
			.SetUnique(true)
			.SetLocation(WorldSystemLocation.Client)
			.AddPoint(WorldSystemPoint.Frame);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnInit()
	{
		super.OnInit();
		Enable(true);
	}

	//------------------------------------------------------------------------------------------------
	//! True when helmet list is set (enables vehicle camera-only mode for crew not wearing an HMD helmet).
	bool HasHmdHelmetPrefabConfig()
	{
		return m_aHmdHelmetPrefabs && m_aHmdHelmetPrefabs.Count() > 0;
	}

	//------------------------------------------------------------------------------------------------
	bool EnforceHmdHelmetInVehicles()
	{
		return m_bEnforceHmdHelmetInVehicles;
	}

	//------------------------------------------------------------------------------------------------
	bool LocalPlayerHelmetMatchesConfiguredPrefabList(SCR_ChimeraCharacter ch)
	{
		if (!HasHmdHelmetPrefabConfig() || !ch)
			return false;
		return HMD_HmdVehicleHudRestriction.CharacterHelmetMatchesPrefabList(ch, m_aHmdHelmetPrefabs);
	}

	//------------------------------------------------------------------------------------------------
	int RegisterDesignation(vector position, string name, int markerColorARGB, int labelColorARGB, float visibilityDistance = -1, int visualKind = -1)
	{
		int id = m_iNextDesignationId++;
		int idx = m_DesignationPositions.Count();
		m_DesignationPositions.Insert(position);
		m_DesignationNames.Insert(name);
		m_DesignationMarkerColors.Insert(markerColorARGB);
		m_DesignationLabelColors.Insert(labelColorARGB);
		m_DesignationVisibilityDistances.Insert(visibilityDistance);
		m_DesignationIds.Insert(id);
		int kindToStore = visualKind;
		if (visualKind == -1)
			kindToStore = HMD_MarkerVisuals.KIND_OWN_DESIGNATION;
		m_DesignationVisualKinds.Insert(kindToStore);
		m_DesignationIdToIndex.Set(id, idx);
		return id;
	}

	//------------------------------------------------------------------------------------------------
	void UpdateDesignation(int id, vector position)
	{
		int idx = -1;
		if (!m_DesignationIdToIndex.Find(id, idx))
			return;
		if (idx < 0 || idx >= m_DesignationPositions.Count())
			return;
		m_DesignationPositions[idx] = position;
	}

	//------------------------------------------------------------------------------------------------
	void UpdateDesignationName(int id, string name)
	{
		int idx = -1;
		if (!m_DesignationIdToIndex.Find(id, idx))
			return;
		if (idx < 0 || idx >= m_DesignationNames.Count())
			return;
		m_DesignationNames[idx] = name;
	}

	//------------------------------------------------------------------------------------------------
	//! Same mapping as RegisterDesignation (visualKind -1 = default).
	void UpdateDesignationVisualKind(int id, int visualKind)
	{
		int idx = -1;
		if (!m_DesignationIdToIndex.Find(id, idx))
			return;
		if (idx < 0 || idx >= m_DesignationVisualKinds.Count())
			return;
		int kindToStore = visualKind;
		if (visualKind == -1)
			kindToStore = HMD_MarkerVisuals.KIND_OWN_DESIGNATION;
		m_DesignationVisualKinds[idx] = kindToStore;
	}

	//------------------------------------------------------------------------------------------------
	void UpdateDesignationMarkerColors(int id, int markerColorARGB, int labelColorARGB)
	{
		int idx = -1;
		if (!m_DesignationIdToIndex.Find(id, idx))
			return;
		if (idx < 0 || idx >= m_DesignationMarkerColors.Count())
			return;
		m_DesignationMarkerColors[idx] = markerColorARGB;
		m_DesignationLabelColors[idx] = labelColorARGB;
	}

	//------------------------------------------------------------------------------------------------
	void UnregisterDesignation(int id)
	{
		int idx = -1;
		if (!m_DesignationIdToIndex.Find(id, idx))
			return;
		m_DesignationIdToIndex.Remove(id);
		int last = m_DesignationPositions.Count() - 1;
		if (idx < 0 || idx > last)
			return;
		if (idx != last)
		{
			m_DesignationPositions[idx] = m_DesignationPositions[last];
			m_DesignationNames[idx] = m_DesignationNames[last];
			m_DesignationMarkerColors[idx] = m_DesignationMarkerColors[last];
			m_DesignationLabelColors[idx] = m_DesignationLabelColors[last];
			m_DesignationVisibilityDistances[idx] = m_DesignationVisibilityDistances[last];
			m_DesignationVisualKinds[idx] = m_DesignationVisualKinds[last];
			int movedId = m_DesignationIds[last];
			m_DesignationIds[idx] = movedId;
			m_DesignationIdToIndex.Set(movedId, idx);
		}
		m_DesignationPositions.Remove(last);
		m_DesignationNames.Remove(last);
		m_DesignationMarkerColors.Remove(last);
		m_DesignationLabelColors.Remove(last);
		m_DesignationVisibilityDistances.Remove(last);
		m_DesignationIds.Remove(last);
		m_DesignationVisualKinds.Remove(last);
	}

	//------------------------------------------------------------------------------------------------
	void Register(IEntity entity, float lifetimeSeconds = -1, string markerName = "", int markerColorARGB = -1, int labelColorARGB = -1)
	{
		if (!entity)
			return;
		int dotColor;
		if (markerColorARGB >= 0)
			dotColor = markerColorARGB;
		else
			dotColor = Color.FromRGBA(0, 255, 0, 255).PackToInt();
		int lblColor;
		if (labelColorARGB >= 0)
			lblColor = labelColorARGB;
		else
			lblColor = Color.FromRGBA(255, 255, 255, 255).PackToInt();
		int existingIdx = m_Markers.Find(entity);
		if (existingIdx >= 0)
		{
			m_CachedPositions[existingIdx] = entity.GetOrigin();
			m_Names[existingIdx] = markerName;
			m_MarkerColors[existingIdx] = dotColor;
			m_LabelColors[existingIdx] = lblColor;
			m_LifetimeSeconds[existingIdx] = lifetimeSeconds;
			m_bStaleStartSet[existingIdx] = false;
			return;
		}
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		RplId id = RplId.Invalid();
		if (rpl)
			id = rpl.Id();
		if (id != RplId.Invalid())
		{
			for (int i = 0; i < m_Markers.Count(); i++)
			{
				if (m_RplIds[i] == id)
				{
					m_Markers[i] = entity;
					m_CachedPositions[i] = entity.GetOrigin();
					m_Names[i] = markerName;
					m_MarkerColors[i] = dotColor;
					m_LabelColors[i] = lblColor;
					m_LifetimeSeconds[i] = lifetimeSeconds;
					m_bStaleStartSet[i] = false;
					return;
				}
			}
		}
		m_Markers.Insert(entity);
		m_CachedPositions.Insert(entity.GetOrigin());
		m_Names.Insert(markerName);
		m_MarkerColors.Insert(dotColor);
		m_LabelColors.Insert(lblColor);
		m_RplIds.Insert(id);
		m_LifetimeSeconds.Insert(lifetimeSeconds);
		m_bStaleStartSet.Insert(false);
		WorldTimestamp staleInit;
		m_StaleStartServerTime.Insert(staleInit);
	}

	//------------------------------------------------------------------------------------------------
	//! Soft unregister - clears entity ref but keeps cached position so marker stays visible when streamed out
	void Unregister(IEntity entity)
	{
		if (!entity)
			return;
		int idx = m_Markers.Find(entity);
		if (idx >= 0)
		{
			m_Markers[idx] = null;
			ChimeraWorld world = GetWorld();
			if (world)
			{
				m_StaleStartServerTime[idx] = world.GetServerTimestamp();
				m_bStaleStartSet[idx] = true;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Removes this entity from the IFF pool immediately (no stale retention). Use for placeable IFF beacons; HUDMarkerComponent stream-out still uses Unregister.
	void RemoveMarkerEntry(IEntity entity)
	{
		if (!entity)
			return;
		int idx = m_Markers.Find(entity);
		if (idx < 0)
		{
			RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
			RplId id = RplId.Invalid();
			if (rpl)
				id = rpl.Id();
			if (id != RplId.Invalid())
			{
				for (int i = 0; i < m_Markers.Count(); i++)
				{
					if (m_RplIds[i] == id)
					{
						idx = i;
						break;
					}
				}
			}
		}
		if (idx < 0)
			return;
		m_Markers.RemoveOrdered(idx);
		m_CachedPositions.RemoveOrdered(idx);
		m_Names.RemoveOrdered(idx);
		m_MarkerColors.RemoveOrdered(idx);
		m_LabelColors.RemoveOrdered(idx);
		m_RplIds.RemoveOrdered(idx);
		m_LifetimeSeconds.RemoveOrdered(idx);
		m_bStaleStartSet.RemoveOrdered(idx);
		m_StaleStartServerTime.RemoveOrdered(idx);
	}

	//------------------------------------------------------------------------------------------------
	//! Streamed-out IFF pool entries: remove when finite lifetime elapsed (GetServerTimestamp). Runs every frame so expiry does not depend on GetMarkerData being called.
	protected void ProcessStaleIffStreamedOutEntries()
	{
		ChimeraWorld world = GetWorld();
		if (!world)
			return;
		WorldTimestamp nowServer = world.GetServerTimestamp();
		for (int i = m_Markers.Count() - 1; i >= 0; i--)
		{
			IEntity e = m_Markers[i];
			if (e)
				continue;
			if (!m_bStaleStartSet[i])
			{
				m_StaleStartServerTime[i] = nowServer;
				m_bStaleStartSet[i] = true;
			}
			float lifetime = m_LifetimeSeconds[i];
			if (lifetime <= 0)
				continue;
			float elapsed = HMD_MarkerLifetimeAuthority.GetElapsedSecondsSinceServerTime(m_StaleStartServerTime[i], world);
			if (elapsed < lifetime)
				continue;
			m_Markers.RemoveOrdered(i);
			m_CachedPositions.RemoveOrdered(i);
			m_Names.RemoveOrdered(i);
			m_MarkerColors.RemoveOrdered(i);
			m_LabelColors.RemoveOrdered(i);
			m_RplIds.RemoveOrdered(i);
			m_LifetimeSeconds.RemoveOrdered(i);
			m_bStaleStartSet.RemoveOrdered(i);
			m_StaleStartServerTime.RemoveOrdered(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AppendDesignationsToOutput(array<vector> positions, array<string> names, array<int> markerColors, array<int> labelColors, array<float> visibilityDistances, array<int> markerVisualKinds)
	{
		for (int v = 0; v < m_DesignationPositions.Count(); v++)
		{
			positions.Insert(m_DesignationPositions[v]);
			names.Insert(m_DesignationNames[v]);
			if (markerColors)
				markerColors.Insert(m_DesignationMarkerColors[v]);
			if (labelColors)
				labelColors.Insert(m_DesignationLabelColors[v]);
			if (visibilityDistances)
				visibilityDistances.Insert(m_DesignationVisibilityDistances[v]);
			if (markerVisualKinds)
				markerVisualKinds.Insert(m_DesignationVisualKinds[v]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fills positions, names and color arrays - uses cached position when entity is streamed out. Stale IFF removal uses server time in ProcessStaleIffStreamedOutEntries (OnUpdate).
	//! @param markerVisualKinds When non-null, filled per dot: HMD_MarkerVisuals KIND_* (IFF / own designation / foreign designation).
	//! @param includeLocalLaserDesignations Pooled designations (own turret, mission spots, etc.).
	//! @param includeForeignLaserDesignations Other players' designators (WCS); gated by Numpad * separately from own laser.
	void GetMarkerData(array<vector> positions, array<string> names, array<int> markerColors, array<int> labelColors, array<float> visibilityDistances, array<int> markerVisualKinds, bool includeLocalLaserDesignations, bool includeForeignLaserDesignations, bool includeIffMarkers)
	{
		if (!positions || !names)
			return;
		positions.Clear();
		names.Clear();
		if (markerColors)
			markerColors.Clear();
		if (labelColors)
			labelColors.Clear();
		if (visibilityDistances)
			visibilityDistances.Clear();
		if (markerVisualKinds)
			markerVisualKinds.Clear();

		if (includeLocalLaserDesignations)
			AppendDesignationsToOutput(positions, names, markerColors, labelColors, visibilityDistances, markerVisualKinds);
		if (includeForeignLaserDesignations)
			HMD_MarkerVisuals.AppendForeignDesignations(positions, names, markerColors, labelColors, visibilityDistances, markerVisualKinds);

		if (!includeIffMarkers)
			return;

		ChimeraWorld world = GetWorld();
		if (!world)
			return;
		WorldTimestamp nowServer = world.GetServerTimestamp();
		for (int i = m_Markers.Count() - 1; i >= 0; i--)
		{
			IEntity e = m_Markers[i];
			vector pos;
			float visDist = -1;
			if (e)
			{
				pos = e.GetOrigin();
				m_CachedPositions[i] = pos;
				m_bStaleStartSet[i] = false;
				HUDMarkerComponent comp = HUDMarkerComponent.Cast(e.FindComponent(HUDMarkerComponent));
				if (comp)
				{
					string compName = comp.GetMarkerName();
					if (!compName || compName.Length() == 0)
					{
						BaseContainer src = comp.GetComponentSource(comp.GetOwner());
						if (src)
							src.Get("m_sMarkerName", compName);
					}
					m_Names[i] = compName;
					m_MarkerColors[i] = comp.GetMarkerColor().PackToInt();
					m_LabelColors[i] = comp.GetLabelColor().PackToInt();
					visDist = comp.GetVisibilityDistance();
				}
				positions.Insert(pos);
				names.Insert(m_Names[i]);
				if (markerColors)
					markerColors.Insert(m_MarkerColors[i]);
				if (labelColors)
					labelColors.Insert(m_LabelColors[i]);
				if (visibilityDistances)
					visibilityDistances.Insert(visDist);
				if (markerVisualKinds)
					markerVisualKinds.Insert(HMD_MarkerVisuals.KIND_IFF_MARKER);
			}
			else
			{
				if (!m_bStaleStartSet[i])
				{
					m_StaleStartServerTime[i] = nowServer;
					m_bStaleStartSet[i] = true;
				}
				pos = m_CachedPositions[i];
				positions.Insert(pos);
				names.Insert(m_Names[i]);
				if (markerColors)
					markerColors.Insert(m_MarkerColors[i]);
				if (labelColors)
					labelColors.Insert(m_LabelColors[i]);
				if (visibilityDistances)
					visibilityDistances.Insert(-1);
				if (markerVisualKinds)
					markerVisualKinds.Insert(HMD_MarkerVisuals.KIND_IFF_MARKER);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	array<vector> GetMarkerPositions()
	{
		array<vector> positions = {};
		array<string> names = {};
		array<int> markerColors = {};
		array<int> labelColors = {};
		GetMarkerData(positions, names, markerColors, labelColors, null, null, true, true, true);
		return positions;
	}

	//------------------------------------------------------------------------------------------------
	array<IEntity> GetMarkers()
	{
		array<IEntity> result = {};
		foreach (IEntity e : m_Markers)
		{
			if (e)
				result.Insert(e);
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	bool TryGetDesignationWorldPositionById(int id, out vector outPos)
	{
		int idx = -1;
		if (!m_DesignationIdToIndex.Find(id, idx))
			return false;
		if (idx < 0 || idx >= m_DesignationPositions.Count())
			return false;
		outPos = m_DesignationPositions[idx];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool TryGetDesignationNameById(int id, out string outName)
	{
		outName = "";
		int idx = -1;
		if (!m_DesignationIdToIndex.Find(id, idx))
			return false;
		if (idx < 0 || idx >= m_DesignationNames.Count())
			return false;
		outName = m_DesignationNames[idx];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! HUD-only designations (e.g. placed test spots) for lock cycle (LSHIFT): ids within camera cone, sorted by angle.
	void CollectDesignationIdsInCameraCone(float maxHalfAngleDeg, array<int> outIds)
	{
		if (!outIds)
			return;
		outIds.Clear();
		ChimeraWorld world = GetWorld();
		if (!world)
			return;
		vector camTM[4];
		world.GetCurrentCamera(camTM);
		vector camPos = camTM[3];
		vector forward = camTM[2];
		float flen = forward.Length();
		if (flen < 0.001)
			return;
		forward = forward * (1.0 / flen);
		ref array<int> tmpIds = {};
		ref array<float> tmpAngles = {};
		int n = m_DesignationPositions.Count();
		for (int i = 0; i < n; i++)
		{
			vector pos = m_DesignationPositions[i];
			vector toT = pos - camPos;
			float dlen = toT.Length();
			if (dlen < 0.01)
				continue;
			toT = toT * (1.0 / dlen);
			float dot = vector.Dot(forward, toT);
			dot = Math.Clamp(dot, -1.0, 1.0);
			float angDeg = Math.Acos(dot) * Math.RAD2DEG;
			if (angDeg > maxHalfAngleDeg)
				continue;
			tmpIds.Insert(m_DesignationIds[i]);
			tmpAngles.Insert(angDeg);
		}
		int cnt = tmpIds.Count();
		int a;
		int b;
		for (a = 0; a < cnt; a++)
		{
			for (b = a + 1; b < cnt; b++)
			{
				if (tmpAngles[a] > tmpAngles[b])
				{
					float tf = tmpAngles[a];
					tmpAngles[a] = tmpAngles[b];
					tmpAngles[b] = tf;
					int tid = tmpIds[a];
					tmpIds[a] = tmpIds[b];
					tmpIds[b] = tid;
				}
			}
		}
		for (int j = 0; j < cnt; j++)
			outIds.Insert(tmpIds[j]);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnUpdate(ESystemPoint point)
	{
		super.OnUpdate(point);
		if (point != ESystemPoint.Frame)
			return;
		ProcessStaleIffStreamedOutEntries();
		if (s_aPendingMarkers.IsEmpty())
			return;
		ChimeraWorld world = GetWorld();
		if (!world)
			return;
		for (int i = s_aPendingMarkers.Count() - 1; i >= 0; i--)
		{
			IEntity ent = s_aPendingMarkers[i];
			if (!ent)
			{
				s_aPendingMarkers.RemoveOrdered(i);
				continue;
			}
			HUDMarkerComponent comp = HUDMarkerComponent.Cast(ent.FindComponent(HUDMarkerComponent));
			if (!comp)
			{
				s_aPendingMarkers.RemoveOrdered(i);
				continue;
			}
			Register(ent, comp.GetLifetimeSeconds(), comp.GetMarkerName(), comp.GetMarkerColor().PackToInt(), comp.GetLabelColor().PackToInt());
			s_aPendingMarkers.RemoveOrdered(i);
		}
	}
}
