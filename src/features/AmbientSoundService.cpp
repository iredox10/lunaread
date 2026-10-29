#include "AmbientSoundService.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QSettings>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace LunaRead {

namespace {

constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = 2.0f * Pi;

inline float clampSample(float val) {
    return std::clamp(val, -32768.0f, 32767.0f);
}

inline void appendLittleEndian16(QByteArray& buffer, int16_t sample) {
    buffer.append(static_cast<char>(sample & 0xFF));
    buffer.append(static_cast<char>((sample >> 8) & 0xFF));
}

} // namespace

void ProceduralAudioState::reset() {
    p_b0 = p_b1 = p_b2 = p_b3 = p_b4 = p_b5 = p_b6 = 0.0f;
    rain_lp = 0.0f;
    for (auto& d : droplets) {
        d.freq = 0.0f;
        d.amp = 0.0f;
        d.decay = 0.0f;
        d.phase = 0.0f;
        d.active = false;
    }
    stream_lp1 = stream_lp2 = stream_lfo1 = stream_lfo2 = wind_lfo = 0.0f;
    ocean_phase = 0.0f;
    ocean_rumble_lp = 0.0f;
    ocean_foam_hp = 0.0f;
    cafe_rumble = 0.0f;
    cafe_clink_amp = cafe_clink_phase = cafe_clink_freq = 0.0f;
}

AmbientSoundService& AmbientSoundService::instance() {
    static AmbientSoundService s_instance;
    return s_instance;
}

AmbientSoundService::AmbientSoundService(QObject* parent)
    : QObject(parent),
      m_audioProcess(new QProcess(this)),
      m_pageFlipProcess(new QProcess(this)),
      m_streamTimer(new QTimer(this)),
      m_sleepTimer(new QTimer(this)) {

    m_state.reset();
    detectBackend();
    loadSettings();

    // Stream timer generates audio buffer chunks periodically
    m_streamTimer->setInterval(35); // ~35ms intervals
    connect(m_streamTimer, &QTimer::timeout, this, &AmbientSoundService::onStreamTimerTimeout);

    // Reconnection / restart handling
    connect(m_audioProcess, &QProcess::finished, this, &AmbientSoundService::onProcessFinished);

    // Sleep timer countdown
    m_sleepTimer->setInterval(1000); // 1 second intervals
    connect(m_sleepTimer, &QTimer::timeout, this, &AmbientSoundService::onSleepTimerTick);

    preparePageFlipCache();
}

AmbientSoundService::~AmbientSoundService() {
    stop();
    if (m_pageFlipProcess && m_pageFlipProcess->state() != QProcess::NotRunning) {
        m_pageFlipProcess->kill();
        m_pageFlipProcess->waitForFinished(100);
    }
}

void AmbientSoundService::detectBackend() {
    const QStringList candidates = {
        QStringLiteral("/usr/bin/pw-play"),
        QStringLiteral("/usr/bin/aplay"),
        QStringLiteral("/usr/bin/paplay"),
        QStandardPaths::findExecutable(QStringLiteral("pw-play")),
        QStandardPaths::findExecutable(QStringLiteral("aplay")),
        QStandardPaths::findExecutable(QStringLiteral("paplay"))
    };

    for (const auto& path : candidates) {
        if (!path.isEmpty() && QFileInfo::exists(path) && QFileInfo(path).isExecutable()) {
            m_audioBackend = path;
            break;
        }
    }
}

void AmbientSoundService::loadSettings() {
    QSettings settings;
    int soundInt = settings.value(QStringLiteral("ambient/sound_type"), static_cast<int>(SoundType::Rainfall)).toInt();
    if (soundInt >= 0 && soundInt <= static_cast<int>(SoundType::PageRustle)) {
        m_currentSound = static_cast<SoundType>(soundInt);
    }
    m_volume = std::clamp(settings.value(QStringLiteral("ambient/volume"), DefaultVolume).toInt(), 0, 100);
    m_pageFlipSoundEnabled = settings.value(QStringLiteral("ambient/page_flip_sound"), false).toBool();
}

