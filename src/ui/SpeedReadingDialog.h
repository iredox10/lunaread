#pragma once

#include <QDialog>
#include <QTimer>
#include <QStringList>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QComboBox>
#include "../core/ThemeTokens.h"

namespace LunaRead {

class SpeedReadingDialog : public QDialog {
    Q_OBJECT
public:
    explicit SpeedReadingDialog(const QString& text, int initialWpm = 350, QWidget* parent = nullptr);
    ~SpeedReadingDialog() override = default;

    void setText(const QString& text);
    void setWpm(int wpm);
    void applyTheme(const ReaderThemeTokens& tokens);

public slots:
    void play();
    void pause();
    void togglePlayPause();
    void stepBack(int words = 10);
    void stepForward(int words = 10);
    void restart();

signals:
    void wordIndexChanged(int currentIndex, int totalWords);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onTick();
    void onSliderMoved(int position);
    void onWpmChanged(int wpm);
    void onChunkSizeChanged(int index);

private:
    void setupUi();
    void updateWordDisplay();
    void updateProgressDisplay();
    int calculateNextDelayMs() const;
    static QString formatOrpWord(const QString& word, const QColor& accentColor, const QColor& textColor);

    QStringList m_words;
    int m_currentIndex = 0;
    int m_wpm = 350;
    int m_chunkSize = 1;
    bool m_isPlaying = false;

    QTimer* m_timer = nullptr;
    ReaderThemeTokens m_tokens;

    QLabel* m_orpDisplay = nullptr;
    QLabel* m_contextPreview = nullptr;
    QLabel* m_wpmLabel = nullptr;
    QLabel* m_progressLabel = nullptr;
    QLabel* m_etaLabel = nullptr;

    QPushButton* m_playPauseBtn = nullptr;
    QPushButton* m_stepBackBtn = nullptr;
    QPushButton* m_stepForwardBtn = nullptr;
    QPushButton* m_restartBtn = nullptr;

    QSlider* m_wpmSlider = nullptr;
    QSlider* m_progressSlider = nullptr;
    QComboBox* m_chunkCombo = nullptr;
};

} // namespace LunaRead
