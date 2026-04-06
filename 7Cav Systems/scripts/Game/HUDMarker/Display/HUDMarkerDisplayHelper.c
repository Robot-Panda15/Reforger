//------------------------------------------------------------------------------------------------

//! Distance sort, per-marker max range, screen cull, and 75-100% range opacity fade before projection

//! CPU baseline: use Workbench profiler on marker-heavy scenes before tuning.

//! Laser designation dots and IFF markers use separate layout roots (each has its own full widget pool).

class HUDMarkerDisplayHelper

{

	static const float MARKER_LABEL_OFFSET_Y = 12.0;

	static const float MARKER_LABEL_WIDTH = 107.0;

	static const float MARKER_LABEL_HEIGHT = 18.0;

	static const string MARKER_DOT_TEXTURE = "{73B3D8BBB785B5B9}UI/Textures/Common/circleFull.edds";



	protected static ref array<string> s_aLastDotTexturesLaser;

	protected static ref array<string> s_aLastDotTexturesIff;



	//------------------------------------------------------------------------------------------------

	static void HideMarkerWidgetPool(array<ImageWidget> markerDots, array<TextWidget> markerLabels)

	{

		int i;

		if (markerDots)

		{

			for (i = 0; i < markerDots.Count(); i++)

			{

				ImageWidget dot = markerDots[i];

				if (dot)

					dot.SetVisible(false);

			}

		}

		if (markerLabels)

		{

			for (i = 0; i < markerLabels.Count(); i++)

			{

				TextWidget label = markerLabels[i];

				if (label)

					label.SetVisible(false);

			}

		}

	}



	//------------------------------------------------------------------------------------------------

	static void FillMarkerPoolFromRoot(Widget root, int poolSize, array<ImageWidget> outDots, array<TextWidget> outLabels)

	{

		if (!root || !outDots || !outLabels || poolSize <= 0)

			return;

		for (int i = 0; i < poolSize; i++)

		{

			Widget w = root.FindAnyWidget(string.Format("MarkerDot%1", i));

			if (w)

			{

				ImageWidget img = ImageWidget.Cast(w);

				if (img)

				{

					img.LoadImageTexture(0, MARKER_DOT_TEXTURE, false, false);

					outDots.Insert(img);

					img.SetVisible(false);

				}

			}

			Widget lw = root.FindAnyWidget(string.Format("MarkerLabel%1", i));

			if (lw)

			{

				TextWidget txt = TextWidget.Cast(lw);

				if (txt)

				{

					outLabels.Insert(txt);

					txt.SetVisible(false);

				}

			}

		}

	}



	//------------------------------------------------------------------------------------------------

	protected static void FetchRenderOrHideOneMarkerKind(

		HUDMarkerSystem sys,

		bool include,

		bool laserDesignation,

		bool iffMarkers,

		array<vector> positions,

		array<string> names,

		array<int> markerColors,

		array<int> labelColors,

		array<float> visibilityDistances,

		array<int> markerVisualKinds,

		BaseWorld world,

		WorkspaceWidget workspace,

		array<ImageWidget> dots,

		array<TextWidget> labels,

		float markerDotSize,

		bool useIffTextureCache)

	{

		if (!include)

		{

			HideMarkerWidgetPool(dots, labels);

			return;

		}

		sys.GetMarkerData(positions, names, markerColors, labelColors, visibilityDistances, markerVisualKinds, laserDesignation, iffMarkers);

		HMD_LaserLockState.ApplyLockedHighlightToMarkerArrays(positions, labelColors);

		RenderSortedMarkers(positions, names, markerColors, labelColors, visibilityDistances, markerVisualKinds, world, workspace, dots, labels, markerDotSize, MARKER_LABEL_OFFSET_Y, MARKER_LABEL_WIDTH, MARKER_LABEL_HEIGHT, useIffTextureCache);

	}

	//------------------------------------------------------------------------------------------------

	static void FetchAndRenderWorldMarkersFromSystem(

		HUDMarkerSystem sys,

		array<vector> positions,

		array<string> names,

		array<int> markerColors,

		array<int> labelColors,

		array<float> visibilityDistances,

		array<int> markerVisualKinds,

		BaseWorld world,

		WorkspaceWidget workspace,

		array<ImageWidget> laserDots,

		array<TextWidget> laserLabels,

		array<ImageWidget> iffDots,

		array<TextWidget> iffLabels,

		float markerDotSize)

