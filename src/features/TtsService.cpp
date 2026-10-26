#include "TtsService.h"
#include <QRegularExpression>
#include <algorithm>

namespace LunaRead {

TtsService::TtsService(QObject* parent) : QObject(parent) {
    m_sentenceTimer = new QTimer(this);
    connect(m_sentenceTimer, &QTimer::timeout, this, &TtsService::nextSentence);
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
    restartTimerIfPlaying();
}

void TtsService::pause() {
    if (!m_isPlaying) return;
    m_isPlaying = false;
    m_sentenceTimer->stop();
    emit playbackStateChanged(false);
}

void TtsService::stop() {
    const bool hadPlaying = m_isPlaying;
    m_isPlaying = false;
    m_sentenceTimer->stop();
    m_currentIndex = 0;
    if (hadPlaying) {
        emit playbackStateChanged(false);
    }
}

void TtsService::nextSentence() {
    if (m_currentIndex + 1 < m_sentences.size()) {
        m_currentIndex++;
        emit activeSentenceChanged(m_currentIndex, m_sentences[m_currentIndex]);
        restartTimerIfPlaying();
    } else {
        stop();
        emit playbackFinished();
    }
}

void TtsService::prevSentence() {
    if (m_currentIndex > 0) {
        m_currentIndex--;
        emit activeSentenceChanged(m_currentIndex, m_sentences[m_currentIndex]);
        restartTimerIfPlaying();
    }
}

void TtsService::seekSentence(int index) {
    if (m_sentences.isEmpty()) return;
    const int target = std::clamp(index, 0, static_cast<int>(m_sentences.size() - 1));
    if (target != m_currentIndex) {
        m_currentIndex = target;
        emit activeSentenceChanged(m_currentIndex, m_sentences[m_currentIndex]);
        restartTimerIfPlaying();
    }
}

void TtsService::setPlaybackRate(double rate) {
    if (rate <= 0.1) rate = 1.0;
    m_playbackRate = rate;
    emit playbackRateChanged(m_playbackRate);
    restartTimerIfPlaying();
}

void TtsService::restartTimerIfPlaying() {
    if (m_isPlaying) {
        const int ms = static_cast<int>(3000.0 / std::max(0.1, m_playbackRate));
        m_sentenceTimer->start(ms);
    }
}

} // namespace LunaRead