void AmbientSoundService::saveSettings() {
    QSettings settings;
    settings.setValue(QStringLiteral("ambient/sound_type"), static_cast<int>(m_currentSound));
    settings.setValue(QStringLiteral("ambient/volume"), m_volume);
    settings.setValue(QStringLiteral("ambient/page_flip_sound"), m_pageFlipSoundEnabled);
}

void AmbientSoundService::preparePageFlipCache() {
    m_pageFlipTempPath = QDir::tempPath() + QStringLiteral("/lunaread_pageflip_80ms.wav");
    if (!QFileInfo::exists(m_pageFlipTempPath)) {
        QByteArray wav = generatePageFlipWav(85);
        QFile file(m_pageFlipTempPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(wav);
            file.close();
        }
    }
}

void AmbientSoundService::play(SoundType type, int volume) {
    const bool soundChanged = (m_currentSound != type);
    m_currentSound = type;
    m_volume = std::clamp(volume, 0, 100);

    saveSettings();

    if (soundChanged) {
        m_state.reset();
        emit soundTypeChanged(m_currentSound);
    }

    if (type == SoundType::PageRustle) {
        playPageFlipSound();
        return;
    }

    if (!m_isPlaying) {
        m_isPlaying = true;
        startPlaybackProcess();
        emit playbackStateChanged(true);
    } else {
        // If already playing via stdin streaming, seamlessly adjust synthesis to the new sound
        if (m_audioBackend.endsWith(QStringLiteral("pw-play")) || m_audioBackend.endsWith(QStringLiteral("aplay"))) {
            writeNextAudioChunk();
        } else {
            // For file-based loop, restart with newly synthesized sound file
            startPlaybackProcess();
        }
    }
}

void AmbientSoundService::stop() {
    if (!m_isPlaying) {
        return;
    }
    m_isPlaying = false;
    m_streamTimer->stop();
    stopPlaybackProcess();
    emit playbackStateChanged(false);
}

void AmbientSoundService::togglePlayPause() {
    if (m_isPlaying) {
        stop();
    } else {
        play(m_currentSound, m_volume);
    }
}

void AmbientSoundService::setVolume(int volume) {
    m_volume = std::clamp(volume, 0, 100);
    saveSettings();
    emit volumeChanged(m_volume);
}

void AmbientSoundService::setSleepTimer(int minutes) {
    m_sleepTimerMinutes = std::max(0, minutes);
    if (m_sleepTimerMinutes > 0) {
        m_remainingSleepSeconds = m_sleepTimerMinutes * 60;
        m_sleepTimer->start();
    } else {
        m_remainingSleepSeconds = 0;
        m_sleepTimer->stop();
    }
    emit sleepTimerChanged(m_sleepTimerMinutes);
    emit sleepTimerTick(m_remainingSleepSeconds);
}

void AmbientSoundService::onSleepTimerTick() {
    if (m_remainingSleepSeconds > 0) {
        --m_remainingSleepSeconds;
        emit sleepTimerTick(m_remainingSleepSeconds);
        if (m_remainingSleepSeconds <= 0) {
            m_sleepTimer->stop();
            m_sleepTimerMinutes = 0;
            emit sleepTimerChanged(0);
            emit sleepTimerExpired();
            stop();
        }
    } else {
        m_sleepTimer->stop();
    }
}

void AmbientSoundService::setPageFlipSoundEnabled(bool enabled) {
    if (m_pageFlipSoundEnabled != enabled) {
        m_pageFlipSoundEnabled = enabled;
        saveSettings();
        emit pageFlipSoundEnabledChanged(enabled);
    }
}

