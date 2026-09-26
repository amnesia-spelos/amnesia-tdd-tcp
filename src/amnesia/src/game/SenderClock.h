#ifndef SENDER_CLOCK_H
#define SENDER_CLOCK_H

// Aligns a Peer's game clock with the local one, so that its samples can be played back a fixed
// delay behind the newest (ADR 0003).
class cSenderClock
{
public:
	// A sample whose clock disagrees with the alignment by more than this starts a new alignment.
	static const double kResyncMs;
	// How much of the latency above the alignment each sample adopts.
	static const double kDriftRate;

	cSenderClock() : mfOffsetMs(0.0) {}

	// Local time minus sender time.
	double GetOffsetMs() const { return mfOffsetMs; }

	// The least delayed sample is the closest to the sender's clock; later ones only add latency, and
	// the alignment drifts towards them slowly, so that a lasting rise in latency is adopted. A sample
	// far later than that means the sender's clock stood still, so it is aligned anew, as is the first
	// sample after abStartOver.
	void Align(double afSenderTimeMs, double afLocalTimeMs, bool abStartOver)
	{
		const double fOffsetMs = afLocalTimeMs - afSenderTimeMs;
		if (abStartOver || fOffsetMs < mfOffsetMs || fOffsetMs - mfOffsetMs > kResyncMs)
			mfOffsetMs = fOffsetMs;
		else
			mfOffsetMs += (fOffsetMs - mfOffsetMs) * kDriftRate;
	}

private:
	double mfOffsetMs;
};

// Header-only, so every model that plays a Peer's samples back shares one definition.
__declspec(selectany) const double cSenderClock::kResyncMs = 1000.0;
__declspec(selectany) const double cSenderClock::kDriftRate = 0.05;

#endif // SENDER_CLOCK_H
