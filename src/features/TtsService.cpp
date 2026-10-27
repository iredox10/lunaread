#include "TtsService.h"
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace LunaRead {

TtsService::TtsService(QObject* parent) : QObject(parent) {
    m_sentenceTimer = new QTimer(this);
    m_sentenceTimer->setSingleShot(true);
    connect(m_sentenceTimer, &QTimer::timeout, this, &TtsService::nextSentence);

    m_speechProcess = new QProcess(this);
    connect(m_speechProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int /*exitCode*/, QProcess::ExitStatus status) {
        if (m_isPlaying && status == QProcess::NormalExit) {
            // Short natural breath pause before next sentence
            QTimer::singleShot(120, this, &TtsService::nextSentence);
        }
    });

    m_sleepCountdownTimer = new QTimer(this);
    m_sleepCountdownTimer->setInterval(1000);
    connect(m_sleepCountdownTimer, &QTimer::timeout, this, &TtsService::onSleepCountdownTick);

    detectBackend();
    detectVoices();
}

TtsService::~TtsService() {
    if (m_sleepCountdownTimer) {
        m_sleepCountdownTimer->stop();
    }
    stopCurrentSpeech();
}

void TtsService::detectBackend() {
    m_speechBackend = QStandardPaths::findExecutable(QStringLiteral("spd-say"));
    if (m_speechBackend.isEmpty()) {
        m_speechBackend = QStandardPaths::findExecutable(QStringLiteral("espeak-ng"));
    }
    if (m_speechBackend.isEmpty()) {
        m_speechBackend = QStandardPaths::findExecutable(QStringLiteral("espeak"));
    }
}

void TtsService::detectVoices() {
    m_availableVoices.clear();

    // Standard high-quality presets
    m_availableVoices.append({QStringLiteral("default"), tr("Default"), QStringLiteral("en")});
    m_availableVoices.append({QStringLiteral("female"), tr("Female Voice"), QStringLiteral("en")});
    m_availableVoices.append({QStringLiteral("male"), tr("Male Voice"), QStringLiteral("en")});
    m_availableVoices.append({QStringLiteral("en-uk"), tr("English UK"), QStringLiteral("en-GB")});
    m_availableVoices.append({QStringLiteral("en-us"), tr("English US"), QStringLiteral("en-US")});

    // Detect additional system voices from espeak-ng / espeak if present
    if (!m_speechBackend.isEmpty() && m_speechBackend.contains(QStringLiteral("espeak"))) {
        QProcess proc;
        proc.start(m_speechBackend, QStringList() << QStringLiteral("--voices=en"));
        if (proc.waitForFinished(1000)) {
            const QString output = QString::fromUtf8(proc.readAllStandardOutput());
            const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (int i = 1; i < lines.size(); ++i) {
                const QString line = lines[i].trimmed();
                const QStringList parts = line.split(QRegularExpression(R"(\s+)"));
                if (parts.size() >= 4) {
                    const QString lang = parts[1];
                    const QString voiceName = parts[3];
                    if (!voiceName.isEmpty() &&
                        voiceName != QStringLiteral("English_(America)") &&
                        voiceName != QStringLiteral("English_(Great_Britain)")) {
                        QString cleanName = voiceName;
                        cleanName.replace(QLatin1Char('_'), QLatin1Char(' '));
                        if (m_availableVoices.size() < 12) {
                            m_availableVoices.append({voiceName, cleanName, lang});
                        }
                    }
                }
            }
        }
    }
}

void TtsService::loadText(const QString& text) {
    const bool wasPlaying = m_isPlaying;
    stop();

    m_sentences.clear();
    m_currentIndex = 0;
    m_chapterEndSentenceIndex = -1;

    // Split on sentence punctuation: . ! ? 。 ！ ？ \n
    QRegularExpression re(R"([^\.\!\?。\！？\n]+[\.\!\?。\！？\n]*)");
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        QString s = it.next().captured(0).trimmed();
        if (!s.isEmpty()) {
            m_sentences.append(s);
        }
    }

    emit sentencesLoaded(static_cast<int>(m_sentences.size()));

    if (wasPlaying && !m_sentences.isEmpty()) {
        play();
    }
}

void TtsService::play() {
    if (m_sentences.isEmpty()) return;
    m_isPlaying = true;

    emit playbackStateChanged(true);
    emit activeSentenceChanged(m_currentIndex, m_sentences[m_currentIndex]);
    speakCurrentSentence();
}

void TtsService::pause() {
    if (!m_isPlaying) return;
    m_isPlaying = false;
    stopCurrentSpeech();
    emit playbackStateChanged(false);
}

