#include "MovementDetector.h"

#include <QDateTime>

#include <algorithm>
#include <cmath>

MovementDetector::MovementDetector(const MovementDetectorConfig& config, QObject* parent)
    : QObject(parent), _cfg(config) {}

QString MovementDetector::stateName() const {
    switch (_state) {
    case State::Still: return QStringLiteral("still");
    case State::Moving: return QStringLiteral("moving");
    default: return QStringLiteral("unknown");
    }
}

void MovementDetector::reset() {
    _samples.clear();
    _levels.clear();
    _frick = 0.0;
    _hasFrick = false;
    _lastSampleMs = -1;
    if (_state != State::Unknown) {
        _state = State::Unknown;
        emit stateChanged(_state);
    }
    _pending = State::Unknown;
    _pendingSince = 0;
}

void MovementDetector::addSample(double rssiDbm, qint64 nowMs) {
    if (nowMs < 0) nowMs = QDateTime::currentMSecsSinceEpoch();
    _samples.emplace_back(nowMs, rssiDbm);
    _lastSampleMs = nowMs;
    _levels.emplace_back(nowMs, smoothedLevel(nowMs));
    evaluate(nowMs);
}

// Mean of the raw samples inside the smoothing window: the current "level"
// of the signal. Always non-empty here because the sample just added is
// inside the window.
double MovementDetector::smoothedLevel(qint64 nowMs) const {
    double sum = 0.0;
    int n = 0;
    for (auto it = _samples.rbegin(); it != _samples.rend(); ++it) {
        if (nowMs - it->first > _cfg.smoothMs) break;
        sum += it->second;
        ++n;
    }
    return n > 0 ? sum / n : 0.0;
}

// True when the smoothed level has been sitting within +/- bandDb of itself
// over the settle window: the signal found a new stable home.
bool MovementDetector::levelsSettled(qint64 nowMs) const {
    if (_levels.size() < 2) return false;
    double mn = _levels.front().second, mx = mn;
    for (const auto& p : _levels) {
        mn = std::min(mn, p.second);
        mx = std::max(mx, p.second);
    }
    return (mx - mn) <= 2.0 * _cfg.bandDb;
}

void MovementDetector::evaluate(qint64 nowMs) {
    // Raw samples are retained for the full data window so a brief radio
    // fade never starves the minSamples gate; the excursion test and the
    // level smoothing only ever look at the recent smoothing window.
    while (!_samples.empty() && nowMs - _samples.front().first > _cfg.dataWindowMs)
        _samples.pop_front();
    while (!_levels.empty() && nowMs - _levels.front().first > _cfg.settleMs)
        _levels.pop_front();

    State candidate = State::Unknown;
    const bool fresh = _lastSampleMs >= 0 && nowMs - _lastSampleMs <= _cfg.staleMs;
    if (fresh && static_cast<int>(_samples.size()) >= _cfg.minSamples) {
        if (_state == State::Still && _hasFrick) {
            // Excursion test on the raw samples inside the smoothing window:
            // anything beyond frick +/- band means the headphones moved.
            // Single spikes are absorbed by the moving-confirm hold below.
            bool excursion = false;
            for (auto it = _samples.rbegin(); it != _samples.rend(); ++it) {
                if (nowMs - it->first > _cfg.smoothMs) break;
                if (std::fabs(it->second - _frick) > _cfg.bandDb) {
                    excursion = true;
                    break;
                }
            }
            candidate = excursion ? State::Moving : State::Still;
        } else {
            // No baseline yet, or currently moving: only a settled signal
            // earns the Still verdict (and re-captures the baseline).
            candidate = levelsSettled(nowMs)
                            ? State::Still
                            : (_state == State::Moving ? State::Moving : State::Unknown);
        }
    }

    if (candidate != _pending) {
        _pending = candidate;
        _pendingSince = nowMs;
    }
    const int confirmMs = candidate == State::Moving   ? _cfg.movingConfirmMs
                          : candidate == State::Still ? _cfg.stillConfirmMs
                                                      : 0;
    if (candidate != _state && nowMs - _pendingSince >= confirmMs) {
        _state = candidate;
        if (_state == State::Still) {
            // Re-anchor: the baseline follows the device to its new place.
            _frick = smoothedLevel(nowMs);
            _hasFrick = true;
        } else if (_state == State::Unknown) {
            _hasFrick = false;
        }
        emit stateChanged(_state);
    }
}
