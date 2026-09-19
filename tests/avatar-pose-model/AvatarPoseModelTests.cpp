#include "AvatarPoseModel.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	const std::string kMap = "maps/01_rainy_hall.map";

	void Expect(bool abCondition, const std::string& asDescription)
	{
		if (abCondition) return;
		std::cerr << "FAIL: " << asDescription << "\n";
		exit(1);
	}

	bool Near(float afActual, float afExpected)
	{
		return std::fabs(afActual - afExpected) < 0.001f;
	}

	cAvatarPoseSample Pose(double afSenderTimeMs, float afX, float afYawDegrees = 0.0f,
		unsigned int alTeleportCounter = 0, const std::string& asMap = kMap)
	{
		cAvatarPoseSample sample;
		sample.mfSenderTimeMs = afSenderTimeMs;
		sample.mlTeleportCounter = alTeleportCounter;
		sample.mfX = afX;
		sample.mfY = 1.0f;
		sample.mfZ = -2.0f;
		sample.mfYawDegrees = afYawDegrees;
		sample.msMapFile = asMap;
		return sample;
	}

	// Renders the model at a local time and returns the rendered x, failing if it is dormant.
	float RenderedX(cAvatarPoseModel& aModel, double afLocalTimeMs, const std::string& asDescription)
	{
		cAvatarRenderedPose rendered;
		Expect(aModel.Sample(afLocalTimeMs, kMap, rendered), asDescription + " (awake)");
		return rendered.mfX;
	}

	cAvatarPoseSample LanternPose(double afSenderTimeMs, float afX, bool abLanternRaised,
		unsigned int alTeleportCounter = 0)
	{
		cAvatarPoseSample sample = Pose(afSenderTimeMs, afX, 0.0f, alTeleportCounter);
		sample.mbLanternRaised = abLanternRaised;
		return sample;
	}

	// Renders the model at a local time and returns whether the lantern is raised, failing if it is dormant.
	bool RenderedLantern(cAvatarPoseModel& aModel, double afLocalTimeMs, const std::string& asDescription)
	{
		cAvatarRenderedPose rendered;
		Expect(aModel.Sample(afLocalTimeMs, kMap, rendered), asDescription + " (awake)");
		return rendered.mbLanternRaised;
	}
}

