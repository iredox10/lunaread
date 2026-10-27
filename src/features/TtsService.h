#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QProcess>

namespace LunaRead {

/**
 * @brief Text-to-Speech (TTS) service with authentic Linux speech engine integration.
 *
 * Utilizes system speech synthesizers (spd-say / espeak-ng / espeak) for spoken narration
 * with fallback to cadence estimation. Emits activeSentenceChanged for karaoke synchronization
 * and supports variable playback rates and sentence seeking.
 */
class TtsService : public QObject {
    Q_OBJECT
public:
    explicit TtsService(QObject* parent = nullptr);
    ~TtsService() override;

    void loadText(const QString& text);
    void play();
    void pause();
    void stop();
    void nextSentence();
    void prevSentence();
    void seekSentence(int index);

    [[nodiscard]] bool isPlaying() const noexcept { return m_isPlaying; }
    [[nodiscard]] int currentIndex() const noexcept { return m_currentIndex; }
    [[nodiscard]] int sentenceCount() const noexcept { return static_cast<int>(m_sentences.size()); }
    [[nodiscard]] const QStringList& sentences() const noexcept { return m_sentences; }
    [[nodiscard]] double playbackRate() const noexcept { return m_playbackRate; }
    void setPlaybackRate(double rate);
    [[nodiscard]] QString speechBackend() const noexcept { return m_speechBackend; }

signals:
    void activeSentenceChanged(int index, const QString& sentence);
    void playbackFinished();
    void playbackStateChanged(bool isPlaying);
    void playbackRateChanged(double rate);
    void sentencesLoaded(int count);

private:
    void speakCurrentSentence();
    void stopCurrentSpeech();
    void detectBackend();

    QStringList m_sentences;
    int m_currentIndex = 0;
    bool m_isPlaying = false;
    double m_playbackRate = 1.0;
    QTimer* m_sentenceTimer = nullptr;
    QProcess* m_speechProcess = nullptr;
    QString m_speechBackend;
};

} // namespace LunaRead
