#include "PrivacyLockDialog.h"
#include "../features/PrivacyLockService.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QKeyEvent>
#include <QPaintEvent>
#include <algorithm>

namespace LunaRead {

// =========================================================================
// PinBulletsWidget Implementation
// =========================================================================

PinBulletsWidget::PinBulletsWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(44);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void PinBulletsWidget::setShakeOffset(int offset) {
    if (m_shakeOffset != offset) {
        m_shakeOffset = offset;
        update();
    }
}

void PinBulletsWidget::setPinLength(int length) {
    m_pinLength = std::max(1, length);
    update();
}

void PinBulletsWidget::setEnteredCount(int count) {
    if (m_enteredCount != count) {
        m_enteredCount = count;
        update();
    }
}

void PinBulletsWidget::setError(bool error) {
    if (m_isError != error) {
        m_isError = error;
        update();
    }
}

QSize PinBulletsWidget::sizeHint() const {
    return QSize(240, 44);
}

void PinBulletsWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int totalSlots = (m_pinLength > 0) ? m_pinLength : std::max(4, m_enteredCount);
    constexpr qreal radius = 8.5;
    constexpr qreal diameter = radius * 2.0;
    constexpr qreal spacing = 16.0;
    const qreal totalWidth = static_cast<qreal>(totalSlots) * diameter + static_cast<qreal>(totalSlots - 1) * spacing;

    const qreal startX = (static_cast<qreal>(width()) - totalWidth) / 2.0 + static_cast<qreal>(m_shakeOffset);
    const qreal centerY = static_cast<qreal>(height()) / 2.0;

    const QColor activeColor = m_isError ? QColor(QStringLiteral("#EF4444")) : QColor(QStringLiteral("#3B82F6"));
    const QColor inactiveBorder = m_isError ? QColor(QStringLiteral("#EF4444")) : QColor(QStringLiteral("#9CA3AF"));
    const QColor inactiveFill = m_isError ? QColor(QStringLiteral("#FEE2E2")) : QColor(QStringLiteral("#F3F4F6"));

    for (int i = 0; i < totalSlots; ++i) {
        const qreal cx = startX + static_cast<qreal>(i) * (diameter + spacing) + radius;
        const qreal cy = centerY;

        if (i < m_enteredCount) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(activeColor);
            painter.drawEllipse(QPointF(cx, cy), radius, radius);
        } else {
            painter.setPen(QPen(inactiveBorder, 2.0));
            painter.setBrush(inactiveFill);
            painter.drawEllipse(QPointF(cx, cy), radius - 1.0, radius - 1.0);
        }
    }
}

// =========================================================================
// PrivacyLockDialog Implementation
// =========================================================================

