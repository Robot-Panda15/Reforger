//------------------------------------------------------------------------------------------------
// DecalMaterialSwitcherComponent - Cycles through a series of slide prefabs. Next/Previous Slide
// spawn the prefab for the current index. Add with ActionsManagerComponent containing
// NextSlideUserAction and PreviousSlideUserAction. Buttons are hidden when only 1 prefab.
// Requires RplComponent for multiplayer.
//------------------------------------------------------------------------------------------------

class DecalMaterialSwitcherComponentClass: ScriptComponentClass
{
}

class DecalMaterialSwitcherComponent: ScriptComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Slide prefabs to cycle through (one per slide)", "et")]
	protected ref array<ResourceName> m_aSlidePrefabs;

	[Attribute("0.003 2.521 0.046", UIWidgets.EditBox, "Slide position (X Y Z) relative to parent")]
	protected vector m_vDecalPosition;

	[Attribute("0 180 0", UIWidgets.EditBox, "Slide angles (X Y Z) in degrees")]
	protected vector m_vDecalAngles;

	[Attribute("3.34", UIWidgets.EditBox, "Slide scale")]
	protected float m_fDecalScale;

	[RplProp(condition: RplCondition.NoOwner, onRplName: "OnDecalIndexChanged")]
	protected int m_iCurrentDecalIndex = 0;

	protected IEntity m_DecalChildEntity;
	protected bool m_bInitialized = false;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		Print("[DecalSwitcher] EOnInit: owner=" + owner.GetName(), LogLevel.NORMAL);
		if (!m_aSlidePrefabs || m_aSlidePrefabs.Count() == 0)
		{
			Print("[DecalMaterialSwitcherComponent] No slide prefabs configured - add prefabs to m_aSlidePrefabs", LogLevel.ERROR);
			return;
		}
		m_bInitialized = true;
		SpawnSlideForCurrentIndex(owner);
		Print("[DecalSwitcher] EOnInit: spawned slide prefab", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Build 4x4 transform from position, angles (X Y Z degrees), and scale. AnglesToMatrix expects (yaw,pitch,roll)=(Y,X,Z).
	void BuildTransformFromTemplate(out vector transform[4])
	{
		vector anglesYPR = Vector(m_vDecalAngles[1], m_vDecalAngles[0], m_vDecalAngles[2]);
		vector rotMat[3];
		Math3D.AnglesToMatrix(anglesYPR, rotMat);
		Math3D.MatrixScale(rotMat, m_fDecalScale);
		transform[0] = rotMat[0];
		transform[1] = rotMat[1];
		transform[2] = rotMat[2];
		transform[3] = m_vDecalPosition;
	}

	//------------------------------------------------------------------------------------------------
	void CycleToNextSlide()
	{
		Print("[DecalSwitcher] CycleToNextSlide: called", LogLevel.NORMAL);
		if (!m_bInitialized || !m_aSlidePrefabs || m_aSlidePrefabs.Count() == 0)
		{
			Print("[DecalSwitcher] CycleToNextSlide: early return (not initialized or no prefabs)", LogLevel.NORMAL);
			return;
		}
		m_iCurrentDecalIndex = (m_iCurrentDecalIndex + 1) % m_aSlidePrefabs.Count();
		Replication.BumpMe();
		SpawnSlideForCurrentIndex(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	void CycleToPreviousSlide()
	{
		Print("[DecalSwitcher] CycleToPreviousSlide: called", LogLevel.NORMAL);
		if (!m_bInitialized || !m_aSlidePrefabs || m_aSlidePrefabs.Count() == 0)
		{
			Print("[DecalSwitcher] CycleToPreviousSlide: early return", LogLevel.NORMAL);
			return;
		}
		m_iCurrentDecalIndex--;
		if (m_iCurrentDecalIndex < 0)
			m_iCurrentDecalIndex = m_aSlidePrefabs.Count() - 1;
		Replication.BumpMe();
		SpawnSlideForCurrentIndex(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	void SetSlideByIndex(int index)
	{
		if (!m_aSlidePrefabs || m_aSlidePrefabs.Count() == 0)
			return;

		m_iCurrentDecalIndex = Math.Clamp(index, 0, m_aSlidePrefabs.Count() - 1);
		Replication.BumpMe();
		SpawnSlideForCurrentIndex(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	int GetCurrentIndex()
	{
		return m_iCurrentDecalIndex;
	}

	//------------------------------------------------------------------------------------------------
	int GetSlideCount()
	{
		if (!m_aSlidePrefabs)
			return 0;
		return m_aSlidePrefabs.Count();
	}

	//------------------------------------------------------------------------------------------------
	void OnDecalIndexChanged()
	{
		SpawnSlideForCurrentIndex(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	// Spawn the slide prefab for current index. Removes old slide entity if any.
	void SpawnSlideForCurrentIndex(IEntity owner)
	{
		if (!owner || !m_aSlidePrefabs || m_aSlidePrefabs.Count() == 0)
			return;
		int index = Math.Clamp(m_iCurrentDecalIndex, 0, m_aSlidePrefabs.Count() - 1);
		ResourceName prefabName = m_aSlidePrefabs.Get(index);
		if (prefabName.GetPath() == "")
		{
			Print("[DecalSwitcher] SpawnSlideForCurrentIndex: empty prefab at index " + index, LogLevel.ERROR);
			return;
		}

		// Remove existing slide entity
		if (m_DecalChildEntity)
		{
			IEntity parent = m_DecalChildEntity.GetParent();
			if (parent)
				parent.RemoveChild(m_DecalChildEntity, false);
			SCR_EntityHelper.DeleteEntityAndChildren(m_DecalChildEntity);
			m_DecalChildEntity = null;
		}

		Resource res = Resource.Load(prefabName);
		if (!res)
		{
			Print("[DecalSwitcher] SpawnSlideForCurrentIndex: Resource.Load failed for '" + prefabName.GetPath() + "'", LogLevel.ERROR);
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.LOCAL;
		params.Parent = owner;
		params.Scale = 1.0;
		BuildTransformFromTemplate(params.Transform);

		IEntity newSlide = GetGame().SpawnEntityPrefab(res, world, params);
		if (newSlide)
		{
			m_DecalChildEntity = newSlide;
			if (newSlide.GetParent() != owner)
				owner.AddChild(newSlide, -1, EAddChildFlags.NONE);
			vector ourTransform[4];
			vector parentWorld[4];
			vector worldTransform[4];
			BuildTransformFromTemplate(ourTransform);
			owner.GetWorldTransform(parentWorld);
			worldTransform[0] = ourTransform[0];
			worldTransform[1] = ourTransform[1];
			worldTransform[2] = ourTransform[2];
			worldTransform[3] = m_vDecalPosition.Multiply4(parentWorld);
			newSlide.SetWorldTransform(worldTransform);
			vector worldPos = newSlide.GetOrigin();
			vector localMat[4];
			newSlide.GetLocalTransform(localMat);
			Print("[DecalSwitcher] SpawnSlideForCurrentIndex: spawned slide " + index + " prefab=" + prefabName.GetPath(), LogLevel.NORMAL);
			Print("[DecalSwitcher] worldPos=(" + worldPos[0] + " " + worldPos[1] + " " + worldPos[2] + ")", LogLevel.NORMAL);
			Print("[DecalSwitcher] localPos=(" + localMat[3][0] + " " + localMat[3][1] + " " + localMat[3][2] + ")", LogLevel.NORMAL);
		}
		else
			Print("[DecalSwitcher] SpawnSlideForCurrentIndex: spawn failed", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	void DecalMaterialSwitcherComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
	}
}