void AmbientSoundService::playPageFlipSound() {
    if (!m_pageFlipSoundEnabled) {
        return;
    }
    if (m_audioBackend.isEmpty()) {
        detectBackend();
    }
    if (m_audioBackend.isEmpty()) {
        return;
    }

    if (!QFileInfo::exists(m_pageFlipTempPath)) {
        preparePageFlipCache();
    }

    if (m_pageFlipProcess->state() != QProcess::NotRunning) {
        m_pageFlipProcess->kill();
        m_pageFlipProcess->waitForFinished(50);
    }

    QStringList args;
    if (m_audioBackend.endsWith(QStringLiteral("aplay"))) {
        args << QStringLiteral("-q") << m_pageFlipTempPath;
    } else {
        args << m_pageFlipTempPath;
    }

    m_pageFlipProcess->start(m_audioBackend, args);
}

void AmbientSoundService::startPlaybackProcess() {
    if (m_audioBackend.isEmpty()) {
        detectBackend();
        if (m_audioBackend.isEmpty()) {
            return;
        }
    }

    stopPlaybackProcess();

    // Prefer raw stdin streaming for pw-play and aplay for seamless zero-gap streaming
    if (m_audioBackend.endsWith(QStringLiteral("pw-play"))) {
        QStringList args = {
            QStringLiteral("--raw"),
            QStringLiteral("--rate"), QString::number(SampleRate),
            QStringLiteral("--channels"), QString::number(Channels),
            QStringLiteral("--format"), QStringLiteral("s16"),
            QStringLiteral("-")
        };
        m_audioProcess->start(m_audioBackend, args);
        if (m_audioProcess->waitForStarted(500)) {
            // Pre-seed buffer with 200ms of audio
            writeNextAudioChunk();
            writeNextAudioChunk();
            m_streamTimer->start();
            return;
        }
    } else if (m_audioBackend.endsWith(QStringLiteral("aplay"))) {
        QStringList args = {
            QStringLiteral("-q"),
            QStringLiteral("-f"), QStringLiteral("S16_LE"),
            QStringLiteral("-r"), QString::number(SampleRate),
            QStringLiteral("-c"), QString::number(Channels),
            QStringLiteral("-")
        };
        m_audioProcess->start(m_audioBackend, args);
        if (m_audioProcess->waitForStarted(500)) {
            // Pre-seed buffer
            writeNextAudioChunk();
            writeNextAudioChunk();
            m_streamTimer->start();
            return;
        }
    }

    // Fallback: Generate a 6-second seamless loop WAV file for paplay or if raw stream fails
    m_tempLoopWavFile = std::make_unique<QTemporaryFile>(QDir::tempPath() + QStringLiteral("/lunaread_ambient_XXXXXX.wav"));
    if (m_tempLoopWavFile->open()) {
        QByteArray wav = generateWav(m_currentSound, 6.0, m_volume);
        m_tempLoopWavFile->write(wav);
        m_tempLoopWavFile->flush();
        m_tempLoopWavFile->close();

        QStringList args;
        if (m_audioBackend.endsWith(QStringLiteral("aplay"))) {
            args << QStringLiteral("-q") << m_tempLoopWavFile->fileName();
        } else {
            args << m_tempLoopWavFile->fileName();
        }
        m_audioProcess->start(m_audioBackend, args);
    }
}

void AmbientSoundService::stopPlaybackProcess() {
    if (m_audioProcess->state() != QProcess::NotRunning) {
        m_audioProcess->terminate();
        if (!m_audioProcess->waitForFinished(200)) {
            m_audioProcess->kill();
            m_audioProcess->waitForFinished(100);
        }
    }
}

void AmbientSoundService::writeNextAudioChunk() {
    if (!m_isPlaying || m_audioProcess->state() != QProcess::Running) {
        return;
    }

    // Write ~80ms chunk (3528 samples = 7056 bytes)
    constexpr int ChunkSamples = 3528;
    QByteArray pcm = generateProceduralPcm(m_currentSound, ChunkSamples, m_volume, m_state);
    m_audioProcess->write(pcm);
}

void AmbientSoundService::onStreamTimerTimeout() {
    if (!m_isPlaying || m_audioProcess->state() != QProcess::Running) {
        return;
    }

    // Keep PipeWire buffer topped up to ~250ms of audio
    constexpr qint64 MaxBufferedBytes = 22050; // ~250ms at 44100Hz 16-bit
    if (m_audioProcess->bytesToWrite() < MaxBufferedBytes) {
        writeNextAudioChunk();
    }
}