	{

		if (!sys || !world || !workspace)

			return;

		bool incLaser = HUDMarkerVisibility.ShouldIncludeLaserDesignationDotsInHUD();

		bool incIff = HUDMarkerVisibility.ShouldIncludeIffMarkersInHUD();

		if (!incLaser && !incIff)

		{

			HideMarkerWidgetPool(laserDots, laserLabels);

			HideMarkerWidgetPool(iffDots, iffLabels);

			return;

		}

		FetchRenderOrHideOneMarkerKind(sys, incLaser, true, false, positions, names, markerColors, labelColors, visibilityDistances, markerVisualKinds, world, workspace, laserDots, laserLabels, markerDotSize, false);

		FetchRenderOrHideOneMarkerKind(sys, incIff, false, true, positions, names, markerColors, labelColors, visibilityDistances, markerVisualKinds, world, workspace, iffDots, iffLabels, markerDotSize, true);

	}



	//------------------------------------------------------------------------------------------------

	protected static float DistanceSq(vector a, vector b)

	{

		float dx = a[0] - b[0];

		float dy = a[1] - b[1];

		float dz = a[2] - b[2];

		return dx * dx + dy * dy + dz * dz;

	}



	//------------------------------------------------------------------------------------------------

	protected static void ApplyMarkerDotTextureCache(ImageWidget dot, int dotIndex, string texPath, bool useIffTextureCache)

	{

		if (!dot)

			return;

		if (useIffTextureCache)

		{

			if (!s_aLastDotTexturesIff)

				s_aLastDotTexturesIff = new array<string>();

			while (s_aLastDotTexturesIff.Count() <= dotIndex)

				s_aLastDotTexturesIff.Insert("");

			if (texPath != s_aLastDotTexturesIff[dotIndex])

			{

				dot.LoadImageTexture(0, texPath, false, false);

				s_aLastDotTexturesIff[dotIndex] = texPath;

			}

		}

		else

		{

			if (!s_aLastDotTexturesLaser)

				s_aLastDotTexturesLaser = new array<string>();

			while (s_aLastDotTexturesLaser.Count() <= dotIndex)

				s_aLastDotTexturesLaser.Insert("");

			if (texPath != s_aLastDotTexturesLaser[dotIndex])

			{

				dot.LoadImageTexture(0, texPath, false, false);

				s_aLastDotTexturesLaser[dotIndex] = texPath;

			}

		}

	}



	//------------------------------------------------------------------------------------------------

	//! @param useIffTextureCache Separate static texture cache per layout (both use 0..N-1 indices).

	static void RenderSortedMarkers(

		array<vector> positions,

		array<string> names,

		array<int> markerColors,

		array<int> labelColors,

		array<float> visibilityDistances,

		array<int> markerVisualKinds,

		BaseWorld world,

		WorkspaceWidget workspace,

		array<ImageWidget> markerDots,

		array<TextWidget> markerLabels,

		float markerDotSize,

		float labelOffsetY,

		float labelWidth,

		float labelHeight,

		bool useIffTextureCache)

