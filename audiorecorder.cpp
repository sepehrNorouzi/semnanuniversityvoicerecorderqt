#include "audiorecorder.h"
#include "audiolevel.h"

#include "ui_audiorecorder.h"

#include <QAudioProbe>
#include <QAudioRecorder>
#include <QDir>
#include <QFileDialog>
#include <QMediaRecorder>
#include <QStandardPaths>
#include <qdebug.h>
#include <QMessageBox>

static qreal getPeakValue(const QAudioFormat &format);
static QVector<qreal> getBufferLevels(const QAudioBuffer &buffer);

template <class T>
static QVector<qreal> getBufferLevels(const T *buffer, int frames, int channels);

QString baseDIR = "";



AudioRecorder::AudioRecorder()
    : ui(new Ui::AudioRecorder)
{
    ui->setupUi(this);
    setFixedSize(QSize(600,400));
    m_audioRecorder = new QAudioRecorder(this);
    m_probe = new QAudioProbe(this);
    connect(m_probe, &QAudioProbe::audioBufferProbed,
            this, &AudioRecorder::processBuffer);
    m_probe->setSource(m_audioRecorder);

    //audio devices
    ui->audioDeviceBox->addItem(tr("Default"), QVariant(QString()));
    for (auto &device: m_audioRecorder->audioInputs()) {
        ui->audioDeviceBox->addItem(device, QVariant(device));
    }

    //groupCB:
    ui->groupCB->addItem(QStringLiteral("Dr.Kiani - Computer Graphics")    , QVariant(0));
    ui->groupCB->addItem(QStringLiteral("Dr.Kiani - IT Projects Managment"), QVariant(1));
    ui->groupCB->addItem(QStringLiteral("Dr.Rastgoo - MATLAB Lab")         , QVariant(2));

    //rangeCB:
    ui->rangeCB->addItem(QStringLiteral("0 - 199")   , QVariant(0));
    ui->rangeCB->addItem(QStringLiteral("200 - 399"), QVariant(1));
    ui->rangeCB->addItem(QStringLiteral("400 - 599"), QVariant(2));
    ui->rangeCB->addItem(QStringLiteral("600 - 799"), QVariant(3));

    //countSPB:
    ui->curCntSPB->setRange(1, 10);

    connect(m_audioRecorder, &QAudioRecorder::durationChanged, this, &AudioRecorder::updateProgress);
    connect(m_audioRecorder, &QAudioRecorder::statusChanged, this, &AudioRecorder::updateStatus);
    connect(m_audioRecorder, &QAudioRecorder::stateChanged, this, &AudioRecorder::onStateChanged);
    connect(m_audioRecorder, QOverload<QMediaRecorder::Error>::of(&QAudioRecorder::error), this,
            &AudioRecorder::displayErrorMessage);
    connect(ui->rangeCB, SIGNAL(currentIndexChanged(int)), this, SLOT(onRangeChange(int)));

}

bool AudioRecorder::isFormComplete()
{
    return ui->idSPB->value() != 0 && ui->nameLE->text() != "";
}

void AudioRecorder::updateProgress(qint64 duration)
{
    if (m_audioRecorder->error() != QMediaRecorder::NoError || duration < 100)
        return;

    ui->statusbar->showMessage(tr("Recorded %1 sec").arg(duration / 1000));
}

void AudioRecorder::updateStatus(QMediaRecorder::Status status)
{
    QString statusMessage;

    switch (status) {
    case QMediaRecorder::RecordingStatus:
        statusMessage = tr("Recording to %1").arg(m_audioRecorder->actualLocation().toString());
        break;
    case QMediaRecorder::PausedStatus:
        clearAudioLevels();
        statusMessage = tr("Paused");
        break;
    case QMediaRecorder::UnloadedStatus:
    case QMediaRecorder::LoadedStatus:
        clearAudioLevels();
        statusMessage = tr("Stopped");
    default:
        break;
    }

    if (m_audioRecorder->error() == QMediaRecorder::NoError)
        ui->statusbar->showMessage(statusMessage);
}

void AudioRecorder::onStateChanged(QMediaRecorder::State state)
{
    switch (state) {
    case QMediaRecorder::RecordingState:
        ui->recordButton->setText(tr("Stop"));
        ui->pauseButton->setText(tr("Pause"));
        break;
    case QMediaRecorder::PausedState:
        ui->recordButton->setText(tr("Stop"));
        ui->pauseButton->setText(tr("Resume"));
        break;
    case QMediaRecorder::StoppedState:
        ui->recordButton->setText(tr("Record"));
        ui->pauseButton->setText(tr("Pause"));
        break;
    }

    ui->pauseButton->setEnabled(m_audioRecorder->state() != QMediaRecorder::StoppedState);
}

