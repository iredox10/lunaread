#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QProcess>
#include <QTimer>
#include <QTemporaryFile>
#include <memory>
#include <random>
#include <vector>

namespace LunaRead {

/**
 * @brief Ambient relaxing background sound and focus noise soundscapes.
 */
enum class SoundType {
    Rainfall = 0,    // 🌧️ Gentle Rain
    WhiteNoise,      // ⚪ White Noise (focus)
    PinkNoise,       // 📻 Pink Noise (deep focus)
    ForestStream,    // 🍃 Forest & Wind / Brook Stream
    OceanWaves,      // 🌊 Ocean Waves
    CafeAmbience,    // ☕ Cafe Ambience
    PageRustle       // 📖 Crisp paper flip sound effect
};

/**
 * @brief Internal procedural state containing filter memories, LFO phases, and resonator pools.
 */
struct ProceduralAudioState {
    // Pink noise Kellet IIR filter state variables
    float p_b0 = 0.0f;
    float p_b1 = 0.0f;
    float p_b2 = 0.0f;
    float p_b3 = 0.0f;
    float p_b4 = 0.0f;
    float p_b5 = 0.0f;
    float p_b6 = 0.0f;

    // Rainfall filter & droplet resonators
    float rain_lp = 0.0f;
    struct Droplet {
        float freq = 0.0f;
        float amp = 0.0f;
        float decay = 0.0f;
        float phase = 0.0f;
        bool active = false;
    };
    static constexpr int MaxDroplets = 16;
    Droplet droplets[MaxDroplets];

    // Forest stream bandpass & slow wind LFOs
    float stream_lp1 = 0.0f;
    float stream_lp2 = 0.0f;
    float stream_lfo1 = 0.0f;
    float stream_lfo2 = 0.0f;
    float wind_lfo = 0.0f;

    // Ocean waves swell LFO & surf foam filter
    float ocean_phase = 0.0f;
    float ocean_rumble_lp = 0.0f;
    float ocean_foam_hp = 0.0f;

    // Cafe chatter rumble and cup/spoon clinks
    float cafe_rumble = 0.0f;
    float cafe_clink_amp = 0.0f;
    float cafe_clink_phase = 0.0f;
    float cafe_clink_freq = 0.0f;

    // Deterministic random generator
    std::mt19937 rng{1337};
    std::uniform_real_distribution<float> uniformDist{-1.0f, 1.0f};

    void reset();
};

/**
 * @brief Reading Ambient Sound Generator and Background Noise Player for LunaRead.
 *
 * Implements Moon+ Reader's iconic ambient background soundscape player:
 * - Real-time procedural audio synthesis (Rainfall, White Noise, Pink Noise, Forest Stream,
 *   Ocean Waves, Cafe Ambience)
 * - Crisp 80ms paper flip sound effect on page turns
 * - Seamless loop audio playback via Linux audio server tools (pw-play, aplay, paplay)
 * - Dynamic volume control (0% to 100%)
 * - Sleep timer with automatic countdown and stop
 * - QSettings persistence for user sound preferences
 */
class AmbientSoundService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(SoundType currentSound READ currentSound NOTIFY soundTypeChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(int sleepTimerMinutes READ sleepTimerMinutes WRITE setSleepTimer NOTIFY sleepTimerChanged)
    Q_PROPERTY(bool pageFlipSoundEnabled READ isPageFlipSoundEnabled WRITE setPageFlipSoundEnabled NOTIFY pageFlipSoundEnabledChanged)

public:
    static constexpr int SampleRate = 44100;
    static constexpr int BitsPerSample = 16;
    static constexpr int Channels = 1;
    static constexpr int DefaultVolume = 70;

    static AmbientSoundService& instance();

    explicit AmbientSoundService(QObject* parent = nullptr);
    ~AmbientSoundService() override;

    // Required core methods
    void play(SoundType type, int volume = DefaultVolume);
    void stop();
    void setVolume(int volume);
    [[nodiscard]] bool isPlaying() const noexcept { return m_isPlaying; }
    void playPageFlipSound();

    // Sound and volume getters/toggles
    [[nodiscard]] SoundType currentSound() const noexcept { return m_currentSound; }
    [[nodiscard]] int volume() const noexcept { return m_volume; }
    void togglePlayPause();

    // Sleep timer control
    void setSleepTimer(int minutes);
    [[nodiscard]] int sleepTimerMinutes() const noexcept { return m_sleepTimerMinutes; }
    [[nodiscard]] int remainingSleepSeconds() const noexcept { return m_remainingSleepSeconds; }
    void cancelSleepTimer() { setSleepTimer(0); }

    // Page flip sound option
    [[nodiscard]] bool isPageFlipSoundEnabled() const noexcept { return m_pageFlipSoundEnabled; }
    void setPageFlipSoundEnabled(bool enabled);

    // Audio backend detection
    [[nodiscard]] QString audioBackend() const noexcept { return m_audioBackend; }

    // Static procedural generation helpers
    static QByteArray generateProceduralPcm(SoundType type, int sampleCount, int volume, ProceduralAudioState& state);
    static QByteArray generateWav(SoundType type, double durationSeconds, int volume = DefaultVolume);
    static QByteArray generatePageFlipWav(int volume = DefaultVolume);
    static QByteArray buildWavHeader(int pcmDataSize, int sampleRate = SampleRate, int channels = Channels, int bitsPerSample = BitsPerSample);

    // Sound metadata helpers
    static QString soundTypeName(SoundType type);
    static QString soundTypeIcon(SoundType type);
    static QString soundTypeDescription(SoundType type);
    static QList<SoundType> availableAmbientSounds();

signals:
    void playbackStateChanged(bool isPlaying);
    void soundTypeChanged(SoundType type);
    void volumeChanged(int volume);
    void sleepTimerChanged(int minutes);
    void sleepTimerTick(int remainingSeconds);
    void sleepTimerExpired();
    void pageFlipSoundEnabledChanged(bool enabled);

private slots:
    void onStreamTimerTimeout();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onSleepTimerTick();

private:
    void detectBackend();
    void startPlaybackProcess();
    void stopPlaybackProcess();
    void writeNextAudioChunk();
    void preparePageFlipCache();
    void loadSettings();
    void saveSettings();

    bool m_isPlaying = false;
    SoundType m_currentSound = SoundType::Rainfall;
    int m_volume = DefaultVolume;
    int m_sleepTimerMinutes = 0;
    int m_remainingSleepSeconds = 0;
    bool m_pageFlipSoundEnabled = false;

    ProceduralAudioState m_state;

    QProcess* m_audioProcess = nullptr;
    QProcess* m_pageFlipProcess = nullptr;
    QTimer* m_streamTimer = nullptr;
    QTimer* m_sleepTimer = nullptr;

    QString m_audioBackend;
    QString m_pageFlipTempPath;
    std::unique_ptr<QTemporaryFile> m_tempLoopWavFile;
};

} // namespace LunaRead