PrivacyLockDialog::PrivacyLockDialog(Mode mode, QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
{
    setupUi();
    updateLabelsForStep();
}

PrivacyLockDialog::~PrivacyLockDialog() {
    if (m_shakeAnimation) {
        m_shakeAnimation->stop();
        delete m_shakeAnimation;
        m_shakeAnimation = nullptr;
    }
}

void PrivacyLockDialog::setupUi() {
    setWindowTitle(tr("Privacy PIN"));
    setModal(true);
    setFixedSize(360, 520);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #FFFFFF; }"
        "QLabel { color: #111827; font-family: 'Segoe UI', system-ui, -apple-system, sans-serif; }"
    ));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(28, 20, 28, 20);
    rootLayout->setSpacing(8);

    // Icon Header
    m_iconLabel = new QLabel(QStringLiteral("🔒"), this);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet(QStringLiteral("font-size: 32px; min-height: 40px; margin-bottom: 2px;"));
    rootLayout->addWidget(m_iconLabel);

    // Title
    m_titleLabel = new QLabel(this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: #111827;"));
    rootLayout->addWidget(m_titleLabel);

    // Subtitle / Prompt
    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setWordWrap(true);
    m_subtitleLabel->setStyleSheet(QStringLiteral("font-size: 13px; color: #6B7280; min-height: 28px;"));
    rootLayout->addWidget(m_subtitleLabel);

    // PIN Bullets Display
    m_bulletsWidget = new PinBulletsWidget(this);
    m_bulletsWidget->setPinLength(m_pinLength);
    rootLayout->addWidget(m_bulletsWidget);

    // Status / Error message
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; min-height: 18px;"));
    rootLayout->addWidget(m_statusLabel);

    // Number Pad Grid
    auto* keypadContainer = new QWidget(this);
    m_keypadLayout = new QGridLayout(keypadContainer);
    m_keypadLayout->setSpacing(10);
    m_keypadLayout->setContentsMargins(12, 4, 12, 8);

    const QString keyStyle = QStringLiteral(
        "QPushButton {"
        "  background-color: #F3F4F6;"
        "  color: #111827;"
        "  border: 1px solid #E5E7EB;"
        "  border-radius: 26px;"
        "  font-size: 20px;"
        "  font-weight: 600;"
        "  min-width: 52px;"
        "  min-height: 52px;"
        "  max-width: 52px;"
        "  max-height: 52px;"
        "}"
        "QPushButton:hover { background-color: #E5E7EB; border-color: #D1D5DB; }"
        "QPushButton:pressed { background-color: #D1D5DB; }"
    );

    const QString actionKeyStyle = QStringLiteral(
        "QPushButton {"
        "  background-color: #F9FAFB;"
        "  color: #4B5563;"
        "  border: 1px solid #E5E7EB;"
        "  border-radius: 26px;"
        "  font-size: 15px;"
        "  font-weight: 600;"
        "  min-width: 52px;"
        "  min-height: 52px;"
        "  max-width: 52px;"
        "  max-height: 52px;"
        "}"
        "QPushButton:hover { background-color: #F3F4F6; color: #111827; border-color: #D1D5DB; }"
        "QPushButton:pressed { background-color: #E5E7EB; }"
    );

    // Keys 1 to 9
    for (int num = 1; num <= 9; ++num) {
        const int row = (num - 1) / 3;
        const int col = (num - 1) % 3;
        const QString digitStr = QString::number(num);

        auto* btn = new QPushButton(digitStr, keypadContainer);
        btn->setStyleSheet(keyStyle);
        btn->setFocusPolicy(Qt::NoFocus);
        connect(btn, &QPushButton::clicked, this, [this, digitStr]() {
            appendDigit(digitStr);
        });
        m_keypadLayout->addWidget(btn, row, col, Qt::AlignCenter);
    }

    // Row 3: Clear ("C"), 0, Backspace ("⌫")
    auto* clearBtn = new QPushButton(tr("C"), keypadContainer);
    clearBtn->setStyleSheet(actionKeyStyle);
    clearBtn->setToolTip(tr("Clear"));
    clearBtn->setFocusPolicy(Qt::NoFocus);
    connect(clearBtn, &QPushButton::clicked, this, &PrivacyLockDialog::clearPin);
    m_keypadLayout->addWidget(clearBtn, 3, 0, Qt::AlignCenter);

    auto* zeroBtn = new QPushButton(QStringLiteral("0"), keypadContainer);
    zeroBtn->setStyleSheet(keyStyle);
    zeroBtn->setFocusPolicy(Qt::NoFocus);
    connect(zeroBtn, &QPushButton::clicked, this, [this]() {
        appendDigit(QStringLiteral("0"));
    });
    m_keypadLayout->addWidget(zeroBtn, 3, 1, Qt::AlignCenter);

    auto* backspaceBtn = new QPushButton(QStringLiteral("⌫"), keypadContainer);
    backspaceBtn->setStyleSheet(actionKeyStyle);
    backspaceBtn->setToolTip(tr("Backspace"));
    backspaceBtn->setFocusPolicy(Qt::NoFocus);
    connect(backspaceBtn, &QPushButton::clicked, this, &PrivacyLockDialog::deleteDigit);
    m_keypadLayout->addWidget(backspaceBtn, 3, 2, Qt::AlignCenter);

    rootLayout->addWidget(keypadContainer);

    // Bottom Action Row (Cancel & Submit)
    auto* actionLayout = new QHBoxLayout();
    actionLayout->setContentsMargins(6, 4, 6, 0);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: #6B7280;"
        "  border: 1px solid #D1D5DB;"
        "  border-radius: 6px;"
        "  padding: 8px 16px;"
        "  font-weight: 500;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover { background-color: #F3F4F6; color: #111827; }"
        "QPushButton:pressed { background-color: #E5E7EB; }"
    ));
    connect(m_cancelBtn, &QPushButton::clicked, this, &PrivacyLockDialog::reject);
    actionLayout->addWidget(m_cancelBtn);

    actionLayout->addStretch();

    m_submitBtn = new QPushButton(tr("Unlock"), this);
    m_submitBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #2563EB;"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 20px;"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover { background-color: #1D4ED8; }"
        "QPushButton:pressed { background-color: #1E40AF; }"
    ));
    connect(m_submitBtn, &QPushButton::clicked, this, &PrivacyLockDialog::submitPin);
    actionLayout->addWidget(m_submitBtn);

    rootLayout->addLayout(actionLayout);
}