static QVariant boxValue(const QComboBox *box)
{
    int idx = box->currentIndex();
    if (idx == -1)
        return QVariant();

    return box->itemData(idx);
}

void AudioRecorder::toggleRecord()
{
    if(!isFormComplete()) {
        ui->statusbar->showMessage("Please complete the form first!!");
        return;
    }
    if (m_audioRecorder->state() == QMediaRecorder::StoppedState) {
        m_audioRecorder->setAudioInput(boxValue(ui->audioDeviceBox).toString());

        QAudioEncoderSettings settings;
        settings.setCodec("audio/pcm");
        settings.setSampleRate(48000);
        settings.setBitRate(32000);

        QString container = "audio/x-wav";

        m_audioRecorder->setEncodingSettings(settings, QVideoEncoderSettings(), container);
        setOutputLocation();
        m_audioRecorder->record();

    }
    else {
        if(baseDIR != "") {
            m_audioRecorder->stop();
            qDebug() << "Here " << ui->curCntSPB->value() << " " << ui->curNumSPB->value();
            int curCTN = ui->curCntSPB->value();
            int curNum = ui->curNumSPB->value();
            curCTN++;
            if(curCTN > 10) {
                curCTN = 1;
                curNum++;
                if(curNum > ui->curNumSPB->maximum()) {
                    QMessageBox::information(this, "Success", "You have finished this range successfuly");
                    return;
                }
            }
            ui->curCntSPB->setValue(curCTN);
            ui->curNumSPB->setValue(curNum);
        }
        else {
            setOutputLocation();
        }
    }
}

void AudioRecorder::togglePause()
{
    if (m_audioRecorder->state() != QMediaRecorder::PausedState)
        m_audioRecorder->pause();
    else
        m_audioRecorder->record();
}

void AudioRecorder::setOutputLocation()
{
    if(baseDIR == "") {
#ifdef Q_OS_WINRT
        // UWP does not allow to store outside the sandbox
        const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (!QDir().mkpath(cacheDir)) {
            qWarning() << "Failed to create cache directory";
            return;
        }
        QString fileName = cacheDir + QLatin1String("/output.wav");
#else
        baseDIR = QFileDialog::getExistingDirectory();
#endif
    }
    int curNumber    = ui->curNumSPB->value();
    int curCount     = ui->curCntSPB->value();
    int id       = ui->idSPB->value();
    QString name = ui->nameLE->text();
    const QString m_dir = baseDIR + '\\' + name + "'s Semnan Recordings" + '\\' + QString::number(curNumber) + '\\';
    QString saveFile = QDir().fromNativeSeparators(m_dir);
    qDebug() << saveFile;
    QDir dir(saveFile);
    if(!dir.exists()) {
        if(!dir.mkpath(saveFile)) {
            QMessageBox::warning(this, "Couldn't access file system", "The app couldn't access your file system please open the app using admin priviliges.");
            exit(0);
        };
    }
    qDebug() << saveFile + QString::number(id)+ '_' + QString::number(curNumber) + "_" + QString::number(curCount) + ".wav";
    m_audioRecorder->setOutputLocation(QUrl::fromLocalFile(saveFile + QString::number(id)+ '_' + QString::number(curNumber) + "_" + QString::number(curCount) + ".wav"));
    m_audioRecorder->setObjectName(saveFile);
    m_outputLocationSet = true;

}

void AudioRecorder::displayErrorMessage()
{
    ui->statusbar->showMessage(m_audioRecorder->errorString());
}

void AudioRecorder::onRangeChange(int state)
{
    switch (state) {
    case 0:
        ui->curNumSPB->setRange(0, 199);
        ui->curNumSPB->setValue(0);
        break;
    case 1:
        ui->curNumSPB->setRange(200, 399);
        ui->curNumSPB->setValue(200);
        break;
    case 2:
        ui->curNumSPB->setRange(400, 599);
        ui->curNumSPB->setValue(400);
        break;
    case 3:
        ui->curNumSPB->setRange(600, 799);
        ui->curNumSPB->setValue(600);
        break;
    default:
        ui->curNumSPB->setRange(0, 199);
        ui->curNumSPB->setValue(0);
        break;
    }

}