void AmbientSoundService::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    // Seamless loop restart when using file playback mode
    if (m_isPlaying) {
        if (m_tempLoopWavFile && QFile::exists(m_tempLoopWavFile->fileName())) {
            QStringList args;
            if (m_audioBackend.endsWith(QStringLiteral("aplay"))) {
                args << QStringLiteral("-q") << m_tempLoopWavFile->fileName();
            } else {
                args << m_tempLoopWavFile->fileName();
            }
            m_audioProcess->start(m_audioBackend, args);
        } else {
            startPlaybackProcess();
        }
    }
}

QByteArray AmbientSoundService::buildWavHeader(int pcmDataSize, int sampleRate, int channels, int bitsPerSample) {
    QByteArray header;
    header.resize(44);
    char* data = header.data();

    const int byteRate = sampleRate * channels * (bitsPerSample / 8);
    const int blockAlign = channels * (bitsPerSample / 8);
    const int chunkSize = 36 + pcmDataSize;

    std::memcpy(data, "RIFF", 4);
    *reinterpret_cast<uint32_t*>(data + 4) = static_cast<uint32_t>(chunkSize);
    std::memcpy(data + 8, "WAVE", 4);

    std::memcpy(data + 12, "fmt ", 4);
    *reinterpret_cast<uint32_t*>(data + 16) = 16; // Subchunk1Size for PCM
    *reinterpret_cast<uint16_t*>(data + 20) = 1;  // AudioFormat (1 = PCM)
    *reinterpret_cast<uint16_t*>(data + 22) = static_cast<uint16_t>(channels);
    *reinterpret_cast<uint32_t*>(data + 24) = static_cast<uint32_t>(sampleRate);
    *reinterpret_cast<uint32_t*>(data + 28) = static_cast<uint32_t>(byteRate);
    *reinterpret_cast<uint16_t*>(data + 32) = static_cast<uint16_t>(blockAlign);
    *reinterpret_cast<uint16_t*>(data + 34) = static_cast<uint16_t>(bitsPerSample);

    std::memcpy(data + 36, "data", 4);
    *reinterpret_cast<uint32_t*>(data + 40) = static_cast<uint32_t>(pcmDataSize);

    return header;
}

