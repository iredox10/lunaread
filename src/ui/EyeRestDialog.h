#pragma once

#include <QDialog>
#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include "../core/ThemeTokens.h"

namespace LunaRead {

/**
 * @brief Custom widget rendering the calming eye relaxation graphic & countdown ring.
 *
 * Implements the 20-20-20 rule visualization:
 * - Circular countdown progress arc
 * - Stylized soothing eye symbol with gentle iris, pupil, and glint
 * - Prominent countdown text and completion state
 */
class EyeRelaxationGraphicWidget : public QWidget {
    Q_OBJECT

public:
    explicit EyeRelaxationGraphicWidget(QWidget* parent = nullptr);
    ~EyeRelaxationGraphicWidget() override = default;

    void setCountdown(int remainingSeconds, int totalSeconds);
    void setColors(const QColor& ringColor, const QColor& trackColor, const QColor& textColor);

    [[nodiscard]] int remainingSeconds() const noexcept { return m_remainingSeconds; }
    [[nodiscard]] int totalSeconds() const noexcept { return m_totalSeconds; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_remainingSeconds = 20;
    int m_totalSeconds = 20;
    QColor m_ringColor = QColor(16, 185, 129);  // Calming emerald
    QColor m_trackColor = QColor(255, 255, 255, 30);
    QColor m_textColor = QColor(241, 245, 249);
};

/**
 * @brief Gentle semi-transparent overlay dialog for eye rest reminder.
 *
 * Implements Moon+ Reader's iconic Eye Rest reminder ("Look 20 feet away for 20 seconds
 * after 45 minutes of reading").
 *
 * Features:
 * - Calming translucent overlay backdrop dims viewport glare
 * - Stylized countdown graphic (20 seconds for the 20-20-20 rule)
 * - "Take a Break" message with session duration
 * - "I'm back / Skip" button to resume reading
 * - "Snooze (15m)" button to snooze reminder for 15 minutes
 * - "Mute for this session" option
 * - Theme-aware styling (adapts to Parchment, OLED Black, Sepia, Night, Modern Light)
 */
class EyeRestDialog : public QDialog {
    Q_OBJECT
    Q_PROPERTY(int countdownSeconds READ countdownSeconds WRITE setCountdownSeconds)
    Q_PROPERTY(int sessionMinutesRead READ sessionMinutesRead WRITE setSessionMinutesRead)

public:
    explicit EyeRestDialog(int sessionMinutesRead = 45, QWidget* parent = nullptr);
    ~EyeRestDialog() override = default;

    [[nodiscard]] int sessionMinutesRead() const noexcept { return m_sessionMinutesRead; }
    void setSessionMinutesRead(int minutes);

    [[nodiscard]] int countdownSeconds() const noexcept { return m_totalCountdownSeconds; }
    void setCountdownSeconds(int seconds);

    [[nodiscard]] int remainingSeconds() const noexcept { return m_remainingSeconds; }

    void applyTheme(const ReaderThemeTokens& tokens);

    void startCountdown(int seconds = 20);
    void pauseCountdown();
    void resumeCountdown();
    void resetCountdown();

signals:
    void restCompleted();
    void restSkipped();
    void snoozeRequested(int minutes);
    void muteRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onTimerTick();
    void onSkipClicked();
    void onSnoozeClicked();
    void onMuteClicked();

private:
    void setupUi();
    void updateTimerDisplay();
    void updateStyles();

    int m_sessionMinutesRead = 45;
    int m_totalCountdownSeconds = 20;
    int m_remainingSeconds = 20;

    QTimer* m_timer = nullptr;
    ReaderThemeTokens m_tokens;
    bool m_hasCustomTokens = false;

    // UI elements
    QWidget* m_centralCard = nullptr;
    EyeRelaxationGraphicWidget* m_graphicWidget = nullptr;
    QLabel* m_badgeLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_sessionLabel = nullptr;
    QLabel* m_adviceLabel = nullptr;
    QPushButton* m_skipBtn = nullptr;
    QPushButton* m_snoozeBtn = nullptr;
    QPushButton* m_muteBtn = nullptr;
};

} // namespace LunaRead