void PrivacyLockDialog::setMode(Mode mode) {
    if (m_mode != mode) {
        m_mode = mode;
        resetState();
    }
}

void PrivacyLockDialog::setPinLength(int length) {
    m_pinLength = length;
    if (m_bulletsWidget) {
        m_bulletsWidget->setPinLength(length);
    }
    clearPin();
}

void PrivacyLockDialog::setTitle(const QString& title) {
    m_customTitle = title;
    updateLabelsForStep();
}

void PrivacyLockDialog::setDescription(const QString& description) {
    m_customDesc = description;
    updateLabelsForStep();
}

void PrivacyLockDialog::resetState() {
    m_step = (m_mode == Mode::SetPin) ? Step::EnterNew : Step::Initial;
    m_currentPin.clear();
    m_firstPin.clear();
    m_oldPin.clear();
    if (m_bulletsWidget) {
        m_bulletsWidget->setEnteredCount(0);
        m_bulletsWidget->setError(false);
    }
    if (m_statusLabel) {
        m_statusLabel->clear();
    }
    updateLabelsForStep();
}

void PrivacyLockDialog::updateLabelsForStep() {
    switch (m_mode) {
    case Mode::Unlock:
        m_titleLabel->setText(m_customTitle.isEmpty() ? tr("LunaRead Locked") : m_customTitle);
        m_subtitleLabel->setText(m_customDesc.isEmpty() ? tr("Enter your PIN to unlock") : m_customDesc);
        m_submitBtn->setText(tr("Unlock"));
        break;

    case Mode::SetPin:
        m_titleLabel->setText(m_customTitle.isEmpty() ? tr("Set Privacy PIN") : m_customTitle);
        if (m_step == Step::EnterNew) {
            m_subtitleLabel->setText(tr("Enter a new 4-digit PIN"));
            m_submitBtn->setText(tr("Continue"));
        } else {
            m_subtitleLabel->setText(tr("Confirm your new 4-digit PIN"));
            m_submitBtn->setText(tr("Set PIN"));
        }
        break;

    case Mode::ChangePin:
        m_titleLabel->setText(m_customTitle.isEmpty() ? tr("Change Privacy PIN") : m_customTitle);
        if (m_step == Step::Initial) {
            m_subtitleLabel->setText(tr("Enter your current PIN"));
            m_submitBtn->setText(tr("Next"));
        } else if (m_step == Step::EnterNew) {
            m_subtitleLabel->setText(tr("Enter a new 4-digit PIN"));
            m_submitBtn->setText(tr("Continue"));
        } else {
            m_subtitleLabel->setText(tr("Confirm your new 4-digit PIN"));
            m_submitBtn->setText(tr("Change PIN"));
        }
        break;

    case Mode::RemovePin:
        m_titleLabel->setText(m_customTitle.isEmpty() ? tr("Remove Privacy PIN") : m_customTitle);
        m_subtitleLabel->setText(tr("Enter current PIN to remove protection"));
        m_submitBtn->setText(tr("Remove PIN"));
        break;
    }
}

void PrivacyLockDialog::appendDigit(const QString& digit) {
    if (digit.isEmpty()) {
        return;
    }

    if (m_pinLength > 0 && m_currentPin.length() >= m_pinLength) {
        return;
    }

    m_currentPin += digit;
    m_bulletsWidget->setEnteredCount(static_cast<int>(m_currentPin.length()));
    m_bulletsWidget->setError(false);
    m_statusLabel->clear();

    if (m_pinLength > 0 && m_currentPin.length() == m_pinLength) {
        submitPin();
    }
}

void PrivacyLockDialog::deleteDigit() {
    if (m_currentPin.isEmpty()) {
        return;
    }

    m_currentPin.chop(1);
    m_bulletsWidget->setEnteredCount(static_cast<int>(m_currentPin.length()));
    m_bulletsWidget->setError(false);
    m_statusLabel->clear();
}

void PrivacyLockDialog::clearPin() {
    m_currentPin.clear();
    m_bulletsWidget->setEnteredCount(0);
    m_bulletsWidget->setError(false);
    m_statusLabel->clear();
}