QByteArray AmbientSoundService::generateProceduralPcm(SoundType type, int sampleCount, int volume, ProceduralAudioState& state) {
    QByteArray buffer;
    buffer.reserve(sampleCount * 2);

    const float volFactor = (static_cast<float>(volume) / 100.0f);

    switch (type) {
    case SoundType::WhiteNoise: {
        for (int i = 0; i < sampleCount; ++i) {
            float white = state.uniformDist(state.rng);
            float out = white * 32767.0f * volFactor * 0.35f;
            appendLittleEndian16(buffer, static_cast<int16_t>(clampSample(out)));
        }
        break;
    }

    case SoundType::PinkNoise: {
        for (int i = 0; i < sampleCount; ++i) {
            float white = state.uniformDist(state.rng);
            state.p_b0 = 0.99886f * state.p_b0 + white * 0.0555179f;
            state.p_b1 = 0.99332f * state.p_b1 + white * 0.0750759f;
            state.p_b2 = 0.96900f * state.p_b2 + white * 0.1538520f;
            state.p_b3 = 0.86650f * state.p_b3 + white * 0.3104856f;
            state.p_b4 = 0.55000f * state.p_b4 + white * 0.5329522f;
            state.p_b5 = -0.7616f * state.p_b5 - white * 0.0168980f;
            float pink = (state.p_b0 + state.p_b1 + state.p_b2 + state.p_b3 + state.p_b4 + state.p_b5 + state.p_b6 + white * 0.5362f) * 0.11f;
            state.p_b6 = white * 0.115926f;

            float out = pink * 32767.0f * volFactor * 0.55f;
            appendLittleEndian16(buffer, static_cast<int16_t>(clampSample(out)));
        }
        break;
    }

    case SoundType::Rainfall: {
        for (int i = 0; i < sampleCount; ++i) {
            float white = state.uniformDist(state.rng);
            state.p_b0 = 0.99886f * state.p_b0 + white * 0.0555179f;
            state.p_b1 = 0.99332f * state.p_b1 + white * 0.0750759f;
            state.p_b2 = 0.96900f * state.p_b2 + white * 0.1538520f;
            state.p_b3 = 0.86650f * state.p_b3 + white * 0.3104856f;
            state.p_b4 = 0.55000f * state.p_b4 + white * 0.5329522f;
            state.p_b5 = -0.7616f * state.p_b5 - white * 0.0168980f;
            float pink = (state.p_b0 + state.p_b1 + state.p_b2 + state.p_b3 + state.p_b4 + state.p_b5 + state.p_b6 + white * 0.5362f) * 0.11f;
            state.p_b6 = white * 0.115926f;

            // Lowpass filter background drizzle (~1200 Hz)
            state.rain_lp += 0.15f * (pink - state.rain_lp);
            float sample = state.rain_lp * 0.45f;

            // Trigger gentle rain droplets (~90 drops/second)
            if (state.uniformDist(state.rng) > 0.9959f) {
                for (auto& drop : state.droplets) {
                    if (!drop.active || drop.amp < 0.02f) {
                        drop.freq = 1400.0f + (state.uniformDist(state.rng) + 1.0f) * 1100.0f;
                        drop.amp = 0.20f + (state.uniformDist(state.rng) + 1.0f) * 0.30f;
                        drop.decay = 0.9991f;
                        drop.phase = 0.0f;
                        drop.active = true;
                        break;
                    }
                }
            }

            // Resonate active droplets
            for (auto& drop : state.droplets) {
                if (drop.active) {
                    sample += drop.amp * std::sin(drop.phase);
                    drop.phase += (TwoPi * drop.freq) / static_cast<float>(SampleRate);
                    drop.amp *= drop.decay;
                    if (drop.amp < 0.005f) {
                        drop.active = false;
                    }
                }
            }

            float out = sample * 32767.0f * volFactor * 0.6f;
            appendLittleEndian16(buffer, static_cast<int16_t>(clampSample(out)));
        }
        break;
    }

    case SoundType::ForestStream: {
        for (int i = 0; i < sampleCount; ++i) {
            float white = state.uniformDist(state.rng);
            state.p_b0 = 0.99886f * state.p_b0 + white * 0.0555179f;
            state.p_b1 = 0.99332f * state.p_b1 + white * 0.0750759f;
            state.p_b2 = 0.96900f * state.p_b2 + white * 0.1538520f;
            state.p_b3 = 0.86650f * state.p_b3 + white * 0.3104856f;
            state.p_b4 = 0.55000f * state.p_b4 + white * 0.5329522f;
            state.p_b5 = -0.7616f * state.p_b5 - white * 0.0168980f;
            float pink = (state.p_b0 + state.p_b1 + state.p_b2 + state.p_b3 + state.p_b4 + state.p_b5 + state.p_b6 + white * 0.5362f) * 0.11f;
            state.p_b6 = white * 0.115926f;

            state.stream_lfo1 += 0.000045f;
            state.stream_lfo2 += 0.000115f;
            state.wind_lfo += 0.000012f;

            float cut1 = 0.07f + 0.035f * std::sin(state.stream_lfo1 * TwoPi);
            float cut2 = 0.20f + 0.070f * std::sin(state.stream_lfo2 * TwoPi);
            state.stream_lp1 += cut1 * (pink - state.stream_lp1);
            state.stream_lp2 += cut2 * (pink - state.stream_lp2);

            float windSwell = 0.5f + 0.5f * std::sin(state.wind_lfo * TwoPi);
            float sample = (state.stream_lp1 * 0.6f + (state.stream_lp2 - state.stream_lp1) * 0.35f) * (0.75f + 0.25f * windSwell);

            float out = sample * 32767.0f * volFactor * 0.65f;
            appendLittleEndian16(buffer, static_cast<int16_t>(clampSample(out)));
        }
        break;
    }

    case SoundType::OceanWaves: {
        for (int i = 0; i < sampleCount; ++i) {
            float white = state.uniformDist(state.rng);
            state.p_b0 = 0.99886f * state.p_b0 + white * 0.0555179f;
            state.p_b1 = 0.99332f * state.p_b1 + white * 0.0750759f;
            state.p_b2 = 0.96900f * state.p_b2 + white * 0.1538520f;
            state.p_b3 = 0.86650f * state.p_b3 + white * 0.3104856f;
            state.p_b4 = 0.55000f * state.p_b4 + white * 0.5329522f;
            state.p_b5 = -0.7616f * state.p_b5 - white * 0.0168980f;
            float pink = (state.p_b0 + state.p_b1 + state.p_b2 + state.p_b3 + state.p_b4 + state.p_b5 + state.p_b6 + white * 0.5362f) * 0.11f;
            state.p_b6 = white * 0.115926f;

            // 10-second wave swell cycle
            state.ocean_phase += TwoPi / (static_cast<float>(SampleRate) * 10.0f);
            if (state.ocean_phase >= TwoPi) state.ocean_phase -= TwoPi;

            float rawSwell = 0.5f + 0.5f * std::sin(state.ocean_phase);
            float swell = rawSwell * rawSwell * rawSwell;

            // Low rumble (~180 Hz)
            state.ocean_rumble_lp += 0.025f * (pink - state.ocean_rumble_lp);

            // Foam hiss highpass
            float foam = white - state.ocean_foam_hp;
            state.ocean_foam_hp += 0.22f * foam;
            float foamAmp = std::max(0.0f, rawSwell - 0.4f) * 1.6f;

            float sample = state.ocean_rumble_lp * (0.35f + 0.65f * swell) + foam * foamAmp * 0.18f;

            float out = sample * 32767.0f * volFactor * 0.65f;
            appendLittleEndian16(buffer, static_cast<int16_t>(clampSample(out)));
        }
        break;
    }

    case SoundType::CafeAmbience: {
        for (int i = 0; i < sampleCount; ++i) {
            float white = state.uniformDist(state.rng);
            state.p_b0 = 0.99886f * state.p_b0 + white * 0.0555179f;
            state.p_b1 = 0.99332f * state.p_b1 + white * 0.0750759f;
            state.p_b2 = 0.96900f * state.p_b2 + white * 0.1538520f;
            state.p_b3 = 0.86650f * state.p_b3 + white * 0.3104856f;
            state.p_b4 = 0.55000f * state.p_b4 + white * 0.5329522f;
            state.p_b5 = -0.7616f * state.p_b5 - white * 0.0168980f;
            float pink = (state.p_b0 + state.p_b1 + state.p_b2 + state.p_b3 + state.p_b4 + state.p_b5 + state.p_b6 + white * 0.5362f) * 0.11f;
            state.p_b6 = white * 0.115926f;

            // Room murmur lowpass (~350 Hz)
            state.cafe_rumble += 0.045f * (pink - state.cafe_rumble);

            // Occasional ceramic cup/spoon clink (~once every 2.5s)
            if (state.cafe_clink_amp < 0.01f && state.uniformDist(state.rng) > 0.99998f) {
                state.cafe_clink_freq = 2700.0f + (state.uniformDist(state.rng) + 1.0f) * 900.0f;
                state.cafe_clink_amp = 0.32f;
                state.cafe_clink_phase = 0.0f;
            }

            float clink = 0.0f;
            if (state.cafe_clink_amp > 0.005f) {
                clink = state.cafe_clink_amp * std::sin(state.cafe_clink_phase);
                state.cafe_clink_phase += (TwoPi * state.cafe_clink_freq) / static_cast<float>(SampleRate);
                state.cafe_clink_amp *= 0.9993f;
            }

            float sample = state.cafe_rumble * 0.65f + clink * 0.35f;

            float out = sample * 32767.0f * volFactor * 0.65f;
            appendLittleEndian16(buffer, static_cast<int16_t>(clampSample(out)));
        }
        break;
    }

    case SoundType::PageRustle: {
        for (int i = 0; i < sampleCount; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(sampleCount);
            float env = (t < 0.12f) ? (t / 0.12f) : std::exp(-6.5f * (t - 0.12f));
            float flutter = 0.8f + 0.2f * std::sin(TwoPi * 90.0f * (static_cast<float>(i) / static_cast<float>(SampleRate)));
            float white = state.uniformDist(state.rng);
            float s = white * env * flutter * 0.70f;
            float out = s * 32767.0f * volFactor;
            appendLittleEndian16(buffer, static_cast<int16_t>(clampSample(out)));
        }
        break;
    }
    }

    return buffer;
}