void TtsService::stop() {
    const bool hadPlaying = m_isPlaying;
    m_isPlaying = false;
    stopCurrentSpeech();
    m_currentIndex = 0;
    if (hadPlaying) {
        emit playbackStateChanged(false);
    }
}

void TtsService::nextSentence() {
    if (!m_isPlaying) return;

    // Check if EndOfChapter boundary reached
    if (m_sleepMode == SleepTimerMode::EndOfChapter &&
        m_chapterEndSentenceIndex >= 0 &&
        m_currentIndex >= m_chapterEndSentenceIndex) {
        m_sleepMode = SleepTimerMode::Off;
        stop();
        emit sleepTimerExpired();
        emit sleepTimerModeChanged(SleepTimerMode::Off);
        return;
    }

    if (m_currentIndex + 1 < m_sentences.size()) {
        m_currentIndex++;
        emit activeSentenceChanged(m_currentIndex, m_sentences[m_currentIndex]);
        speakCurrentSentence();
    } else {
        const bool wasEndOfChapter = (m_sleepMode == SleepTimerMode::EndOfChapter);
        if (wasEndOfChapter) {
            m_sleepMode = SleepTimerMode::Off;
            emit sleepTimerModeChanged(SleepTimerMode::Off);
        }
        stop();
        if (wasEndOfChapter) {
            emit sleepTimerExpired();
        }
        emit playbackFinished();
    }
}

void TtsService::prevSentence() {
    if (m_currentIndex > 0) {
        m_currentIndex--;
        emit activeSentenceChanged(m_currentIndex, m_sentences[m_currentIndex]);
        if (m_isPlaying) {
            speakCurrentSentence();
        }
    }
}

void TtsService::seekSentence(int index) {
    if (m_sentences.isEmpty()) return;
    const int target = std::clamp(index, 0, static_cast<int>(m_sentences.size() - 1));
    if (target != m_currentIndex) {
        m_currentIndex = target;
        emit activeSentenceChanged(m_currentIndex, m_sentences[m_currentIndex]);
        if (m_isPlaying) {
            speakCurrentSentence();
        }
    }
}

void TtsService::setPlaybackRate(double rate) {
    rate = std::clamp(rate, 0.5, 3.0);
    if (std::abs(m_playbackRate - rate) < 0.001) return;
    m_playbackRate = rate;
    emit playbackRateChanged(m_playbackRate);
    if (m_isPlaying) {
        speakCurrentSentence();
    }
}

void TtsService::setPitch(int pitch) {
    pitch = std::clamp(pitch, -50, 50);
    if (m_pitch == pitch) return;
    m_pitch = pitch;
    emit pitchChanged(m_pitch);
    if (m_isPlaying) {
        speakCurrentSentence();
    }
}

void TtsService::setVoice(const QString& voiceId) {
    if (m_currentVoice == voiceId) return;
    m_currentVoice = voiceId;
    emit voiceChanged(m_currentVoice);
    if (m_isPlaying) {
        speakCurrentSentence();
    }
}

void TtsService::setSleepTimer(SleepTimerMode mode) {
    m_sleepMode = mode;

    switch (mode) {
    case SleepTimerMode::Off:
        m_sleepCountdownTimer->stop();
        m_sleepRemainingSeconds = 0;
        break;
    case SleepTimerMode::Minutes_5:
        m_sleepRemainingSeconds = 5 * 60;
        m_sleepCountdownTimer->start();
        break;
    case SleepTimerMode::Minutes_10:
        m_sleepRemainingSeconds = 10 * 60;
        m_sleepCountdownTimer->start();
        break;
    case SleepTimerMode::Minutes_15:
        m_sleepRemainingSeconds = 15 * 60;
        m_sleepCountdownTimer->start();
        break;
    case SleepTimerMode::Minutes_30:
        m_sleepRemainingSeconds = 30 * 60;
        m_sleepCountdownTimer->start();
        break;
    case SleepTimerMode::Minutes_45:
        m_sleepRemainingSeconds = 45 * 60;
        m_sleepCountdownTimer->start();
        break;
    case SleepTimerMode::Minutes_60:
        m_sleepRemainingSeconds = 60 * 60;
        m_sleepCountdownTimer->start();
        break;
    case SleepTimerMode::EndOfChapter:
        m_sleepCountdownTimer->stop();
        m_sleepRemainingSeconds = 0;
        break;
    }

    emit sleepTimerModeChanged(mode);
    emit sleepTimerTick(m_sleepRemainingSeconds);
}