void PrivacyLockDialog::submitPin() {
    if (m_pinLength > 0 && m_currentPin.length() < m_pinLength) {
        triggerError(tr("Please enter a %1-digit PIN.").arg(m_pinLength));
        return;
    }

    if (m_currentPin.isEmpty()) {
        triggerError(tr("Please enter a PIN."));
        return;
    }

    auto& service = PrivacyLockService::instance();

    switch (m_mode) {
    case Mode::Unlock: {
        if (service.unlock(m_currentPin)) {
            emit unlocked();
            accept();
        } else {
            triggerError(tr("Incorrect PIN. Please try again."));
        }
        break;
    }

    case Mode::SetPin: {
        if (m_step == Step::EnterNew) {
            m_firstPin = m_currentPin;
            m_currentPin.clear();
            m_bulletsWidget->setEnteredCount(0);
            m_step = Step::ConfirmNew;
            updateLabelsForStep();
        } else if (m_step == Step::ConfirmNew) {
            if (m_currentPin == m_firstPin) {
                const bool ok = service.setPin(QString(), m_firstPin);
                if (ok) {
                    emit pinSetSuccessfully();
                    accept();
                } else {
                    triggerError(tr("Failed to save PIN."));
                }
            } else {
                triggerError(tr("PINs do not match. Try again."));
                m_firstPin.clear();
                m_step = Step::EnterNew;
                updateLabelsForStep();
            }
        }
        break;
    }

    case Mode::ChangePin: {
        if (m_step == Step::Initial) {
            if (service.verifyPin(m_currentPin)) {
                m_oldPin = m_currentPin;
                m_currentPin.clear();
                m_bulletsWidget->setEnteredCount(0);
                m_step = Step::EnterNew;
                updateLabelsForStep();
            } else {
                triggerError(tr("Current PIN is incorrect."));
            }
        } else if (m_step == Step::EnterNew) {
            m_firstPin = m_currentPin;
            m_currentPin.clear();
            m_bulletsWidget->setEnteredCount(0);
            m_step = Step::ConfirmNew;
            updateLabelsForStep();
        } else if (m_step == Step::ConfirmNew) {
            if (m_currentPin == m_firstPin) {
                const bool ok = service.setPin(m_oldPin, m_firstPin);
                if (ok) {
                    emit pinSetSuccessfully();
                    accept();
                } else {
                    triggerError(tr("Failed to update PIN."));
                }
            } else {
                triggerError(tr("PINs do not match. Try again."));
                m_firstPin.clear();
                m_step = Step::EnterNew;
                updateLabelsForStep();
            }
        }
        break;
    }

    case Mode::RemovePin: {
        if (service.verifyPin(m_currentPin)) {
            service.removePin(m_currentPin);
            emit pinSetSuccessfully();
            accept();
        } else {
            triggerError(tr("Incorrect PIN."));
        }
        break;
    }
    }
}

void PrivacyLockDialog::triggerError(const QString& errorMessage) {
    m_statusLabel->setText(errorMessage);
    m_statusLabel->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600; color: #EF4444; min-height: 18px;"));

    m_currentPin.clear();
    m_bulletsWidget->setEnteredCount(0);
    m_bulletsWidget->setError(true);

    if (m_shakeAnimation) {
        m_shakeAnimation->stop();
        delete m_shakeAnimation;
        m_shakeAnimation = nullptr;
    }

    m_shakeAnimation = new QPropertyAnimation(m_bulletsWidget, "shakeOffset", this);
    m_shakeAnimation->setDuration(360);
    m_shakeAnimation->setKeyValueAt(0.00, 0);
    m_shakeAnimation->setKeyValueAt(0.15, -14);
    m_shakeAnimation->setKeyValueAt(0.30, 14);
    m_shakeAnimation->setKeyValueAt(0.45, -10);
    m_shakeAnimation->setKeyValueAt(0.60, 10);
    m_shakeAnimation->setKeyValueAt(0.75, -5);
    m_shakeAnimation->setKeyValueAt(0.90, 5);
    m_shakeAnimation->setKeyValueAt(1.00, 0);

    connect(m_shakeAnimation, &QPropertyAnimation::finished, this, [this]() {
        m_bulletsWidget->setError(false);
    });

    m_shakeAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

void PrivacyLockDialog::keyPressEvent(QKeyEvent* event) {
    if (!event->text().isEmpty() && event->text().at(0).isDigit()) {
        appendDigit(event->text().left(1));
        event->accept();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Backspace:
        deleteDigit();
        event->accept();
        return;
    case Qt::Key_Delete:
        clearPin();
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        submitPin();
        event->accept();
        return;
    case Qt::Key_Escape:
        reject();
        event->accept();
        return;
    default:
        break;
    }

    QDialog::keyPressEvent(event);
}

} // namespace LunaRead
