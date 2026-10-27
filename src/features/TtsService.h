#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QProcess>
#include <QList>

namespace LunaRead {

/**
 * @brief Sleep timer duration presets for TTS playback.
 */
enum class SleepTimerMode {
    Off,
    Minutes_5,
    Minutes_10,
    Minutes_15,
    Minutes_30,
    Minutes_45,
    Minutes_60,
    EndOfChapter
};

/**
 * @brief Text-to-Speech voice descriptor.
 */
struct TtsVoice {
    QString id;
    QString name;
    QString language;
};

/**
 * @brief Text-to-Speech (TTS) service with authentic Linux speech engine integration.
 *
 * Utilizes system speech synthesizers (spd-say / espeak-ng / espeak) for spoken narration
 * with fallback to cadence estimation. Emits activeSentenceChanged for karaoke synchronization,
 * supports variable playback rates (0.5x to 3.0x), pitch adjustments (-50 to +50),
 * voice presets, and countdown / EndOfChapter sleep timers.
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
    [[nodiscard]] QString speechBackend() const noexcept { return m_speechBackend; }

    // --- Voice & Speech Configuration ---
    [[nodiscard]] const QList<TtsVoice>& availableVoices() const noexcept { return m_availableVoices; }
    [[nodiscard]] QString currentVoice() const noexcept { return m_currentVoice; }
    [[nodiscard]] QString voice() const noexcept { return m_currentVoice; }
    void setVoice(const QString& voiceId);

    [[nodiscard]] double playbackRate() const noexcept { return m_playbackRate; }
    void setPlaybackRate(double rate);
    [[nodiscard]] double speed() const noexcept { return m_playbackRate; }
    void setSpeed(double speed) { setPlaybackRate(speed); }

    [[nodiscard]] int pitch() const noexcept { return m_pitch; }
    void setPitch(int pitch);

    // --- Sleep Timer ---
    void setSleepTimer(SleepTimerMode mode);
    [[nodiscard]] SleepTimerMode sleepTimerMode() const noexcept { return m_sleepMode; }
    [[nodiscard]] int sleepTimerRemainingSeconds() const noexcept { return m_sleepRemainingSeconds; }
    void notifyChapterFinished();
    void setChapterEndSentenceIndex(int index) noexcept { m_chapterEndSentenceIndex = index; }
    [[nodiscard]] int chapterEndSentenceIndex() const noexcept { return m_chapterEndSentenceIndex; }

signals:
    void activeSentenceChanged(int index, const QString& sentence);
    void playbackFinished();
    void playbackStateChanged(bool isPlaying);
    void playbackRateChanged(double rate);
    void sentencesLoaded(int count);
    void sleepTimerTick(int remainingSeconds);
    void sleepTimerExpired();
    void sleepTimerModeChanged(SleepTimerMode mode);
    void voiceChanged(const QString& voiceId);
    void pitchChanged(int pitch);

private slots:
    void onSleepCountdownTick();

private:
    void speakCurrentSentence();
    void stopCurrentSpeech();
    void detectBackend();
    void detectVoices();

    QStringList m_sentences;
    int m_currentIndex = 0;
    bool m_isPlaying = false;
    double m_playbackRate = 1.0;
    int m_pitch = 0; // -50 to +50
    QString m_currentVoice = QStringLiteral("default");
    QList<TtsVoice> m_availableVoices;

    QTimer* m_sentenceTimer = nullptr;
    QProcess* m_speechProcess = nullptr;
    QString m_speechBackend;

    // Sleep timer state
    SleepTimerMode m_sleepMode = SleepTimerMode::Off;
    int m_sleepRemainingSeconds = 0;
    int m_chapterEndSentenceIndex = -1;
    QTimer* m_sleepCountdownTimer = nullptr;
};

} // namespace LunaRead
