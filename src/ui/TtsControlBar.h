#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QMenu>
#include <QActionGroup>
#include <QTimer>
#include <QPointer>

#include "../core/ThemeTokens.h"
#include "../features/TtsService.h"

namespace LunaRead {

/**
 * @brief Sleep timer duration presets for TTS playback.
 */
enum class SleepTimerOption {
    Off = 0,
    Minutes5 = 5,
    Minutes10 = 10,
    Minutes15 = 15,
    Minutes30 = 30,
    EndOfChapter = -1
};

/**
 * @brief Floating karaoke-style TTS audio control bar for reader viewport.
 *
 * Implements a floating HUD anchored at the bottom of the reader viewport with:
 * - Frosted glass translucent appearance and subtle 1px border outline.
 * - Sentence progress bar & sentence counter label ("Sentence 14 / 85").
 * - Interactive seeking via progress slider.
 * - Real-time Karaoke sentence preview label.
 * - Play / Pause button with smooth vector icon toggle.
 * - Skip Previous and Skip Next sentence buttons.
 * - Speech rate selector menu (1.0x, 1.25x, 1.5x, 2.0x).
 * - Sleep timer dropdown menu (Off, 5 min, 10 min, 15 min, 30 min, End of Chapter)
 *   with active countdown ticker.
 * - Close button to dismiss HUD and stop playback.
 * - Real-time karaoke synchronization with TtsService.
 */
class TtsControlBar : public QWidget {
    Q_OBJECT

public:
    explicit TtsControlBar(QWidget* parent = nullptr);
    explicit TtsControlBar(TtsService* service, QWidget* parent = nullptr);
    ~TtsControlBar() override;

    // Service binding
    void setTtsService(TtsService* service);
    [[nodiscard]] TtsService* ttsService() const noexcept { return m_ttsService.data(); }

    // Theme integration
    void updateTheme(const ReaderThemeTokens& tokens);

    // Anchor & repositioning inside viewport
    void reposition(const QSize& parentSize);

    // Getters
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] int currentSentenceIndex() const noexcept { return m_currentIndex; }
    [[nodiscard]] int totalSentences() const noexcept { return m_totalSentences; }
    [[nodiscard]] double speechRate() const noexcept;
    [[nodiscard]] SleepTimerOption sleepTimerOption() const noexcept { return m_sleepOption; }
    [[nodiscard]] int sleepSecondsRemaining() const noexcept { return m_sleepSecondsRemaining; }
    [[nodiscard]] QString activeSentenceText() const;

public slots:
    void play();
    void pause();
    void togglePlayPause();
    void stop();
    void nextSentence();
    void prevSentence();
    void seekToSentence(int index);
    void setSpeechRate(double rate);
    void setSleepTimer(SleepTimerOption option);
    void dismiss();

signals:
    void playbackStateChanged(bool isPlaying);
    void sentenceHighlighted(int index, const QString& sentence);
    void speechRateChanged(double rate);
    void sleepTimerChanged(SleepTimerOption option);
    void sleepTimerTriggered();
    void closeRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onActiveSentenceChanged(int index, const QString& sentence);
    void onPlaybackStateChanged(bool isPlaying);
    void onPlaybackFinished();
    void onSentencesLoaded(int count);
    void onSleepTimerTick();
    void onSliderValueChanged(int value);
    void onProgressSliderReleased();

private:
    void setupUi();
    void setupStyle();
    void buildSpeechRateMenu();
    void buildSleepTimerMenu();
    void updatePlayPauseUi(bool isPlaying);
    void updateSentenceCounterUi(int current, int total);
    void updateSleepTimerUi();

    // High-DPI antialiased vector icon generation
    [[nodiscard]] QIcon drawPlayIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawPauseIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawPrevIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawNextIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawCloseIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawSpeedIcon(const QColor& color) const;
    [[nodiscard]] QIcon drawMoonIcon(const QColor& color) const;

    QPointer<TtsService> m_ttsService;
    ReaderThemeTokens m_tokens;

    // UI elements
    QLabel* m_counterLabel = nullptr;
    QLabel* m_karaokePreviewLabel = nullptr;
    QSlider* m_progressSlider = nullptr;
    QPushButton* m_prevBtn = nullptr;
    QPushButton* m_playPauseBtn = nullptr;
    QPushButton* m_nextBtn = nullptr;
    QPushButton* m_rateBtn = nullptr;
    QMenu* m_rateMenu = nullptr;
    QActionGroup* m_rateActionGroup = nullptr;
    QPushButton* m_sleepBtn = nullptr;
    QMenu* m_sleepMenu = nullptr;
    QActionGroup* m_sleepActionGroup = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // State
    int m_currentIndex = 0;
    int m_totalSentences = 0;
    double m_currentRate = 1.0;
    bool m_isSliderDragging = false;

    // Sleep Timer
    SleepTimerOption m_sleepOption = SleepTimerOption::Off;
    int m_sleepSecondsRemaining = 0;
    QTimer* m_sleepCountdownTimer = nullptr;
};

} // namespace LunaRead