int main()
{
	{
		cAvatarPoseModel model;
		model.AddPose(Pose(5000.0, 3.0f, 45.0f), 1000.0);
		cAvatarRenderedPose rendered;
		Expect(model.Sample(1000.0, kMap, rendered), "a single Pose for the current map wakes the Avatar");
		Expect(Near(rendered.mfX, 3.0f) && Near(rendered.mfY, 1.0f) && Near(rendered.mfZ, -2.0f) &&
			Near(rendered.mfYawDegrees, 45.0f), "a single Pose is rendered as sent");
	}

	{
		// The sender's clock runs 4000 ms ahead of the receiver's, and each Pose arrives at once.
		cAvatarPoseModel model;
		model.AddPose(Pose(5000.0, 0.0f), 1000.0);
		model.AddPose(Pose(5100.0, 2.0f), 1100.0);
		Expect(Near(RenderedX(model, 1100.0, "renders 100 ms behind"), 0.0f),
			"renders 100 ms behind the newest sample, in the sender's time");
		Expect(Near(RenderedX(model, 1150.0, "interpolates"), 1.0f),
			"interpolates position linearly between the samples around the render time");
		Expect(Near(RenderedX(model, 1175.0, "interpolates"), 1.5f),
			"follows the samples as local time passes");
	}

	{
		cAvatarPoseModel model;
		model.AddPose(Pose(5000.0, 0.0f), 1000.0);
		model.AddPose(Pose(5100.0, 2.0f), 1100.0);
		Expect(Near(RenderedX(model, 1300.0, "holds"), 2.0f),
			"holds the last Pose when the buffer runs dry instead of extrapolating");
		Expect(Near(RenderedX(model, 60000.0, "holds"), 2.0f), "keeps holding for as long as no Pose arrives");
	}

	{
		cAvatarPoseModel model;
		model.AddPose(Pose(5000.0, 0.0f, 170.0f), 1000.0);
		model.AddPose(Pose(5100.0, 0.0f, -170.0f), 1100.0);
		cAvatarRenderedPose rendered;
		model.Sample(1125.0, kMap, rendered);
		Expect(Near(rendered.mfYawDegrees, 175.0f) || Near(rendered.mfYawDegrees, -185.0f),
			"yaw turns along the shortest arc across 180 degrees");
		model.Sample(1175.0, kMap, rendered);
		Expect(Near(rendered.mfYawDegrees, -175.0f) || Near(rendered.mfYawDegrees, 185.0f),
			"yaw keeps turning the short way past 180 degrees");

		model.AddPose(Pose(5200.0, 0.0f, 170.0f), 1200.0);
		model.Sample(1225.0, kMap, rendered);
		Expect(Near(rendered.mfYawDegrees, -175.0f) || Near(rendered.mfYawDegrees, 185.0f),
			"yaw turns back the short way across -180 degrees");
	}

	{
		cAvatarPoseModel model;
		model.AddPose(Pose(5000.0, 0.0f, 0.0f, 7), 1000.0);
		model.AddPose(Pose(5100.0, 1.0f, 0.0f, 8), 1100.0);
		Expect(Near(RenderedX(model, 1199.0, "teleport"), 0.0f),
			"a changed teleport counter keeps the earlier Pose instead of gliding");
		Expect(Near(RenderedX(model, 1200.0, "teleport"), 1.0f), "and snaps to the teleported Pose on its time");
	}

	{
		cAvatarPoseModel model;
		model.AddPose(Pose(5000.0, 0.0f), 1000.0);
		model.AddPose(Pose(5100.0, 3.1f), 1100.0);
		Expect(Near(RenderedX(model, 1150.0, "distance snap"), 0.0f),
			"Poses more than 3 m apart are not interpolated");
		Expect(Near(RenderedX(model, 1200.0, "distance snap"), 3.1f), "and snap to the distant Pose on its time");

		model.AddPose(Pose(5200.0, 6.0f), 1200.0);
		Expect(Near(RenderedX(model, 1250.0, "within snap distance"), 4.55f),
			"Poses up to 3 m apart are interpolated");
	}

	{
		cAvatarPoseModel model;
		model.AddPose(Pose(5000.0, 0.0f), 1000.0);
		model.AddPose(Pose(5100.0, 2.0f), 1100.0);
		model.AddPose(Pose(5050.0, 1.9f), 1110.0);
		model.AddPose(Pose(5100.0, 1.8f), 1120.0);
		Expect(Near(RenderedX(model, 1150.0, "out of order"), 1.0f),
			"a Pose older than the newest one is discarded");
		Expect(Near(RenderedX(model, 1300.0, "duplicate"), 2.0f),
			"a Pose as old as the newest one is discarded");
	}

	{
		const std::string sOtherMap = "maps/02_old archives: west.map";
		cAvatarPoseModel model;
		cAvatarRenderedPose rendered;
		Expect(!model.Sample(1000.0, kMap, rendered), "an Avatar without a Pose is dormant");

		model.AddPose(Pose(5000.0, 0.0f, 0.0f, 0, sOtherMap), 1000.0);
		Expect(!model.Sample(1000.0, kMap, rendered), "an Avatar whose latest Pose names another map is dormant");
		model.AddPose(Pose(5100.0, 1.0f, 0.0f, 0, sOtherMap), 1100.0);
		Expect(!model.Sample(1100.0, "", rendered), "an Avatar is dormant while no map is loaded");
		Expect(!model.Sample(1150.0, sOtherMap, rendered),
			"a Dormant Avatar's buffer is cleared, so it stays dormant until a new Pose arrives");

		model.AddPose(Pose(5200.0, 2.0f, 0.0f, 0, sOtherMap), 1200.0);
		Expect(model.Sample(1200.0, sOtherMap, rendered) && Near(rendered.mfX, 2.0f),
			"a Dormant Avatar wakes on the first Pose for the local map, at that Pose");

		model.AddPose(Pose(5300.0, 3.0f), 1300.0);
		Expect(model.Sample(1300.0, kMap, rendered) && Near(rendered.mfX, 3.0f),
			"a map change in the Pose stream clears the buffer instead of interpolating across maps");
		Expect(!model.Sample(1310.0, sOtherMap, rendered), "the Avatar is dormant once the local map differs");
	}

	{
		cAvatarPoseModel model;
		model.AddPose(Pose(5000.0, 0.0f), 1050.0);
		model.AddPose(Pose(5100.0, 2.0f), 1100.0);
		Expect(Near(RenderedX(model, 1150.0, "jitter"), 1.0f),
			"the sender's clock is aligned by the least delayed Pose, so a late one does not skew it");
	}

	{
		// The sender's clock stood still for two seconds, as it does while its game loads.
		cAvatarPoseModel model;
		model.AddPose(Pose(5000.0, 0.0f), 1000.0);
		model.AddPose(Pose(5100.0, 2.0f), 1100.0);
		model.AddPose(Pose(5200.0, 2.5f), 3100.0);
		model.AddPose(Pose(5300.0, 3.0f), 3200.0);
		Expect(Near(RenderedX(model, 3250.0, "sender stall"), 2.75f),
			"the sender's clock is re-aligned when it falls far behind, instead of holding forever");
	}

	{
		// The sender's clock started over, as when its game restarts or a recording is replayed.
		cAvatarPoseModel model;
		model.AddPose(Pose(5000.0, 0.0f), 1000.0);
		model.AddPose(Pose(5100.0, 2.0f), 1100.0);
		model.AddPose(Pose(2000.0, 1.0f), 1200.0);
		model.AddPose(Pose(2100.0, 1.5f), 1300.0);
		Expect(Near(RenderedX(model, 1350.0, "sender restart"), 1.25f),
			"a Pose far older than the newest starts the stream over instead of being discarded");
	}

	{
		// Poses at 20 Hz walking 0.1 m each, whose latency rises by 200 ms and stays there.
		cAvatarPoseModel model;
		model.AddPose(Pose(5000.0, 0.0f), 1000.0);
		for (int index = 1; index <= 200; ++index)
			model.AddPose(Pose(5000.0 + index * 50.0, index * 0.1f), 1200.0 + index * 50.0);
		Expect(Near(RenderedX(model, 11225.0, "latency rise"), 19.85f),
			"a lasting rise in latency is adopted, so the Avatar is again rendered 100 ms behind");
	}

	{
		cAvatarPoseModel model;
		model.AddPose(LanternPose(5000.0, 0.0f, false), 1000.0);
		model.AddPose(LanternPose(5100.0, 1.0f, true), 1100.0);
		model.AddPose(LanternPose(5200.0, 2.0f, false), 1200.0);
		Expect(!RenderedLantern(model, 1100.0, "lantern at a sample"), "the lantern is rendered as the Pose at render time");
		Expect(!RenderedLantern(model, 1190.0, "lantern while interpolating"),
			"while interpolating, the lantern comes from the older sample, even when the newer one is nearer");
		Expect(RenderedLantern(model, 1200.0, "lantern switch"), "the lantern switches on the sender's time");
		Expect(RenderedLantern(model, 1290.0, "lantern while interpolating"),
			"a raised lantern stays raised until the render time reaches the Pose that lowers it");
		Expect(!RenderedLantern(model, 1300.0, "lantern switch"), "the lantern is lowered on the sender's time");
	}

	{
		cAvatarPoseModel model;
		model.AddPose(LanternPose(5000.0, 0.0f, false), 1000.0);
		model.AddPose(LanternPose(5100.0, 2.0f, true), 1100.0);
		Expect(RenderedLantern(model, 1300.0, "lantern hold"), "the lantern holds with the last Pose when samples stop");
		Expect(RenderedLantern(model, 60000.0, "lantern hold"), "and keeps holding for as long as no Pose arrives");
	}

	{
		cAvatarPoseModel model;
		model.AddPose(LanternPose(5000.0, 0.0f, true, 7), 1000.0);
		model.AddPose(LanternPose(5100.0, 1.0f, false, 8), 1100.0);
		Expect(RenderedLantern(model, 1199.0, "lantern before a teleport"),
			"the lantern is held with the Pose before a teleport");
		Expect(!RenderedLantern(model, 1200.0, "lantern at a teleport"), "and snaps with the teleported Pose");

		model.AddPose(LanternPose(5200.0, 10.0f, true, 8), 1200.0);
		Expect(!RenderedLantern(model, 1250.0, "lantern before a distance snap"),
			"the lantern is held with the Pose before a distance snap");
		Expect(RenderedLantern(model, 1300.0, "lantern at a distance snap"), "and snaps with the distant Pose");
	}

	std::cout << "Avatar Pose model cases passed\n";
	return 0;
}