	{

		if (!positions || !world || !workspace || !markerDots || markerDots.IsEmpty())

			return;



		if (positions.Count() == 0)

		{

			HideMarkerWidgetPool(markerDots, markerLabels);

			return;

		}



		vector camTM[4];

		world.GetCurrentCamera(camTM);

		vector camPos = camTM[3];



		int screenWPhys = workspace.GetWidth();

		int screenHPhys = workspace.GetHeight();

		float screenW = workspace.DPIUnscale(screenWPhys);

		float screenH = workspace.DPIUnscale(screenHPhys);



		int n = positions.Count();

		array<int> candIdx = {};

		array<float> candDistSq = {};

		for (int i = 0; i < n; i++)

		{

			float maxV = -1;

			if (visibilityDistances && i < visibilityDistances.Count())

				maxV = visibilityDistances[i];

			float distSq = DistanceSq(camPos, positions[i]);

			if (maxV > 0)

			{

				float maxVSq = maxV * maxV;

				if (distSq > maxVSq)

					continue;

			}

			candIdx.Insert(i);

			candDistSq.Insert(distSq);

		}



		int nc = candIdx.Count();

		for (int ii = 1; ii < nc; ii++)

		{

			int keyIdx = candIdx[ii];

			float keyD = candDistSq[ii];

			int j = ii - 1;

			while (j >= 0 && candDistSq[j] > keyD)

			{

				candIdx[j + 1] = candIdx[j];

				candDistSq[j + 1] = candDistSq[j];

				j--;

			}

			candIdx[j + 1] = keyIdx;

			candDistSq[j + 1] = keyD;

		}



		int dotIndex = 0;

		int pool = markerDots.Count();



		for (int s = 0; s < nc && dotIndex < pool; s++)

		{

			int p = candIdx[s];

			float distSq = candDistSq[s];

			vector worldPos = positions[p];

			float maxV = -1;

			if (visibilityDistances && p < visibilityDistances.Count())

				maxV = visibilityDistances[p];

			float dist = Math.Sqrt(distSq);

			float opacity = 1.0;

			if (maxV > 0)

			{

				float fadeStart = 0.75 * maxV;

				if (dist > fadeStart)

				{

					float span = 0.25 * maxV;

					if (span > 0)

						opacity = 1.0 - ((dist - fadeStart) / span);

					else

						opacity = 0;

					if (opacity < 0)

						opacity = 0;

					if (opacity > 1)

						opacity = 1;

				}

			}

			if (opacity <= 0)

				continue;



			string name = "";

			if (names && p < names.Count())

				name = names[p];

			Color dotColor;

			if (markerColors && p < markerColors.Count())

				dotColor = Color.FromInt(markerColors[p]);

			else

				dotColor = Color.FromRGBA(0, 255, 0, 255);

			Color lblColor;

			if (labelColors && p < labelColors.Count())

				lblColor = Color.FromInt(labelColors[p]);

			else

				lblColor = Color.FromRGBA(255, 255, 255, 255);



			vector screenPos = workspace.ProjWorldToScreen(worldPos, world);

			if (screenPos[2] <= 0)

				continue;



			float posX = screenPos[0];

			float posY = screenPos[1];

			if (posX < 0 || posY < 0 || posX > screenW || posY > screenH)

				continue;



			ImageWidget dot = markerDots[dotIndex];

			if (dot)

			{

				int kind = HMD_MarkerVisuals.KIND_IFF_MARKER;

				if (markerVisualKinds && p < markerVisualKinds.Count())

					kind = markerVisualKinds[p];

				bool lockedMatch = false;

				if (HMD_LaserLockState.IsLocked())

				{

					vector lp;

					if (HMD_LaserLockState.TryGetLockedTargetWorldPosition(lp))

						lockedMatch = HMD_MarkerVisuals.LockedPositionMatches(worldPos, lp, HMD_MarkerVisuals.LOCK_MATCH_M);

				}

				string texPath = HMD_MarkerVisuals.ResolveMarkerDotTexture(kind, lockedMatch);

				ApplyMarkerDotTextureCache(dot, dotIndex, texPath, useIffTextureCache);

				//! Foreign: Lase_Element square + laser code label only (no green circle HUD dot).

				if (lockedMatch || kind == HMD_MarkerVisuals.KIND_FOREIGN_DESIGNATION)

					dot.SetColor(Color.FromRGBA(255, 255, 255, 255));

				else

					dot.SetColor(dotColor);

				dot.SetOpacity(opacity);

				float dSize = markerDotSize;

				bool foreignDesignationSquare = (kind == HMD_MarkerVisuals.KIND_FOREIGN_DESIGNATION);

				if (foreignDesignationSquare)

					dSize = markerDotSize * HMD_MarkerVisuals.LASER_DESIGNATOR_DOT_SIZE_MULT;

				FrameSlot.SetPos(dot, posX - (dSize * 0.5), posY - (dSize * 0.5));

				FrameSlot.SetSize(dot, dSize, dSize);

				dot.SetVisible(true);

				if (markerLabels && dotIndex < markerLabels.Count())

				{

					TextWidget label = markerLabels[dotIndex];

					if (label)

					{

						//! Own lase: dot only; laser code is shown in rangefinder / vehicle HUD elsewhere.

						if (kind == HMD_MarkerVisuals.KIND_OWN_DESIGNATION)

						{

							label.SetVisible(false);

						}

						else

						{

							string displayName = "Marker";

							if (name && name.Length() > 0)

								displayName = name;

							label.SetText(displayName);

							float labelY = posY + labelOffsetY;

							if (foreignDesignationSquare)

							{

								float extra = (dSize - markerDotSize) * 0.5;

								labelY = labelY + extra;

							}

							FrameSlot.SetPos(label, posX - (labelWidth * 0.5), labelY);

							FrameSlot.SetSize(label, labelWidth, labelHeight);

							label.SetColor(lblColor);

							label.SetOutline(2, 0xFF000000);

							label.SetShadow(2, 0xFF000000, 1, 0, 0);

							label.SetExactFontSize(16);

							label.SetEnabled(true);

							label.SetOpacity(opacity);

							label.SetVisible(true);

						}

					}

				}

			}

			dotIndex++;

		}



		for (int i = dotIndex; i < pool; i++)

		{

			ImageWidget dot = markerDots[i];

			if (dot)

			{

				dot.SetVisible(false);

				if (markerLabels && i < markerLabels.Count())

				{

					TextWidget label = markerLabels[i];

					if (label)

						label.SetVisible(false);

				}

			}

		}

	}

}


