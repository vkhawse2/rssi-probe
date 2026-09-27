#pragma once

#include <QObject>

#include <deque>
#include <utility>

// MovementDetector turns a stream of Bluetooth RSSI samples into a
// Still/Moving verdict using a re-anchoring baseline ("frick").
//
// The idea: when the headphones sit still somewhere, the signal lives at a
// certain level. That level is captured as the baseline the moment the
// detector declares Still (e.g. when ANC turns on). While new samples stay
// within +/- bandDb of the baseline, the device is still. When a sample
// jumps outside the band, the device is moving. While moving, the baseline
// stays frozen; once the signal settles again (levels within +/- bandDb of
// each other), the device is still in a *new* place, the baseline is
// re-captured there, and the loop continues.
//
// RSSI is noisy sample-to-sample (seated-still captures swing ~+/-7 dB), so
// the excursion test runs on raw samples but must hold for movingConfirmMs
// before it flips the verdict: a single spike never triggers it. The still
// verdict needs the smoothed level to sit inside the band for settleMs plus
// stillConfirmMs. Timestamps are injectable so the logic is unit-testable.
struct MovementDetectorConfig {
    double bandDb = 15.0;      // +/- dB around the baseline: inside -> still
    int smoothMs = 2000;       // excursion test + level smoothing look at this window
    int settleMs = 4000;       // smoothed-level history window for the settle check
    int dataWindowMs = 12000;  // raw sample retention: the minSamples gate looks this far back
    int minSamples = 4;        // samples needed inside the data window before any verdict
    int movingConfirmMs = 3000; // excursion must hold this long -> Moving
    int stillConfirmMs = 5000;  // settled candidate must hold this long -> Still
    int staleMs = 20000;       // no samples for this long -> Unknown
};

class MovementDetector : public QObject {
    Q_OBJECT
public:
    enum class State { Unknown, Still, Moving };
    Q_ENUM(State)

    explicit MovementDetector(const MovementDetectorConfig& config = MovementDetectorConfig{},
                              QObject* parent = nullptr);

    // nowMs < 0 uses the current time. Tests pass explicit times.
    void addSample(double rssiDbm, qint64 nowMs = -1);
    State state() const { return _state; }
    QString stateName() const;
    void reset();

signals:
    void stateChanged(MovementDetector::State state);

private:
    void evaluate(qint64 nowMs);
    double smoothedLevel(qint64 nowMs) const;
    bool levelsSettled(qint64 nowMs) const;

    MovementDetectorConfig _cfg;
    std::deque<std::pair<qint64, double>> _samples; // (timestampMs, rssiDbm)
    std::deque<std::pair<qint64, double>> _levels;  // (timestampMs, smoothed dBm)
    double _frick = 0.0;  // baseline dBm, captured at each Still verdict
    bool _hasFrick = false;
    State _state = State::Unknown;
    State _pending = State::Unknown;
    qint64 _pendingSince = 0;
    qint64 _lastSampleMs = -1;
};