void TtsService::onSleepCountdownTick() {
    if (m_sleepRemainingSeconds > 0) {
        --m_sleepRemainingSeconds;
        emit sleepTimerTick(m_sleepRemainingSeconds);
        if (m_sleepRemainingSeconds <= 0) {
            m_sleepCountdownTimer->stop();
            m_sleepMode = SleepTimerMode::Off;
            stop();
            emit sleepTimerExpired();
            emit sleepTimerModeChanged(SleepTimerMode::Off);
        }
    }
}

void TtsService::notifyChapterFinished() {
    if (m_sleepMode == SleepTimerMode::EndOfChapter) {
        m_sleepMode = SleepTimerMode::Off;
        stop();
        emit sleepTimerExpired();
        emit sleepTimerModeChanged(SleepTimerMode::Off);
    }
}

void TtsService::stopCurrentSpeech() {
    m_sentenceTimer->stop();
    if (m_speechProcess && m_speechProcess->state() != QProcess::NotRunning) {
        m_speechProcess->terminate();
        if (!m_speechProcess->waitForFinished(100)) {
            m_speechProcess->kill();
        }
    }

    // Cancel queued spd-say playback
    if (m_speechBackend.contains(QStringLiteral("spd-say"))) {
        QProcess::execute(m_speechBackend, QStringList() << QStringLiteral("-S"));
    }
}

void TtsService::speakCurrentSentence() {
    stopCurrentSpeech();

    if (!m_isPlaying || m_currentIndex < 0 || m_currentIndex >= m_sentences.size()) {
        return;
    }

    const QString sentence = m_sentences[m_currentIndex].trimmed();
    if (sentence.isEmpty()) {
        nextSentence();
        return;
    }

    if (!m_speechBackend.isEmpty()) {
        QStringList args;
        if (m_speechBackend.contains(QStringLiteral("spd-say"))) {
            // Rate range in spd-say is -100 to 100 (0 is normal)
            int rateVal = std::clamp(static_cast<int>((m_playbackRate - 1.0) * 100.0), -100, 100);
            args << QStringLiteral("-r") << QString::number(rateVal);

            // Pitch range in spd-say is -100 to 100 (0 is normal)
            int pitchVal = std::clamp(m_pitch * 2, -100, 100);
            args << QStringLiteral("-p") << QString::number(pitchVal);

            // Voice presets and selection
            if (m_currentVoice == QStringLiteral("female")) {
                args << QStringLiteral("-t") << QStringLiteral("female1");
            } else if (m_currentVoice == QStringLiteral("male")) {
                args << QStringLiteral("-t") << QStringLiteral("male1");
            } else if (m_currentVoice == QStringLiteral("en-uk")) {
                args << QStringLiteral("-l") << QStringLiteral("en-GB");
            } else if (m_currentVoice == QStringLiteral("en-us")) {
                args << QStringLiteral("-l") << QStringLiteral("en-US");
            } else if (m_currentVoice != QStringLiteral("default")) {
                args << QStringLiteral("-y") << m_currentVoice;
            }

            args << QStringLiteral("-w") // Wait for speech to finish before returning
                 << sentence;
        } else if (m_speechBackend.contains(QStringLiteral("espeak"))) {
            // Speed in words per minute (normal ~175)
            int wpm = std::clamp(static_cast<int>(175.0 * m_playbackRate), 80, 525);
            args << QStringLiteral("-s") << QString::number(wpm);

            // Pitch in espeak is 0 to 99 (normal 50)
            int pitchVal = std::clamp(50 + m_pitch, 0, 99);
            args << QStringLiteral("-p") << QString::number(pitchVal);

            // Voice selection
            if (m_currentVoice == QStringLiteral("female")) {
                args << QStringLiteral("-v") << QStringLiteral("en+f2");
            } else if (m_currentVoice == QStringLiteral("male")) {
                args << QStringLiteral("-v") << QStringLiteral("en+m2");
            } else if (m_currentVoice == QStringLiteral("en-uk")) {
                args << QStringLiteral("-v") << QStringLiteral("en-gb");
            } else if (m_currentVoice == QStringLiteral("en-us")) {
                args << QStringLiteral("-v") << QStringLiteral("en-us");
            } else if (m_currentVoice != QStringLiteral("default")) {
                args << QStringLiteral("-v") << m_currentVoice;
            }

            args << sentence;
        }

        m_speechProcess->start(m_speechBackend, args);
    } else {
        // Fallback: estimate sentence duration based on word count
        int wordCount = static_cast<int>(sentence.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts).size());
        int estimatedMs = static_cast<int>((std::max(1, wordCount) * 350.0) / std::max(0.1, m_playbackRate));
        m_sentenceTimer->start(estimatedMs);
    }
}

} // namespace LunaRead
