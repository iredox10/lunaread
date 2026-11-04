#include "TtsService.h"
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
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
    const QString home = QDir::homePath();

    // 1. Detect Piper Neural TTS (highest studio quality, local & offline)
    m_piperExecutable = QStandardPaths::findExecutable(QStringLiteral("piper"));
    if (m_piperExecutable.isEmpty()) {
        const QString localBin = home + QStringLiteral("/.local/bin/piper");
        if (QFile::exists(localBin)) {
            m_piperExecutable = localBin;
        }
    }

    const QString piperVoices = home + QStringLiteral("/.local/share/piper/voices");
    if (QDir(piperVoices).exists()) {
        m_piperVoicesDir = piperVoices;
    }

    // 2. Detect Edge-TTS (Azure Neural Speech)
    QString edgeTts = QStandardPaths::findExecutable(QStringLiteral("edge-tts"));
    if (edgeTts.isEmpty() && QFile::exists(home + QStringLiteral("/.local/bin/edge-tts"))) {
        edgeTts = home + QStringLiteral("/.local/bin/edge-tts");
    }
    m_hasEdgeTts = !edgeTts.isEmpty();

    // 3. Fallback system synthesizers
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

    // 1. Piper Neural studio models (Local & Offline)
    if (!m_piperExecutable.isEmpty() && !m_piperVoicesDir.isEmpty()) {
        const QString lessacModel = m_piperVoicesDir + QStringLiteral("/en_US-lessac-medium.onnx");
        const QString amyModel = m_piperVoicesDir + QStringLiteral("/en_US-amy-medium.onnx");

        if (QFile::exists(lessacModel)) {
            m_availableVoices.append({QStringLiteral("piper:lessac"), tr("Christopher (Studio Male • Piper Neural)"), QStringLiteral("en-US")});
        }
        if (QFile::exists(amyModel)) {
            m_availableVoices.append({QStringLiteral("piper:amy"), tr("Amy (Studio Female • Piper Neural)"), QStringLiteral("en-US")});
        }
    }

    // 2. Microsoft Azure Edge Neural voices
    if (m_hasEdgeTts) {
        m_availableVoices.append({QStringLiteral("edge:en-US-JennyNeural"), tr("Jenny (Natural Female • Azure Neural)"), QStringLiteral("en-US")});
        m_availableVoices.append({QStringLiteral("edge:en-US-GuyNeural"), tr("Guy (Natural Male • Azure Neural)"), QStringLiteral("en-US")});
        m_availableVoices.append({QStringLiteral("edge:en-GB-SoniaNeural"), tr("Sonia (British Female • Azure Neural)"), QStringLiteral("en-GB")});
        m_availableVoices.append({QStringLiteral("edge:en-US-AriaNeural"), tr("Aria (Expressive Female • Azure Neural)"), QStringLiteral("en-US")});
    }

    // 3. System voice fallbacks if no neural engines available
    if (m_availableVoices.isEmpty()) {
        m_availableVoices.append({QStringLiteral("default"), tr("Default System Voice"), QStringLiteral("en")});
        m_availableVoices.append({QStringLiteral("female"), tr("Female Voice"), QStringLiteral("en")});
        m_availableVoices.append({QStringLiteral("male"), tr("Male Voice"), QStringLiteral("en")});
    }

    // Set default voice to first high quality neural voice if current is default or not found
    if (!m_availableVoices.isEmpty()) {
        bool currentFound = false;
        for (const auto& v : m_availableVoices) {
            if (v.id == m_currentVoice) {
                currentFound = true;
                break;
            }
        }
        if (!currentFound) {
            m_currentVoice = m_availableVoices.first().id;
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
        if (!m_speechProcess->waitForFinished(50)) {
            m_speechProcess->kill();
        }
    }

    // Immediately terminate any background streaming audio processes
    QProcess::execute(QStringLiteral("pkill"), QStringList() << QStringLiteral("-f") << QStringLiteral("aplay -r 22050"));
    QProcess::execute(QStringLiteral("pkill"), QStringList() << QStringLiteral("-f") << QStringLiteral("mpv .*lunaread_tts.mp3"));

    // Cancel queued spd-say playback
    if (!m_speechBackend.isEmpty() && m_speechBackend.contains(QStringLiteral("spd-say"))) {
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

    // 1. Piper Neural Studio Speech (Local & Offline)
    if (m_currentVoice.startsWith(QLatin1String("piper:")) && !m_piperExecutable.isEmpty()) {
        const QString voiceName = m_currentVoice.mid(6);
        QString modelFile = m_piperVoicesDir + QStringLiteral("/en_US-lessac-medium.onnx");
        if (voiceName == QLatin1String("amy")) {
            modelFile = m_piperVoicesDir + QStringLiteral("/en_US-amy-medium.onnx");
        }

        // Piper length-scale: smaller = faster speech (inverse of playback rate)
        const double lengthScale = 1.0 / std::clamp(m_playbackRate, 0.5, 3.0);

        const QString shellCmd = QStringLiteral("%1 -m \"%2\" --length-scale %3 --output-raw | aplay -r 22050 -f S16_LE -t raw -q")
            .arg(m_piperExecutable, modelFile, QString::number(lengthScale, 'f', 3));

        m_speechProcess->start(QStringLiteral("bash"), QStringList() << QStringLiteral("-c") << shellCmd);
        m_speechProcess->write(sentence.toUtf8());
        m_speechProcess->closeWriteChannel();
        return;
    }

    // 2. Microsoft Azure Edge Neural Speech
    if (m_currentVoice.startsWith(QLatin1String("edge:")) && m_hasEdgeTts) {
        const QString voiceName = m_currentVoice.mid(5);
        const int ratePercent = static_cast<int>((m_playbackRate - 1.0) * 100.0);
        const QString rateStr = (ratePercent >= 0 ? QString("+%1%").arg(ratePercent) : QString("%1%").arg(ratePercent));
        const QString edgeExe = QStandardPaths::findExecutable(QStringLiteral("edge-tts")).isEmpty()
            ? QDir::homePath() + QStringLiteral("/.local/bin/edge-tts")
            : QStandardPaths::findExecutable(QStringLiteral("edge-tts"));

        const QString shellCmd = QStringLiteral("%1 -f - --voice \"%2\" --rate=\"%3\" --write-media /tmp/lunaread_tts.mp3 && mpv --no-video --really-quiet /tmp/lunaread_tts.mp3")
            .arg(edgeExe, voiceName, rateStr);

        m_speechProcess->start(QStringLiteral("bash"), QStringList() << QStringLiteral("-c") << shellCmd);
        m_speechProcess->write(sentence.toUtf8());
        m_speechProcess->closeWriteChannel();
        return;
    }

    // 3. Fallback to system synthesizers (spd-say / espeak)
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

void TtsService::speakText(const QString& text) {
    if (text.trimmed().isEmpty()) return;

    if (!m_piperExecutable.isEmpty() && !m_piperVoicesDir.isEmpty()) {
        const QString modelFile = m_piperVoicesDir + QStringLiteral("/en_US-lessac-medium.onnx");
        const QString shellCmd = QStringLiteral("%1 -m \"%2\" --output-raw | aplay -r 22050 -f S16_LE -t raw -q")
            .arg(m_piperExecutable, modelFile);
        auto* p = new QProcess();
        connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), p, &QObject::deleteLater);
        p->start(QStringLiteral("bash"), QStringList() << QStringLiteral("-c") << shellCmd);
        p->write(text.trimmed().toUtf8());
        p->closeWriteChannel();
        return;
    }

    if (!m_speechBackend.isEmpty()) {
        QStringList args;
        if (m_speechBackend.contains(QStringLiteral("spd-say"))) {
            args << QStringLiteral("-r") << QStringLiteral("0") << text.trimmed();
        } else {
            args << text.trimmed();
        }
        QProcess::startDetached(m_speechBackend, args);
    }
}

} // namespace LunaRead