void AudioRecorder::clearAudioLevels()
{
    for (int i = 0; i < m_audioLevels.size(); ++i)
        m_audioLevels.at(i)->setLevel(0);
}

// This function returns the maximum possible sample value for a given audio format
qreal getPeakValue(const QAudioFormat& format)
{

    if (!format.isValid())
        return qreal(0);

    if (format.codec() != "audio/pcm")
        return qreal(0);

    switch (format.sampleType()) {
    case QAudioFormat::Unknown:
        break;
    case QAudioFormat::Float:
        if (format.sampleSize() != 32)
            return qreal(0);
        return qreal(1.00003);
    case QAudioFormat::SignedInt:
        if (format.sampleSize() == 32)
            return qreal(INT_MAX);
        if (format.sampleSize() == 16)
            return qreal(SHRT_MAX);
        if (format.sampleSize() == 8)
            return qreal(CHAR_MAX);
        break;
    case QAudioFormat::UnSignedInt:
        if (format.sampleSize() == 32)
            return qreal(UINT_MAX);
        if (format.sampleSize() == 16)
            return qreal(USHRT_MAX);
        if (format.sampleSize() == 8)
            return qreal(UCHAR_MAX);
        break;
    }

    return qreal(0);
}

// returns the audio level for each channel
QVector<qreal> getBufferLevels(const QAudioBuffer& buffer)
{
    QVector<qreal> values;

    if (!buffer.format().isValid() || buffer.format().byteOrder() != QAudioFormat::LittleEndian)
        return values;

    if (buffer.format().codec() != "audio/pcm")
        return values;

    int channelCount = buffer.format().channelCount();
    values.fill(0, channelCount);
    qreal peak_value = getPeakValue(buffer.format());
    if (qFuzzyCompare(peak_value, qreal(0)))
        return values;

    switch (buffer.format().sampleType()) {
    case QAudioFormat::Unknown:
    case QAudioFormat::UnSignedInt:
        if (buffer.format().sampleSize() == 32)
            values = getBufferLevels(buffer.constData<quint32>(), buffer.frameCount(), channelCount);
        if (buffer.format().sampleSize() == 16)
            values = getBufferLevels(buffer.constData<quint16>(), buffer.frameCount(), channelCount);
        if (buffer.format().sampleSize() == 8)
            values = getBufferLevels(buffer.constData<quint8>(), buffer.frameCount(), channelCount);
        for (int i = 0; i < values.size(); ++i)
            values[i] = qAbs(values.at(i) - peak_value / 2) / (peak_value / 2);
        break;
    case QAudioFormat::Float:
        if (buffer.format().sampleSize() == 32) {
            values = getBufferLevels(buffer.constData<float>(), buffer.frameCount(), channelCount);
            for (int i = 0; i < values.size(); ++i)
                values[i] /= peak_value;
        }
        break;
    case QAudioFormat::SignedInt:
        if (buffer.format().sampleSize() == 32)
            values = getBufferLevels(buffer.constData<qint32>(), buffer.frameCount(), channelCount);
        if (buffer.format().sampleSize() == 16)
            values = getBufferLevels(buffer.constData<qint16>(), buffer.frameCount(), channelCount);
        if (buffer.format().sampleSize() == 8)
            values = getBufferLevels(buffer.constData<qint8>(), buffer.frameCount(), channelCount);
        for (int i = 0; i < values.size(); ++i)
            values[i] /= peak_value;
        break;
    }

    return values;
}

template <class T>
QVector<qreal> getBufferLevels(const T *buffer, int frames, int channels)
{
    QVector<qreal> max_values;
    max_values.fill(0, channels);

    for (int i = 0; i < frames; ++i) {
        for (int j = 0; j < channels; ++j) {
            qreal value = qAbs(qreal(buffer[i * channels + j]));
            if (value > max_values.at(j))
                max_values.replace(j, value);
        }
    }

    return max_values;
}

void AudioRecorder::processBuffer(const QAudioBuffer& buffer)
{
    if (m_audioLevels.count() != buffer.format().channelCount()) {
        qDeleteAll(m_audioLevels);
        m_audioLevels.clear();
        for (int i = 0; i < buffer.format().channelCount(); ++i) {
            AudioLevel *level = new AudioLevel(ui->centralwidget);
            m_audioLevels.append(level);
            ui->levelsLayout->addWidget(level);
        }
    }

    QVector<qreal> levels = getBufferLevels(buffer);
    for (int i = 0; i < levels.count(); ++i)
        m_audioLevels.at(i)->setLevel(levels.at(i));
}
