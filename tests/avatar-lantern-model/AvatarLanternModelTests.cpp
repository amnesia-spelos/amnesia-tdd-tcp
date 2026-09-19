#include "AvatarLanternModel.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
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
}

int main()
{
	{
		cAvatarLanternModel model;
		Expect(Near(model.Update(true, false, 0.1f), 0.0f), "a lowered lantern gives no light");
		Expect(Near(model.Update(true, true, 0.25f), 0.25f), "a raised lantern fades in like the local one");
		Expect(Near(model.Update(true, true, 0.5f), 0.75f), "and keeps fading in while raised");
		Expect(Near(model.Update(true, true, 0.5f), 1.0f), "reaching full brightness after about a second");
		Expect(Near(model.Update(true, true, 10.0f), 1.0f), "and never beyond it");
	}

	{
		cAvatarLanternModel model;
		model.Update(true, true, 1.0f);
		Expect(Near(model.Update(true, false, 0.1f), 0.8f), "a lowered lantern fades out twice as fast");
		Expect(Near(model.Update(true, false, 0.3f), 0.2f), "and keeps fading out while lowered");
		Expect(Near(model.Update(true, false, 0.3f), 0.0f), "going dark after about half a second");
		Expect(Near(model.Update(true, false, 10.0f), 0.0f), "and never below it");
		Expect(Near(model.Update(true, true, 0.5f), 0.5f), "raising it again fades in from dark");
	}

	{
		cAvatarLanternModel model;
		model.Update(true, true, 1.0f);
		Expect(Near(model.Update(false, true, 0.1f), 0.0f), "a Dormant Avatar gives no light, even with the lantern raised");
		Expect(Near(model.Update(false, true, 5.0f), 0.0f), "and stays dark for as long as it is dormant");
		Expect(Near(model.Update(true, true, 0.25f), 0.25f), "a woken Avatar fades its lantern in from dark");
	}

	std::cout << "Avatar lantern model cases passed\n";
	return 0;
}
