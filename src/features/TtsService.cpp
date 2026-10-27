#include "TtsService.h"
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>

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

    detectBackend();
}

TtsService::~TtsService() {
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

void TtsService::loadText(const QString& text) {
    const bool wasPlaying = m_isPlaying;
    stop();

    m_sentences.clear();
    m_currentIndex = 0;

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

    if (m_currentIndex + 1 < m_sentences.size()) {
        m_currentIndex++;
        emit activeSentenceChanged(m_currentIndex, m_sentences[m_currentIndex]);
        speakCurrentSentence();
    } else {
        stop();
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
    if (rate <= 0.1) rate = 1.0;
    m_playbackRate = rate;
    emit playbackRateChanged(m_playbackRate);
    if (m_isPlaying) {
        speakCurrentSentence();
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
    if (m_speechBackend.contains("spd-say")) {
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
        if (m_speechBackend.contains("spd-say")) {
            // Rate range in spd-say is -100 to 100 (0 is normal)
            int rateVal = std::clamp(static_cast<int>((m_playbackRate - 1.0) * 100.0), -90, 90);
            args << QStringLiteral("-r") << QString::number(rateVal)
                 << QStringLiteral("-w") // Wait for speech to finish before returning
                 << sentence;
        } else if (m_speechBackend.contains("espeak")) {
            // Speed in words per minute (normal ~175)
            int wpm = std::clamp(static_cast<int>(175 * m_playbackRate), 80, 450);
            args << QStringLiteral("-s") << QString::number(wpm) << sentence;
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