QByteArray AmbientSoundService::generateWav(SoundType type, double durationSeconds, int volume) {
    const int sampleCount = static_cast<int>(std::round(durationSeconds * SampleRate));
    ProceduralAudioState state;
    state.reset();

    QByteArray pcm = generateProceduralPcm(type, sampleCount, volume, state);
    QByteArray header = buildWavHeader(pcm.size(), SampleRate, Channels, BitsPerSample);
    return header + pcm;
}

QByteArray AmbientSoundService::generatePageFlipWav(int volume) {
    constexpr double durationSeconds = 0.080; // Crisp 80ms duration
    return generateWav(SoundType::PageRustle, durationSeconds, volume);
}

QString AmbientSoundService::soundTypeName(SoundType type) {
    switch (type) {
    case SoundType::Rainfall:     return tr("Gentle Rain");
    case SoundType::ForestStream: return tr("Forest & Wind");
    case SoundType::CafeAmbience: return tr("Cafe Ambience");
    case SoundType::PinkNoise:    return tr("Pink Noise");
    case SoundType::OceanWaves:   return tr("Ocean Waves");
    case SoundType::WhiteNoise:   return tr("White Noise");
    case SoundType::PageRustle:   return tr("Page Rustle");
    }
    return tr("Unknown");
}

QString AmbientSoundService::soundTypeIcon(SoundType type) {
    switch (type) {
    case SoundType::Rainfall:     return QStringLiteral("🌧️");
    case SoundType::ForestStream: return QStringLiteral("🍃");
    case SoundType::CafeAmbience: return QStringLiteral("☕");
    case SoundType::PinkNoise:    return QStringLiteral("📻");
    case SoundType::OceanWaves:   return QStringLiteral("🌊");
    case SoundType::WhiteNoise:   return QStringLiteral("⚪");
    case SoundType::PageRustle:   return QStringLiteral("📖");
    }
    return QStringLiteral("🎵");
}

QString AmbientSoundService::soundTypeDescription(SoundType type) {
    switch (type) {
    case SoundType::Rainfall:     return tr("Gentle rhythmic drizzle on windowpane");
    case SoundType::ForestStream: return tr("Wind through leaves & gentle brook flow");
    case SoundType::CafeAmbience: return tr("Cozy coffee shop murmur & subtle clinks");
    case SoundType::PinkNoise:    return tr("Deep balanced 1/f noise for intense focus");
    case SoundType::OceanWaves:   return tr("Rhythmic shoreline swells & soothing foam");
    case SoundType::WhiteNoise:   return tr("Pure constant focus noise blocking chatter");
    case SoundType::PageRustle:   return tr("Crisp physical paper turn sound effect");
    }
    return QString();
}

QList<SoundType> AmbientSoundService::availableAmbientSounds() {
    return {
        SoundType::Rainfall,
        SoundType::ForestStream,
        SoundType::CafeAmbience,
        SoundType::PinkNoise,
        SoundType::OceanWaves
    };
}

} // namespace LunaRead
