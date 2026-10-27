#pragma once

#include <QDialog>
#include <QString>
#include <QWidget>
#include <QPropertyAnimation>

class QLabel;
class QPushButton;
class QGridLayout;
class QKeyEvent;
class QPaintEvent;

namespace LunaRead {

/**
 * @brief Visual bullet indicator widget with smooth animation & shake support.
 */
class PinBulletsWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int shakeOffset READ shakeOffset WRITE setShakeOffset)

public:
    explicit PinBulletsWidget(QWidget* parent = nullptr);
    ~PinBulletsWidget() override = default;

    [[nodiscard]] int shakeOffset() const noexcept { return m_shakeOffset; }
    void setShakeOffset(int offset);

    [[nodiscard]] int pinLength() const noexcept { return m_pinLength; }
    void setPinLength(int length);

    [[nodiscard]] int enteredCount() const noexcept { return m_enteredCount; }
    void setEnteredCount(int count);

    [[nodiscard]] bool isError() const noexcept { return m_isError; }
    void setError(bool error);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_shakeOffset = 0;
    int m_pinLength = 4;
    int m_enteredCount = 0;
    bool m_isError = false;
};

/**
 * @brief Interactive PIN entry, setup, change, and removal dialog.
 *
 * Implements Moon+ Reader style App Lock & Private Bookshelf PIN authentication:
 * - 4-digit or custom length PIN input with obscured bullets
 * - On-screen number pad (0-9, Backspace, Clear) + full keyboard support
 * - Modes: Unlock, SetPin, ChangePin, RemovePin
 * - Red error shake animation on invalid PIN
 * - Signals: unlocked(), pinSetSuccessfully()
 */
class PrivacyLockDialog : public QDialog {
    Q_OBJECT

public:
    enum class Mode {
        Unlock,
        SetPin,
        ChangePin,
        RemovePin
    };

    explicit PrivacyLockDialog(Mode mode = Mode::Unlock, QWidget* parent = nullptr);
    ~PrivacyLockDialog() override;

    [[nodiscard]] Mode mode() const noexcept { return m_mode; }
    void setMode(Mode mode);

    [[nodiscard]] int pinLength() const noexcept { return m_pinLength; }
    void setPinLength(int length);

    [[nodiscard]] QString enteredPin() const { return m_currentPin; }

    void setTitle(const QString& title);
    void setDescription(const QString& description);

    // Interactive actions (keypad, keyboard, or programmatic)
    void appendDigit(const QString& digit);
    void deleteDigit();
    void clearPin();
    void submitPin();
    void triggerError(const QString& errorMessage);

signals:
    void unlocked();
    void pinSetSuccessfully();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class Step {
        Initial,        // Unlock, RemovePin, or initial current PIN step of ChangePin
        EnterNew,       // Step 1 of SetPin, Step 2 of ChangePin
        ConfirmNew      // Step 2 of SetPin, Step 3 of ChangePin
    };

    void setupUi();
    void updateLabelsForStep();
    void resetState();

    Mode m_mode = Mode::Unlock;
    Step m_step = Step::Initial;
    int m_pinLength = 4;

    QString m_currentPin;
    QString m_firstPin;
    QString m_oldPin;
    QString m_customTitle;
    QString m_customDesc;

    QLabel* m_iconLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    PinBulletsWidget* m_bulletsWidget = nullptr;
    QLabel* m_statusLabel = nullptr;
    QGridLayout* m_keypadLayout = nullptr;
    QPushButton* m_submitBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPropertyAnimation* m_shakeAnimation = nullptr;
};

} // namespace LunaRead
